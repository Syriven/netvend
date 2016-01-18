#define _SCL_SECURE_NO_WARNINGS

#include <iostream>
#include <string>
#include <map>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/lexical_cast.hpp>
#include <stdexcept>

#include "netvend/common_constants.h"
#include "util/crypto.h"
#include "util/networking.h"
#include "netvend/commands.h"
#include "netvend/packet.h"
#include "netvend/response.h"

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
    
    unsigned long createPage(std::string name, unsigned long pocketID) {
        boost::shared_ptr<commands::Command> command(new commands::CreatePage(name, pocketID));
        
        boost::shared_ptr<commands::results::Result> result = performSingleCommand(command);
        
        boost::shared_ptr<commands::results::CreatePage> ccResult =
          boost::dynamic_pointer_cast<commands::results::CreatePage>(result);
        
        assert(ccResult.get() != NULL);
        
        return ccResult->pageID();
    }
    
    void updatePageByID(unsigned long pageID, unsigned char* data, unsigned short dataSize) {
        boost::shared_ptr<commands::Command> command(new commands::UpdatePageByID(pageID, data, dataSize));
        
        boost::shared_ptr<commands::results::Result> result = performSingleCommand(command);
        
        boost::shared_ptr<commands::results::UpdatePageByID> ucbiResult =
        boost::dynamic_pointer_cast<commands::results::UpdatePageByID>(result);
        
        assert(ucbiResult.get() != NULL);
    }
};

std::map<std::string, Agent> agents;

std::string selectedAgentName;
Agent* selectedAgent;

const char helpstr[] = 
"help - help\n\
q - quit\n\
newagent [name] - new agent\n\
agents - list agents\n\
agent [name] - select agent\n\
\n\
h - Perform netvend handshake\n\
newpocket - Create new Pocket\n\
pocketdeposit [pocketID] - Request deposit address for pocket\n\
newpage [name] [pocketID] - Create a new page with [name], thethered to pocket [pocketID]\n\
write [pageID] [data] - write to page [pageID] with [data] (overwrites old data)";

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
        else if (commandCode == "newagent") {
            std::string name;
            std::cin >> name;

            createNewAgent(name, io);

            selectAgent(name);
            selectedAgent->setConnection(&nvConnection);
        }
        else if (commandCode == "agents") {
            for (std::map<std::string, Agent>::iterator it=agents.begin(); it != agents.end(); it++) {
                std::cout << it->first << " (" << it->second.getAddress() << ")" << std::endl;
            }
        }
        else if (commandCode == "agent") {
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
        else if (commandCode == "newpocket") {
            std::cout << "Pocket created with id " << selectedAgent->createPocket() << std::endl;
        }
        else if (commandCode == "pocketdeposit") {
            unsigned long pocketID;
            
            std::cin >> pocketID;
            
            std::cout << "Pocket " << pocketID << " now has a deposit address " << selectedAgent->requestPocketDepositAddress(pocketID) << std::endl;
        }
        else if (commandCode == "newpage") {
            std::string name;
            unsigned long pocketID;
            
            std::cin >> name >> pocketID;
            
            std::cout << "Page " << selectedAgent->createPage(name, pocketID) << " has been created." << std::endl;
        }
        else if (commandCode == "write") {
            unsigned long pageID;
            std::string s;
            
            std::cin >> pageID >> s;
            
            selectedAgent->updatePageByID(pageID, (unsigned char*)s.data(), s.size());
            
            std::cout << "Page " << pageID << " updated." << std::endl;
        }
        else if (commandCode == "t") {
            createNewAgent("default", io);
            selectAgent("default");
            selectedAgent->setConnection(&nvConnection);
            unsigned long pocket = selectedAgent->performNetvendHandshake();
            unsigned long page = selectedAgent->createPage("testpage", pocket);
            selectedAgent->updatePageByID(page, (unsigned char*)"hi", 2);
        }
        else {
            std::cout << "Unrecognized command." << std::endl;
        }
    }
    
    std::cin.get();
}