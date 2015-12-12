#define _SCL_SECURE_NO_WARNINGS

#include <iostream>
#include <string>
#include <map>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/lexical_cast.hpp>
#include <stdexcept>

#include "common_constants.h"
#include "crypto.h"
#include "networking.h"
#include "commands.h"
#include "nv_packet.h"
#include "nv_response.h"

using boost::asio::ip::tcp;

CryptoPP::AutoSeededRandomPool rng;

class NetvendConnection {
    tcp::socket socket_;
    bool connected;
public:
    NetvendConnection(boost::asio::io_service& io)
      : socket_(io) 
    {
        connected = false;
    }
    tcp::socket& socket() {
        return socket_;
    }
    bool disconnect() {
        if (!connected) return true;
        
        if (networking::disconnectSocket(socket_)) {
            connected = false;
            return true;
        }
        else return false;
    }
    bool connect(std::string ip=DEFAULT_SERVER_IP, unsigned int port=DEFAULT_SERVER_PORT) {
        if (connected) {
            if (!disconnect()) {
                std::cerr << "cannot disconnect before connecting." << std::endl;
                return false;
            }
        }
        
        if (networking::connectSocket(socket_, ip, port)) {
            connected = true;
            return true;
        }
        else {
            connected = false;
            return false;
        }
    }
};

class Agent {
    CryptoPP::InvertibleRSAFunction RSAFunction;
    crypto::RSAPubkey pubkey;
    crypto::RSAPrivkey privkey;
    std::string agentAddress;
    std::string depositAddress;
    bool populated;
    NetvendConnection* nvConnection;
    
    bool connectToNetvend(std::string ip=DEFAULT_SERVER_IP, unsigned int port=DEFAULT_SERVER_PORT) {
        if (nvConnection == NULL) throw std::runtime_error("Must first associate NetvendConnection with Agent.setConnection()");
        if (!populated) throw std::runtime_error("Agent keypair has not been generated");
        
        return nvConnection->connect(ip, port);
    }
    bool disconnectFromNetvend() {
        if (nvConnection == NULL) throw std::runtime_error("Agent has a NULL NetvendConnection pointer");
        return nvConnection->disconnect();
    }
    
public:
    Agent(boost::asio::io_service& io) {
        populated = false;
        nvConnection = NULL;
    }
    void setConnection(NetvendConnection *nv) {
        nvConnection = nv;
    }
    void updateInfo() {
        //update public key
        pubkey = crypto::RSAPubkey(RSAFunction);
        privkey = crypto::RSAPrivkey(RSAFunction);

        //update address
        agentAddress = crypto::RSAPubkeyToNetvendAddress(pubkey);
    }
    void generateNew() {
        RSAFunction.GenerateRandomWithKeySize(rng, AGENT_KEYSIZE);
        updateInfo();
        populated = true;
    }
    void test() {

    }
    std::string getAddress() {
        if (!populated) throw std::runtime_error("Agent keypair has not been generated");
        return agentAddress;
    }
    std::vector<unsigned char> getAddressBytes() {
        if (!populated) throw std::runtime_error("Agent keypair has not been generated");
        return std::vector<unsigned char>(getAddress().begin(), getAddress().end());
    }
    CryptoPP::InvertibleRSAFunction getFunction() {
        if (!populated) throw std::runtime_error("Agent keypair has not been generated");
        return RSAFunction;
    }
    std::vector<unsigned char> signMessage(std::vector<unsigned char> &message) {
        if (!populated) throw std::runtime_error("Agent keypair has not been generated");
        
        return crypto::cryptoSign(message, privkey, rng);
    }
    
    //functions that need to connect to netvend
    unsigned long performNetvendHandshake() {
        connectToNetvend();
        
        //send handshake
        networking::HandshakePacket packet(pubkey);
        packet.writeToSocket(nvConnection->socket());
        
        //receive handshake response
        boost::shared_ptr<networking::HandshakeResponse> response(networking::HandshakeResponse::readFromSocket(nvConnection->socket()));
        if (response.get() == NULL) std::cerr << "read failed" << std::endl;
        
        disconnectFromNetvend();
        
        return response->defaultPocketID();
    }
    
