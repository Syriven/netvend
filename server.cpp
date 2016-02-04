#include <iostream>
#include <cryptopp/osrng.h>
#include <cryptopp/rsa.h>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/chrono.hpp>
#include <pqxx/pqxx>

#include "netvend/common_constants.h"
#include "util/database.h"
#include "util/btc.h"
#include "util/crypto.h"
#include "util/networking.h"
#include "netvend/commands.h"
#include "netvend/packet.h"
#include "netvend/response.h"
#include "netvend/exception.h"

using boost::asio::ip::tcp;

pqxx::connection *dbConn;
boost::property_tree::ptree config;

networking::HandshakeResponse processHandshakePacket(boost::shared_ptr<networking::HandshakePacket> packet) {
    crypto::RSAPubkey pubkey = packet->pubkey();
    
    //find agentAddress from pubkey
    std::string agentAddress = crypto::RSAPubkeyToNetvendAddress(pubkey);
    
    //do we already have a record for this agent?
    bool isNewAgent = ! database::agentRowExists(dbConn, agentAddress);
    
    if (isNewAgent) {
        std::cout << "no agent found; inserting." << std::endl;
        
        //first create a pocket.
        unsigned long defaultPocketID = database::insertPocket(dbConn);
        //std::cout << defaultPocketID << std::endl;
        
        //encode the public key
        std::vector<unsigned char> encodedPubkey = crypto::encodePubkey(pubkey);
        
        //insert agent row
        database::insertAgent(dbConn, agentAddress, encodedPubkey, defaultPocketID);
        
        //now that the agent row is inserted, update pocket to reflect owner
        //we had to do this as a second step due to the pocket's foreign_key constraint
        database::updatePocketOwner(dbConn, defaultPocketID, agentAddress);
        
        return networking::HandshakeResponse(true, defaultPocketID);
    }
    else {
        std::cout << "agent found." << std::endl;
        
        return networking::HandshakeResponse(false);
    }
}

boost::shared_ptr<commands::results::CreatePocket> processCreatePocketCommand(std::string agentAddress, boost::shared_ptr<commands::CreatePocket> command) {    
    unsigned int pocketID = database::insertPocket(dbConn, agentAddress);
    
    int cost = 0;
    
    return boost::shared_ptr<commands::results::CreatePocket>(new commands::results::CreatePocket(cost, pocketID));
}

boost::shared_ptr<commands::results::RequestPocketDepositAddress> processRequestPocketDepositAddressCommand(std::string agentAddress, boost::shared_ptr<commands::RequestPocketDepositAddress> command) {
    unsigned long pocketID = command->pocketID();
    
    database::verifyPocketOwner(dbConn, pocketID, agentAddress);
    
    std::string depositAddress = btc::getNewDepositAddress();
    
    database::updatePocketDepositAddress(dbConn, agentAddress, pocketID, depositAddress);
    
    boost::shared_ptr<commands::results::RequestPocketDepositAddress> rpdaResult(
      new commands::results::RequestPocketDepositAddress(0, depositAddress)
    );
    
    return rpdaResult;
}

boost::shared_ptr<commands::results::PocketTransfer> processPocketTransferCommand(std::string agentAddress, boost::shared_ptr<commands::PocketTransfer> command) {
    unsigned long fromPocketID = command->fromPocketID();
    unsigned long toPocketID = command->toPocketID();
    unsigned long long amount = command->amount();
    
    database::pocketTransfer(dbConn, agentAddress, fromPocketID, toPocketID, amount);
    
    boost::shared_ptr<commands::results::PocketTransfer> ptResult(
      new commands::results::PocketTransfer(0)
    );
    
    return ptResult;
}

