#include "database.h"

namespace database {

NoRowFoundException::NoRowFoundException() : runtime_error("no row found")
{}

DuplicateUniqueValueException::DuplicateUniqueValueException() : runtime_error("tried inserting duplicate unique value")
{}

void prepareConnection(pqxx::connection **dbConn) {
    std::cout << "connecting to database..." << std::endl;
    (*dbConn) = new pqxx::connection("dbname=netvend user=netvend password=badpass");
    std::cout << "connected." << std::endl;
    
    std::cout << "peparing queries..." << std::endl;
    
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
    
    (*dbConn)->prepare(INSERT_FILE, "INSERT INTO files (owner, name, pocket) VALUES ($1, $2, $3) RETURNING file_id");
    (*dbConn)->prepare(FETCH_FILE_OWNER, "SELECT owner FROM files WHERE file_id = $1");
    (*dbConn)->prepare(UPDATE_FILE_BY_ID, "UPDATE files SET data = $2 WHERE file_id = $1");
    (*dbConn)->prepare(READ_FILE_BY_ID, "SELECT data FROM files WHERE file_id = $1");
    
    std::cout << "queries prepared." << std::endl;
}

bool agentRowExists(pqxx::connection *dbConn, std::string agentAddress) {
    pqxx::result result;
    
    pqxx::work tx(*dbConn, "CheckAgentExistsWork");
    result = tx.prepared(CHECK_AGENT_EXISTS)(agentAddress).exec();
    tx.commit();
    
    bool exists;
    result[0][0].to(exists);
    
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
        throw e;// DuplicateUniqueValueException();
    }
}

crypto::RSAPubkey fetchAgentPubkey(pqxx::connection *dbConn, std::string agentAddress) {
    pqxx::work tx(*dbConn, "FetchAgentPubkeyWork");
    pqxx::result result = tx.prepared(FETCH_AGENT_PUBKEY)(agentAddress).exec();
    tx.commit();
    
    if (result.size() == 0) {
        throw NoRowFoundException();
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
        
        tx.commit();
    }
    catch (pqxx::unique_violation& e) {
        throw e;
    }
    
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
        throw NoRowFoundException();
    }
    
    std::string owner;
    result[0][0].to(owner);
    return owner;
}

void updatePocketOwner(pqxx::connection *dbConn, unsigned long pocketID, std::string newOwnerAddress) {
    pqxx::work tx(*dbConn, "UpdatePocketOwnerWork");
    
    tx.prepared(UPDATE_POCKET_OWNER)(pocketID)(newOwnerAddress).exec();
    
    tx.commit();
}

void updatePocketDepositAddress(pqxx::connection *dbConn, std::string ownerAddress, unsigned long pocketID, std::string newDepositAddress) {
    pqxx::work tx(*dbConn, "UpdatePocketDepositAddressWork");
    pqxx::result result;
    
    result = tx.prepared(UPDATE_POCKET_DEPOSIT_ADDRESS)(ownerAddress)(pocketID)(newDepositAddress).exec();
    
    tx.commit();
    
    if (result.affected_rows() == 0) {
        throw NoRowFoundException();
    }
}



unsigned long insertFile(pqxx::connection *dbConn, std::string ownerAddress, std::string name, unsigned long pocketID) {
    pqxx::work tx(*dbConn, "InsertFileWork");
    pqxx::result result;
    
    result = tx.prepared(INSERT_FILE)(ownerAddress)(name)(pocketID).exec();
    
    tx.commit();
    
    unsigned long chunkID;
    result[0][0].to(chunkID);
    return chunkID;
}

std::string fetchFileOwner(pqxx::connection *dbConn, unsigned long chunkID) {
    pqxx::work tx(*dbConn, "FetchFileOwnerWork");
    
    pqxx::result result = tx.prepared(FETCH_FILE_OWNER)(chunkID).exec();
    
    if (result.size() == 0) {
        throw NoRowFoundException();
    }
    
    std::string owner;
    result[0][0].to(owner);
    return owner;
}

void updateFileByID(pqxx::connection *dbConn, unsigned long chunkID, unsigned char* data, unsigned short dataSize) {
    pqxx::work tx(*dbConn, "UpdateFileByIDWork");
    pqxx::result result;
    
    pqxx::binarystring dataBlob(data, dataSize);
    
    result = tx.prepared(UPDATE_FILE_BY_ID)(chunkID)(dataBlob).exec();
    
    tx.commit();
    
    if (result.affected_rows() == 0) {
        throw NoRowFoundException();
    }
}

std::vector<unsigned char> readFileByID(pqxx::connection *dbConn, unsigned long chunkID) {
    pqxx::work tx(*dbConn, "ReadFileByIDWork");
    pqxx::result result = tx.prepared(READ_FILE_BY_ID)(chunkID).exec();
    tx.commit();
    
    if (result.size() == 0) {
        throw NoRowFoundException();
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
        std::cout << "running delete files query:" << std::endl << removeFilesQuery << std::endl << std::endl;
        tx.exec(removeFilesQuery);
    }
    //run the deduct query
    if (!chargeListEmpty) {
        std::cout << "running charge pockets query:" << std::endl << deductPocketsQuery << std::endl << std::endl;
        tx.exec(deductPocketsQuery);
    }
    
    tx.commit();
}

}//namespace database