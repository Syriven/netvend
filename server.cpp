#include <iostream>
#include <cryptopp/osrng.h>
#include <cryptopp/rsa.h>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
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
        std::cout << defaultPocketID << std::endl;
        
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

//throws appropriate errors if the pocket doesn't exist or isn't owned by the agent
void checkOwnedPocket(unsigned long pocketID, std::string agentAddress) {
    std::string fetchedAgentAddress;
    try {
        fetchedAgentAddress = database::fetchPocketOwner(dbConn, pocketID);
    }
    catch (database::NoRowFoundException &e) {
        //Row doesn't exist; pocketID is invalid
        commands::errors::Error* error = new commands::errors::InvalidTarget(boost::lexical_cast<std::string>(pocketID), 0, true);
        throw NetvendCommandException(error);
    }
    if (fetchedAgentAddress != agentAddress) {
        //Pocket exists but is owned by a different agent
        commands::errors::Error* error = new commands::errors::TargetNotOwned(boost::lexical_cast<std::string>(pocketID), 0, true);
        throw NetvendCommandException(error);
    }
}

void checkOwnedPage(unsigned long pageID, std::string agentAddress) {
    std::string fetchedAgentAddress;
    try {
        fetchedAgentAddress = database::fetchPageOwner(dbConn, pageID);
    }
    catch (database::NoRowFoundException &e) {
        //Row doesn't exist; pocketID is invalid
        commands::errors::Error* error = new commands::errors::InvalidTarget(boost::lexical_cast<std::string>(pageID), 0, true);
        throw NetvendCommandException(error);
    }
    if (fetchedAgentAddress != agentAddress) {
        //Pocket exists but is owned by a different agent
        commands::errors::Error* error = new commands::errors::TargetNotOwned(boost::lexical_cast<std::string>(pageID), 0, true);
        throw NetvendCommandException(error);
    }
}

boost::shared_ptr<commands::results::CreatePocket> processCreatePocketCommand(std::string agentAddress, boost::shared_ptr<commands::CreatePocket> command) {    
    unsigned int pocketID = database::insertPocket(dbConn, agentAddress);
    
    int cost = 0;
    
    return boost::shared_ptr<commands::results::CreatePocket>(new commands::results::CreatePocket(cost, pocketID));
}

boost::shared_ptr<commands::results::RequestPocketDepositAddress> processRequestPocketDepositAddressCommand(std::string agentAddress, boost::shared_ptr<commands::RequestPocketDepositAddress> command) {
    unsigned long pocketID = command->pocketID();
    
    checkOwnedPocket(pocketID, agentAddress);
    
    std::string depositAddress = btc::getNewDepositAddress();
    
    database::updatePocketDepositAddress(dbConn, agentAddress, pocketID, depositAddress);
    
    boost::shared_ptr<commands::results::RequestPocketDepositAddress> rpdaResult(
      new commands::results::RequestPocketDepositAddress(0, depositAddress)
    );
    
    return rpdaResult;
}

boost::shared_ptr<commands::results::CreatePage> processCreatePageCommand(std::string agentAddress, boost::shared_ptr<commands::CreatePage> command) {
    unsigned long pocketID = command->pocketID();
    
    checkOwnedPocket(pocketID, agentAddress);
    
    std::string name = command->name();
    
    unsigned long pageID = database::insertPage(dbConn, agentAddress, name, pocketID);
    
    boost::shared_ptr<commands::results::CreatePage> ccResult(
      new commands::results::CreatePage(0, pageID)
    );
    
    return ccResult;
}

boost::shared_ptr<commands::results::UpdatePageByID> processUpdatePageByIDCommand(std::string agentAddress, boost::shared_ptr<commands::UpdatePageByID> command) {
    unsigned long pageID = command->pageID();
    
    checkOwnedPage(pageID, agentAddress);
    
    unsigned char* data = command->data();
    unsigned short dataSize = command->dataSize();
    
    try {
        database::updatePageByID(dbConn, pageID, data, dataSize);
    }
    catch (database::NoRowFoundException &e) {
        commands::errors::Error* error = new commands::errors::InvalidTarget(boost::lexical_cast<std::string>(pageID), 0, true);
        throw NetvendCommandException(error);
    }
    
    boost::shared_ptr<commands::results::UpdatePageByID> ucbiResult(
      new commands::results::UpdatePageByID(0)
    );
    
    return ucbiResult;
}

boost::shared_ptr<commands::results::ReadPageByID> processReadPageByIDCommand(std::string agentAddress, boost::shared_ptr<commands::ReadPageByID> command) {
    unsigned long pageID = command->pageID();
    
    std::vector<unsigned char> pageData;
    try {
        pageData = database::readPageByID(dbConn, pageID);
    }
    catch (database::NoRowFoundException &e) {
        commands::errors::Error* error = new commands::errors::InvalidTarget(boost::lexical_cast<std::string>(pageID), 0, true);
        throw NetvendCommandException(error);
    }
    
    boost::shared_ptr<commands::results::ReadPageByID> readResult(
      new commands::results::ReadPageByID(0, pageData)
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
    else if (command->typeChar() == commands::COMMANDTYPECHAR_CREATE_PAGE) {
        boost::shared_ptr<commands::CreatePage> ccCommand = 
        boost::dynamic_pointer_cast<commands::CreatePage>(command);
        
        if (ccCommand.get() == NULL) {
            throw networking::NetvendDecodeException("Error decoding what seems to be an CreatePage command.");
        }
        
        return processCreatePageCommand(agentAddress, ccCommand);
    }
    else if (command->typeChar() == commands::COMMANDTYPECHAR_UPDATE_PAGE_BY_ID) {
        boost::shared_ptr<commands::UpdatePageByID> writeCommand = 
        boost::dynamic_pointer_cast<commands::UpdatePageByID>(command);
        
        if (writeCommand.get() == NULL) {
            throw networking::NetvendDecodeException("Error decoding what seems to be an ucbi command.");
        }
        
        return processUpdatePageByIDCommand(agentAddress, writeCommand);
    }
    else if (command->typeChar() == commands::COMMANDTYPECHAR_READ_PAGE_BY_ID) {
        boost::shared_ptr<commands::ReadPageByID> readCommand =
        boost::dynamic_pointer_cast<commands::ReadPageByID>(command);
        
        if (readCommand.get() == NULL) {
            throw networking::NetvendDecodeException("Error decoding what seems to be a readPage command.");
        }
        
        return processReadPageByIDCommand(agentAddress, readCommand);
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

int main() {
    database::prepareConnection(&dbConn);
    
    try {
        boost::asio::io_service io;
        ListenServer ls(io);
        io.run();
    }
    catch (std::exception& e) {
        std::cout << e.what() << std::endl;
    }
    
    delete dbConn;
    
    return 0;
}