    boost::shared_ptr<commands::results::Result> performSingleCommand(boost::shared_ptr<commands::Command> command) {
        connectToNetvend();
        
        commands::Batch* cb = new commands::Batch();
        cb->addCommand(command);
        
        boost::shared_ptr<std::vector<unsigned char> > cbData(new std::vector<unsigned char>());
        cb->writeToVch(cbData.get());
        
        std::vector<unsigned char> sig = crypto::cryptoSign(*cbData, privkey, rng);
        
        //create commandBatchPacket
        networking::CommandBatchPacket cbp(agentAddress, cbData, sig);
        
        //send it
        cbp.writeToSocket(nvConnection->socket());
        
        //receive response
        boost::shared_ptr<networking::CommandBatchResponse> response(networking::CommandBatchResponse::readFromSocket(nvConnection->socket(), cb));
        if (response.get() == NULL) std::cerr << "read failed" << std::endl;
        
        disconnectFromNetvend();
        
        boost::shared_ptr<commands::results::Result> result = response->commandResultBatch()->results()->at(0);
        
        if (result->error()) {
            boost::shared_ptr<commands::errors::Error> error = boost::dynamic_pointer_cast<commands::errors::Error>(result);
            throw *error;
        }
        
        return result;
    }
    
    unsigned long createPocket() {
        boost::shared_ptr<commands::Command> command(new commands::CreatePocket());
        
        boost::shared_ptr<commands::results::Result> result = performSingleCommand(command);
        
        boost::shared_ptr<commands::results::CreatePocket> cpResult =
          boost::dynamic_pointer_cast<commands::results::CreatePocket>(result);
        
        assert(cpResult.get() != NULL);
        
        return cpResult->pocketID();
    }
    
    std::string requestPocketDepositAddress(unsigned long pocketID) {
        boost::shared_ptr<commands::Command> command(new commands::RequestPocketDepositAddress(pocketID));
        
        boost::shared_ptr<commands::results::Result> result = performSingleCommand(command);
        
        boost::shared_ptr<commands::results::RequestPocketDepositAddress> rpdaResult =
          boost::dynamic_pointer_cast<commands::results::RequestPocketDepositAddress>(result);
        
        assert(rpdaResult.get() != NULL);
        
        return rpdaResult->depositAddress();
    }
};

std::map<std::string, Agent> agents;

std::string selectedAgentName;
Agent* selectedAgent;

const char helpstr[] = 
"help - help\n\
q - quit\n\
n [name] - new agent\n\
l - list agents\n\
s [name] - select agent\n\
\n\
h - Perform netvend handshake\n\
cp (1/0) - Create new Pocket with or without deposit address\n\
rpda [pocketID] - Request new deposit address for pocket";

void createNewAgent(std::string name, boost::asio::io_service& io, bool output=true) {
    Agent agent(io);
    agent.generateNew();
    agents.insert(std::pair<std::string, Agent>(name, agent));
    if (output)
        std::cout << "new agent '" << name << "' created with address " << agent.getAddress() << std::endl;
}

bool selectAgent(std::string name, bool output=true) {
    selectedAgentName = name;
    try {
        selectedAgent = &(agents.at(name));
    }
    catch (std::out_of_range &e) {
    if (output)
        std::cout << "no agent '" << name << "' exists!" << std::endl;
    return false;
    }
    if (output)
        std::cout << "agent '" << name << "' selected." << std::endl;
    return true;
}

int main() {
    boost::asio::io_service io;
    NetvendConnection nvConnection(io);
    
    while (true) {
        std::cin.sync();

        std::string commandCode;
        std::cout << "> ";
        std::cin >> commandCode;

        if (commandCode == "help") {
            std::cout << helpstr << std::endl;
        }
        else if (commandCode == "q") {
            return 0;
        }
        else if (commandCode == "n") {
            std::string name;
            std::cin >> name;

            createNewAgent(name, io);

            selectAgent(name);
            selectedAgent->setConnection(&nvConnection);
        }
        else if (commandCode == "l") {
            for (std::map<std::string, Agent>::iterator it=agents.begin(); it != agents.end(); it++) {
                std::cout << it->first << " (" << it->second.getAddress() << ")" << std::endl;
            }
        }
        else if (commandCode == "n") {
            std::string name;
            std::cin >> name;

            selectAgent(name);
        }
        else if (commandCode == "h") {
            unsigned long defaultPocketID = selectedAgent->performNetvendHandshake();
            if (defaultPocketID) {
                std::cout << "Handshake complete; added to netvend as a new agent." << std::endl;
                std::cout << "Default agent pocket id: " << defaultPocketID << std::endl;
            }
            else {
                std::cout << "Handshake complete; recognized by netvend as an existing agent." << std::endl;
            }
        }
        else if (commandCode == "cp") {
            std::cout << "Pocket created with id " << selectedAgent->createPocket() << std::endl;
        }
        else if (commandCode == "rpda") {
            unsigned long pocketID;
            std::cin >> pocketID;
            std::cout << "Pocket " << pocketID << " now has a deposit address " << selectedAgent->requestPocketDepositAddress(pocketID) << std::endl;
        }
        else if (commandCode == "t") {
            //selectedAgent->createPost(std::string("test"), std::string("oh wow dataaaa"));
        }
        else {
            std::cout << "Unrecognized command." << std::endl;
        }
    }
    
    std::cin.get();
}