boost::shared_ptr<commands::results::CreateFile> processCreateFileCommand(std::string agentAddress, boost::shared_ptr<commands::CreateFile> command) {
    unsigned long pocketID = command->pocketID();
    
    database::verifyPocketOwner(dbConn, pocketID, agentAddress);
    
    std::string name = command->name();
    
    unsigned long fileID = database::insertFile(dbConn, agentAddress, name, pocketID);
    
    boost::shared_ptr<commands::results::CreateFile> ccResult(
      new commands::results::CreateFile(0, fileID)
    );
    
    return ccResult;
}

boost::shared_ptr<commands::results::UpdateFileByID> processUpdateFileByIDCommand(std::string agentAddress, boost::shared_ptr<commands::UpdateFileByID> command) {
    unsigned long fileID = command->fileID();
    
    database::verifyFileOwner(dbConn, fileID, agentAddress);
    
    unsigned char* data = command->data();
    unsigned short dataSize = command->dataSize();
    
    database::updateFileByID(dbConn, fileID, data, dataSize);
    
    boost::shared_ptr<commands::results::UpdateFileByID> ucbiResult(
      new commands::results::UpdateFileByID(0)
    );
    
    return ucbiResult;
}

boost::shared_ptr<commands::results::ReadFileByID> processReadFileByIDCommand(std::string agentAddress, boost::shared_ptr<commands::ReadFileByID> command) {
    unsigned long fileID = command->fileID();
    
    std::vector<unsigned char> fileData;
    try {
        fileData = database::readFileByID(dbConn, fileID);
    }
    catch (database::NoRowFoundException &e) {
        commands::errors::Error* error = new commands::errors::InvalidTargetError(boost::lexical_cast<std::string>(fileID), 0, true);
        throw NetvendCommandException(error);
    }
    
    boost::shared_ptr<commands::results::ReadFileByID> readResult(
      new commands::results::ReadFileByID(0, fileData)
    );
    
    return readResult;
}

boost::shared_ptr<commands::results::Result> processCommand(std::string agentAddress, boost::shared_ptr<commands::Command> command) {
    if (command->typeChar() == commands::COMMANDTYPECHAR_CREATE_POCKET) {
        boost::shared_ptr<commands::CreatePocket> cpCommand = 
          boost::dynamic_pointer_cast<commands::CreatePocket>(command);
        
        if (cpCommand.get() == NULL) {
            throw networking::NetvendDecodeException("Error decoding what seems to be a createpocket command.");
        }
        
        return processCreatePocketCommand(agentAddress, cpCommand);
    }
    else if (command->typeChar() == commands::COMMANDTYPECHAR_REQUEST_POCKET_DEPOSIT_ADDRESS) {
        boost::shared_ptr<commands::RequestPocketDepositAddress> rpdaCommand = 
          boost::dynamic_pointer_cast<commands::RequestPocketDepositAddress>(command);
        
        if (rpdaCommand.get() == NULL) {
            throw networking::NetvendDecodeException("Error decoding what seems to be an rpda command.");
        }
        
        return processRequestPocketDepositAddressCommand(agentAddress, rpdaCommand);
    }
    else if (command->typeChar() == commands::COMMANDTYPECHAR_POCKET_TRANSFER) {
        boost::shared_ptr<commands::PocketTransfer> ptCommand =
          boost::dynamic_pointer_cast<commands::PocketTransfer>(command);
        
        if (ptCommand.get() == NULL) {
            throw networking::NetvendDecodeException("Error decoding what seems to be a pocketTransfer command.");
        }
        
        return processPocketTransferCommand(agentAddress, ptCommand);
    }
    else if (command->typeChar() == commands::COMMANDTYPECHAR_CREATE_FILE) {
        boost::shared_ptr<commands::CreateFile> ccCommand = 
          boost::dynamic_pointer_cast<commands::CreateFile>(command);
        
        if (ccCommand.get() == NULL) {
            throw networking::NetvendDecodeException("Error decoding what seems to be a CreateFile command.");
        }
        
        return processCreateFileCommand(agentAddress, ccCommand);
    }
    else if (command->typeChar() == commands::COMMANDTYPECHAR_UPDATE_FILE_BY_ID) {
        boost::shared_ptr<commands::UpdateFileByID> writeCommand = 
        boost::dynamic_pointer_cast<commands::UpdateFileByID>(command);
        
        if (writeCommand.get() == NULL) {
            throw networking::NetvendDecodeException("Error decoding what seems to be an ucbi command.");
        }
        
        return processUpdateFileByIDCommand(agentAddress, writeCommand);
    }
    else if (command->typeChar() == commands::COMMANDTYPECHAR_READ_FILE_BY_ID) {
        boost::shared_ptr<commands::ReadFileByID> readCommand =
        boost::dynamic_pointer_cast<commands::ReadFileByID>(command);
        
        if (readCommand.get() == NULL) {
            throw networking::NetvendDecodeException("Error decoding what seems to be a readFile command.");
        }
        
        return processReadFileByIDCommand(agentAddress, readCommand);
    }
    else {
        throw networking::NetvendDecodeException((std::string("Error decoding command with commandtypechar ") + boost::lexical_cast<std::string>(command->typeChar())).c_str());
    }
    return NULL;
}

