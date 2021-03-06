#include "database.h"

namespace database {

NoRowFoundException::NoRowFoundException() : runtime_error("no row found")
{}

DuplicateUniqueValueException::DuplicateUniqueValueException() : runtime_error("tried inserting duplicate unique value")
{}

void prepareConnection(pqxx::connection **dbConn) {
    //std::cout << "connecting to database..." << std::endl;
    (*dbConn) = new pqxx::connection("dbname=netvend user=netvend password=badpass");
    //std::cout << "connected." << std::endl;
    
    //std::cout << "peparing queries..." << std::endl;
    
    (*dbConn)->prepare(FETCH_FILE_FEES_SUPPORTED_PER_POCKET, "SELECT "
                                                                "sub.pocket AS pocket_id, "
                                                                "sub.total_fees AS total_fees, "
                                                                "(pockets.amount - sub.total_fees < 0) AS would_bankrupt "
                                                             "FROM ("
                                                                "SELECT "
                                                                    "pocket, COUNT(*)*$1 + SUM(COALESCE(OCTET_LENGTH(data),0))*$2 AS total_fees "
                                                                "FROM files "
                                                                "GROUP BY pocket "
                                                             ")"
                                                                "AS sub "
                                                             "LEFT JOIN pockets ON sub.pocket = pockets.pocket_id;"
               );
    
    (*dbConn)->prepare(CHECK_AGENT_EXISTS, "SELECT EXISTS(SELECT 1 FROM agents WHERE agent_address = $1)");
    (*dbConn)->prepare(INSERT_AGENT, "INSERT INTO agents (agent_address, public_key, default_pocket) VALUES ($1, $2, $3)");
    (*dbConn)->prepare(FETCH_AGENT_PUBKEY, "SELECT public_key FROM agents WHERE agent_address = $1");
    
    (*dbConn)->prepare(INSERT_POCKET_WITH_DEPOSIT_ADDRESS, "INSERT INTO pockets (owner, deposit_address) VALUES ($1, $2) RETURNING pocket_id");
    (*dbConn)->prepare(INSERT_POCKET, "INSERT INTO pockets (owner) VALUES ($1) RETURNING pocket_id");
    (*dbConn)->prepare(INSERT_POCKET_WITHOUT_OWNER, "INSERT INTO pockets (owner) VALUES (NULL) RETURNING pocket_id");
    (*dbConn)->prepare(FETCH_POCKET_OWNER, "SELECT owner FROM pockets WHERE pocket_id = $1"); 
    (*dbConn)->prepare(UPDATE_POCKET_OWNER, "UPDATE pockets SET owner = $2 WHERE pocket_id = $1");
    (*dbConn)->prepare(UPDATE_POCKET_DEPOSIT_ADDRESS, "UPDATE pockets SET deposit_address = $3 WHERE owner = $1 AND pocket_id = $2");
    (*dbConn)->prepare(FETCH_POCKET_BALANCE, "SELECT amount FROM pockets WHERE pocket_id = $1");
    (*dbConn)->prepare(DEDUCT_FROM_POCKET_WITH_OWNER, "UPDATE pockets SET amount = amount - $3 WHERE owner = $2 AND pocket_id = $1 AND amount - $3 >= 0");
    (*dbConn)->prepare(ADD_TO_POCKET, "UPDATE pockets SET amount = amount + $2 WHERE pocket_id = $1");
    
    (*dbConn)->prepare(INSERT_FILE, "INSERT INTO files (owner, name, pocket) VALUES ($1, $2, $3) RETURNING file_id");
    (*dbConn)->prepare(FETCH_FILE_OWNER, "SELECT owner FROM files WHERE file_id = $1");
    (*dbConn)->prepare(UPDATE_FILE_BY_ID, "UPDATE files SET data = $2 WHERE file_id = $1");
    (*dbConn)->prepare(READ_FILE_BY_ID, "SELECT data FROM files WHERE file_id = $1");
    
    //std::cout << "queries prepared." << std::endl;
}

bool agentRowExists(pqxx::connection *dbConn, std::string agentAddress) {
    pqxx::result result;
    
    pqxx::work tx(*dbConn, "CheckAgentExistsWork");
    result = tx.prepared(CHECK_AGENT_EXISTS)(agentAddress).exec();
    tx.commit();
    
    bool exists; result[0][0].to(exists);
    
    return exists;
}

void insertAgent(pqxx::connection *dbConn, std::string agentAddress, std::vector<unsigned char> DEREncodedPubkey, unsigned long defaultPocketID) {
    pqxx::work tx(*dbConn, "InsertAgentWork");
    
    try {
        pqxx::binarystring pubkeyBlob(DEREncodedPubkey.data(), DEREncodedPubkey.size());
        
        tx.prepared(INSERT_AGENT)(agentAddress)(pubkeyBlob)(defaultPocketID).exec();
        
        tx.commit();
    }
    catch (pqxx::unique_violation& e) {
        commands::errors::Error* error = new commands::errors::ServerLogicError("Agent with that address already exists", 0, true);
        throw NetvendCommandException(error);
    }
}

crypto::RSAPubkey fetchAgentPubkey(pqxx::connection *dbConn, std::string agentAddress) {
    pqxx::work tx(*dbConn, "FetchAgentPubkeyWork");
    pqxx::result result = tx.prepared(FETCH_AGENT_PUBKEY)(agentAddress).exec();
    tx.commit();
    
    if (result.size() == 0) {
        commands::errors::Error* error = new commands::errors::InvalidTargetError(std::string("agent ") + agentAddress, 0, true);
        throw NetvendCommandException(error);
    }
    pqxx::binarystring pubkeyBlob(result[0][0]);
    
    return crypto::decodePubkey(pubkeyBlob.data(), pubkeyBlob.size());
}



unsigned long insertPocket(pqxx::connection *dbConn, std::string ownerAddress, std::string depositAddress) {
    pqxx::work tx(*dbConn, "InsertPocketWork");
    pqxx::result result;
    
    try {
        if (depositAddress != "") {
            result = tx.prepared(INSERT_POCKET_WITH_DEPOSIT_ADDRESS)(ownerAddress)(depositAddress).exec();
        }
        else if (ownerAddress != "") {
            result = tx.prepared(INSERT_POCKET)(ownerAddress).exec();
        }
        else {
            result = tx.prepared(INSERT_POCKET_WITHOUT_OWNER).exec();
        }
    }
    catch (pqxx::unique_violation& e) {
        commands::errors::Error* error = new commands::errors::ServerLogicError("Pocket with that id already exists", 0, true);
        throw NetvendCommandException(error);
    }
    
    tx.commit();
    
    unsigned long pocketID;
    result[0][0].to(pocketID);
    return pocketID;
}

unsigned long insertPocket(pqxx::connection *dbConn, std::string ownerAddress) {
    return insertPocket(dbConn, ownerAddress, "");
}

unsigned long insertPocket(pqxx::connection *dbConn) {
    return insertPocket(dbConn, "", "");
}

std::string fetchPocketOwner(pqxx::connection *dbConn, unsigned long pocketID) {
    pqxx::work tx(*dbConn, "FetchPocketOwnerWork");
    
    pqxx::result result = tx.prepared(FETCH_POCKET_OWNER)(pocketID).exec();
    
    if (result.size() == 0) {
        commands::errors::Error* error = new commands::errors::InvalidTargetError(std::string("p:") + boost::lexical_cast<std::string>(pocketID), 0, true);
        throw NetvendCommandException(error);
    }
    
    std::string owner; result[0][0].to(owner);
    return owner;
}

void verifyPocketOwner(pqxx::connection *dbConn, unsigned long pocketID, std::string agentAddress) {
    if (fetchPocketOwner(dbConn, pocketID) != agentAddress) {
        commands::errors::Error* error = new commands::errors::TargetNotOwnedError(std::string("p:") + boost::lexical_cast<std::string>(pocketID), 0, true);
        throw NetvendCommandException(error);
    }
}

void updatePocketOwner(pqxx::connection *dbConn, unsigned long pocketID, std::string newOwnerAddress) {
    pqxx::work tx(*dbConn, "UpdatePocketOwnerWork");
    
    pqxx::result result = tx.prepared(UPDATE_POCKET_OWNER)(pocketID)(newOwnerAddress).exec();
    
    tx.commit();
    
    if (result.affected_rows() == 0) {
        commands::errors::Error* error = new commands::errors::InvalidTargetError(std::string("p:") + boost::lexical_cast<std::string>(pocketID), 0, true);
        throw NetvendCommandException(error);
    }
}

void updatePocketDepositAddress(pqxx::connection *dbConn, std::string ownerAddress, unsigned long pocketID, std::string newDepositAddress) {
    pqxx::work tx(*dbConn, "UpdatePocketDepositAddressWork");
    
    pqxx::result result = tx.prepared(UPDATE_POCKET_DEPOSIT_ADDRESS)(ownerAddress)(pocketID)(newDepositAddress).exec();
    
    tx.commit();
    
    if (result.affected_rows() == 0) {
        commands::errors::Error* error = new commands::errors::InvalidTargetError(std::string("p:") + boost::lexical_cast<std::string>(pocketID), 0, true);
        throw NetvendCommandException(error);
    }
}

void pocketTransfer(pqxx::connection *dbConn, std::string fromOwnerAddress, unsigned long fromPocketID, unsigned long toPocketID, unsigned long long amount) {
    pqxx::work tx(*dbConn, "PocketTransferWork");
    pqxx::result result;
    
    //first try to deduct from the sender
    result = tx.prepared(DEDUCT_FROM_POCKET_WITH_OWNER)(fromPocketID)(fromOwnerAddress)(amount).exec();
    
    if (result.affected_rows() == 0) {
        //something went wrong. Lets find out what!
        pqxx::result ownerResult = tx.prepared(FETCH_POCKET_OWNER)(fromPocketID).exec();
        if (ownerResult.size() == 0) {
            //no pocket with this pocket_id.
            commands::errors::Error* error = new commands::errors::InvalidTargetError(std::string("p:") + boost::lexical_cast<std::string>(fromPocketID), 0, true);
            throw NetvendCommandException(error);
        }
        else if (ownerResult[0][0].as<std::string>() != fromOwnerAddress) {
            //Pocket not owned by this agent.
            commands::errors::Error* error = new commands::errors::TargetNotOwnedError(std::string("p:") + boost::lexical_cast<std::string>(fromPocketID), 0, true);
            throw NetvendCommandException(error);
        }
        else {
            //Only possibility left should be that pocket can't support transfer.
            pqxx::result balanceResult = tx.prepared(FETCH_POCKET_BALANCE)(fromPocketID).exec();
            unsigned long long balance = balanceResult[0][0].as<unsigned long long>();
            if (balance - amount < 0) {
                commands::errors::Error* error = new commands::errors::CreditInsufficientError(amount, balance, 0, true);
                throw NetvendCommandException(error);
            }
            else {
                //weird! use ServerLogicError as a catchall.
                commands::errors::Error* error = new commands::errors::ServerLogicError(std::string("Transfer from pocket ") + boost::lexical_cast<std::string>(fromPocketID) + std::string(" failed for an unkown reason"), 0, true);
                throw NetvendCommandException(error);
            }
        }
    }
    
    result = tx.prepared(ADD_TO_POCKET)(toPocketID)(amount).exec();
    
    if (result.affected_rows() == 0) {
        //no pocket with this pocket_id
        commands::errors::Error* error = new commands::errors::InvalidTargetError(std::string("p:") + boost::lexical_cast<std::string>(toPocketID), 0, true);
        throw NetvendCommandException(error);
    }
    
    tx.commit();
}



unsigned long insertFile(pqxx::connection *dbConn, std::string ownerAddress, std::string name, unsigned long pocketID) {
    pqxx::work tx(*dbConn, "InsertFileWork");
    pqxx::result result;
    
    try {
        result = tx.prepared(INSERT_FILE)(ownerAddress)(name)(pocketID).exec();
    }
    catch (pqxx::unique_violation& e) {
        commands::errors::Error* error = new commands::errors::ServerLogicError("File with that owner and name already exists", 0, true);
        throw NetvendCommandException(error);
    }
    
    tx.commit();
    
    unsigned long fileID;
    result[0][0].to(fileID);
    return fileID;
}

std::string fetchFileOwner(pqxx::connection *dbConn, unsigned long fileID) {
    pqxx::work tx(*dbConn, "FetchFileOwnerWork");
    
    pqxx::result result = tx.prepared(FETCH_FILE_OWNER)(fileID).exec();
    
    if (result.size() == 0) {
        commands::errors::Error* error = new commands::errors::InvalidTargetError(std::string("f:") + boost::lexical_cast<std::string>(fileID), 0, true);
        throw NetvendCommandException(error);
    }
    
    std::string owner;
    result[0][0].to(owner);
    return owner;
}

void verifyFileOwner(pqxx::connection *dbConn, unsigned long fileID, std::string agentAddress) {
    if (fetchFileOwner(dbConn, fileID) != agentAddress) {
        commands::errors::Error* error = new commands::errors::TargetNotOwnedError(std::string("f:") + boost::lexical_cast<std::string>(fileID), 0, true);
        throw NetvendCommandException(error);
    }
}

void updateFileByID(pqxx::connection *dbConn, unsigned long fileID, unsigned char* data, unsigned short dataSize) {
    pqxx::work tx(*dbConn, "UpdateFileByIDWork");
    pqxx::result result;
    
    pqxx::binarystring dataBlob(data, dataSize);
    
    result = tx.prepared(UPDATE_FILE_BY_ID)(fileID)(dataBlob).exec();
    
    tx.commit();
    
    if (result.affected_rows() == 0) {
        commands::errors::Error* error = new commands::errors::InvalidTargetError(std::string("f:") + boost::lexical_cast<std::string>(fileID), 0, true);
        throw NetvendCommandException(error);
    }
}

std::vector<unsigned char> readFileByID(pqxx::connection *dbConn, unsigned long fileID) {
    pqxx::work tx(*dbConn, "ReadFileByIDWork");
    pqxx::result result = tx.prepared(READ_FILE_BY_ID)(fileID).exec();
    tx.commit();
    
    if (result.size() == 0) {
        commands::errors::Error* error = new commands::errors::InvalidTargetError(std::string("f:") + boost::lexical_cast<std::string>(fileID), 0, true);
        throw NetvendCommandException(error);
    }
    pqxx::binarystring fileDataBlob(result[0][0]);
    
    std::vector<unsigned char> fileDataVch;
    fileDataVch.resize(fileDataBlob.size());
    std::copy(fileDataBlob.begin(), fileDataBlob.end(), fileDataVch.begin());
    return fileDataVch;
}

void chargeFileUpkeepFees(pqxx::connection *dbConn, int creditPerFile, int creditPerByte) {
    //get total bytes each pocket is responsible for supporting
    pqxx::work fetchFeesTx(*dbConn, "FetchFileFeesSupportedPerPocketWork");
    pqxx::result feesPerPocketResult = fetchFeesTx.prepared(FETCH_FILE_FEES_SUPPORTED_PER_POCKET)(creditPerFile)(creditPerByte).exec();
    fetchFeesTx.commit();
    
    //if a pocket can't support the fees, we delete all the files supported by that pocket
    //construct the delete query
    
    std::string removeFilesQuery = "DELETE FROM files WHERE pocket IN ";
    std::string deductPocketsQuery = "UPDATE pockets SET amount = amount - CASE pocket_id ";
    
    std::string bankruptPocketsList = "(";
    std::string chargeablePocketsList = "(";
    for (int i=0; i<feesPerPocketResult.size(); i++) {
        //for each pocket, delete posts if it has too little credit, otherwise deduct
        bool wouldBankrupt; feesPerPocketResult[i]["would_bankrupt"].to(wouldBankrupt);
        if (wouldBankrupt) {
            bankruptPocketsList.append(feesPerPocketResult[i]["pocket_id"].c_str()).append(",");
        }
        else {
            chargeablePocketsList.append(feesPerPocketResult[i]["pocket_id"].c_str()).append(",");
            deductPocketsQuery.append("WHEN ").append(feesPerPocketResult[i]["pocket_id"].c_str()).append(" THEN ").append(feesPerPocketResult[i]["total_fees"].c_str()).append(" ");
        }
    }
    bool bankruptListEmpty = true;
    if (bankruptPocketsList.size() > 1) {
        bankruptPocketsList.pop_back();//remove trailing comma, end list, and add to query
        bankruptListEmpty = false;
    }
    bool chargeListEmpty = true;
    if (chargeablePocketsList.size() > 1) {
        chargeablePocketsList.pop_back();//remove trailing comma
        chargeListEmpty = false;
    }
    bankruptPocketsList.append(")");
    chargeablePocketsList.append(")");
    
    removeFilesQuery.append(bankruptPocketsList);
    deductPocketsQuery.append("END WHERE pocket_id IN ").append(chargeablePocketsList);
    
    //deleting and deducting will be on the same tx.
    pqxx::work tx(*dbConn, "DeductFeesAndDeleteFilesWork");
    //run the delete query
    if (!bankruptListEmpty) {
        //std::cout << "running delete files query:" << std::endl << removeFilesQuery << std::endl << std::endl;
        tx.exec(removeFilesQuery);
    }
    //run the deduct query
    if (!chargeListEmpty) {
        //std::cout << "running charge pockets query:" << std::endl << deductPocketsQuery << std::endl << std::endl;
        tx.exec(deductPocketsQuery);
    }
    
    tx.commit();
}

}//namespace database