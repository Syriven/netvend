#include <iostream>
#include <cryptopp/osrng.h>
#include <cryptopp/rsa.h>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <pqxx/pqxx>

#include "common_constants.h"
#include "database.h"
#include "btc.h"
#include "crypto.h"
#include "networking.h"
#include "commands.h"
#include "nv_packet.h"
#include "nv_response.h"

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

boost::shared_ptr<commands::results::CreatePocket> processCreatePocketCommand(std::string agentAddress, boost::shared_ptr<commands::CreatePocket> command) {    
    unsigned int pocketID;
    if (command->requestingDepositAddress()) {
        std::string depositAddress = btc::getNewDepositAddress();
        pocketID = database::insertPocket(dbConn, agentAddress, depositAddress);
    }
    else {
        pocketID = database::insertPocket(dbConn, agentAddress);
    }
    
    int cost = 0;
    
    return boost::shared_ptr<commands::results::CreatePocket>(new commands::results::CreatePocket(cost, pocketID));
}

boost::shared_ptr<commands::results::RequestPocketDepositAddress> processRequestPocketDepositAddressCommand(std::string agentAddress, boost::shared_ptr<commands::RequestPocketDepositAddress> command) {
    std::string depositAddress = btc::getNewDepositAddress();
    
    unsigned int pocketID = command->pocketID();
    
    database::updatePocketDepositAddress(dbConn, agentAddress, pocketID, depositAddress);
    
    boost::shared_ptr<commands::results::RequestPocketDepositAddress> rpdaResult(
      new commands::results::RequestPocketDepositAddress(0, depositAddress)
    );
    
    return rpdaResult;
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
            throw networking::NetvendDecodeException("Error decoding what seems to be a createpocket command.");
        }
        
        return processRequestPocketDepositAddressCommand(agentAddress, rpdaCommand);
    }
    else {
        throw networking::NetvendDecodeException((std::string("Error decoding command with commandtypechar ") + boost::lexical_cast<std::string>(command->typeChar())).c_str());
    }
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
        boost::shared_ptr<commands::results::Result> result = processCommand(packet->agentAddress(), command);
        crb->addResult(result);
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