networking::CommandBatchResponse processCommandBatchPacket(boost::shared_ptr<networking::CommandBatchPacket> packet) {
    crypto::RSAPubkey pubkey;
    try {
        pubkey = database::fetchAgentPubkey(dbConn, packet->agentAddress());
    }
    catch (database::NoRowFoundException& e) {
        std::cout << "No pubkey for agent; aborting." << std::endl;
        //return;
    }
    
    if (!crypto::verifySig(pubkey, *(packet->commandBatchData()), packet->sig())) {
        std::cout << "signature verification failed." << std::endl;
        //return error
    }
    
    unsigned char* dataPtr = packet->commandBatchData()->data();
    boost::shared_ptr<commands::Batch> cb(commands::Batch::consumeFromBuf(&dataPtr));
    
    std::cout << cb->commands()->size() << " commands in commandBatch." << std::endl;
    
    boost::shared_ptr<commands::results::Batch> crb(new commands::results::Batch(cb.get()));
    
    for (unsigned int i=0; i < cb->commands()->size(); i++) {
        boost::shared_ptr<commands::Command> command = (*(cb->commands()))[i];
        try {
            boost::shared_ptr<commands::results::Result> result = processCommand(packet->agentAddress(), command);
            crb->addResult(result);
        }
        catch (NetvendCommandException &exception) {
            boost::shared_ptr<commands::errors::Error> commandError = exception.commandError();
            std::cout << commandError->error() << std::endl;
            crb->addResult(commandError);
            if (commandError->fatalToBatch()) {
                std::cout << "command " << i << " had fatal error " << commandError->what() << std::endl;
                break;
            }
            else {
                std::cout << "command " << i << " had nonfatal error " << commandError->what() << std::endl;
            }
        }
    }
    
    return networking::CommandBatchResponse(crb, commands::COMMANDBATCH_COMPLETION_ALL);
}

class FeeHandler {
    boost::chrono::process_real_cpu_clock::time_point lastFeeChargedTime;
    pqxx::connection *feeDbConn;
public:
    FeeHandler() {
        database::prepareConnection(&feeDbConn);
    }
    void operator()() {
        chargeFees();
        lastFeeChargedTime = boost::chrono::process_real_cpu_clock::now();
        
        while (true) {
            boost::chrono::process_real_cpu_clock::time_point wakeTime =
                lastFeeChargedTime + boost::chrono::seconds(config.get<int>("fees.fee-interval"));
            boost::this_thread::sleep_until(wakeTime);
            
            lastFeeChargedTime = wakeTime;
            
            chargeFees();
        }
    }
    
    void chargeFees() {
        int creditPerByte = config.get<float>("fees.store-byte") * config.get<int>("fees.fee-interval") * config.get<int>("general.credits-per-satoshi");
        int creditPerFile = config.get<float>("fees.store-file") * config.get<int>("fees.fee-interval") * config.get<int>("general.credits-per-satoshi");
        database::chargeFileUpkeepFees(feeDbConn, creditPerFile, creditPerByte);
    }
};

class ConnectionHandler
  : public boost::enable_shared_from_this<ConnectionHandler>
{
    tcp::socket socket_;
    
public:
    typedef boost::shared_ptr<ConnectionHandler> pointer;
    
    static pointer create(boost::asio::io_service& io) {
        return pointer(new ConnectionHandler(io));
    }
    
    tcp::socket& socket() {
        return socket_;
    }
    
    void start() {
        std::cout << "Reading packet from client." << std::endl;
        
        boost::shared_ptr<networking::NetvendPacket> packet(networking::NetvendPacket::readFromSocket(socket_));
        
        if (packet->typeChar() == networking::PACKETTYPECHAR_HANDSHAKE) {
            std::cout << "Handshake packet." << std::endl;
            boost::shared_ptr<networking::HandshakePacket> hsPacket = boost::dynamic_pointer_cast<networking::HandshakePacket>(packet);
            assert(hsPacket != NULL);
            
            std::cout << "Processing handshake." << std::endl;
            networking::HandshakeResponse response = processHandshakePacket(hsPacket);
            
            std::cout << "Sending response." << std::endl;
            response.writeToSocket(socket_);
            
            std::cout << "Response sent." << std::endl;
        }
        else if (packet->typeChar() == networking::PACKETTYPECHAR_COMMANDBATCH) {
            std::cout << "CommandBatch packet." << std::endl;
            boost::shared_ptr<networking::CommandBatchPacket> cbPacket = boost::dynamic_pointer_cast<networking::CommandBatchPacket>(packet);
            assert(cbPacket.get() != NULL);
            std::cout << "agent address: " << cbPacket->agentAddress() << std::endl;
            
            std::cout << "Processing CommandBatch." << std::endl;
            networking::CommandBatchResponse response = processCommandBatchPacket(cbPacket);
            
            std::cout << "Sending CommandBatchResponse." << std::endl;
            response.writeToSocket(socket_);
            
            std::cout << "Response sent." << std::endl;
        }
        std::cout << "Packet processed." << std::endl << std::endl;
    }

private:
    ConnectionHandler(boost::asio::io_service& io)
      : socket_(io)
    {}
};

class ListenServer {
    tcp::acceptor acceptor_;
    
public:
    ListenServer(boost::asio::io_service& io)
      : acceptor_(io, tcp::endpoint(tcp::v4(), DEFAULT_SERVER_PORT))
    {
        startAccept();
    }

private:
    void startAccept() {
        ConnectionHandler::pointer newConnectionHandler =
          ConnectionHandler::create(acceptor_.get_io_service());
        
        acceptor_.async_accept(newConnectionHandler->socket(),
            boost::bind(&ListenServer::handleAccept, this, newConnectionHandler, boost::asio::placeholders::error));
    }
    
    void handleAccept(ConnectionHandler::pointer newConnectionHandler, const boost::system::error_code& error) {
        if (!error) {
            newConnectionHandler->start();
        }
        else {
            std::cerr << "accept (i think?) returned error code " << error << std::endl;
        }
        
        startAccept();
    }
};

void loadConfigVars() {
    boost::property_tree::ini_parser::read_ini("config.ini", config);
}

int main() {
    std::cout << "Loading config... ";
    loadConfigVars();
    std::cout << "Done." << std::endl;
    
    std::cout << "Preparing database connection... ";
    database::prepareConnection(&dbConn);
    std::cout << "Done." << std::endl << std::endl;
    
    try {
        boost::asio::io_service io;
        
        FeeHandler hs;
        boost::thread hsThread(hs);
        
        ListenServer ls(io);
        
        io.run();
    }
    catch (std::exception& e) {
        std::cout << e.what() << std::endl;
    }
    
    delete dbConn;
    
    return 0;
}