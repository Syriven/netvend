#ifndef NETVEND_NV_PACKET_H
#define NETVEND_NV_PACKET_H

#include <string>
#include <cryptopp/rsa.h>

#include "util/pack.h"
#include "util/networking.h"
#include "netvend/common_constants.h"

namespace networking {

const char PACKETTYPECHAR_HANDSHAKE = 'H';
const char PACKETTYPECHAR_COMMANDBATCH = 'C';

class NetvendPacket {
    unsigned char typeChar_;
public:
    NetvendPacket(unsigned char typeChar);
    static NetvendPacket* readFromSocket(boost::asio::ip::tcp::socket& socket);
    void writeToSocket(boost::asio::ip::tcp::socket& socket);
    unsigned char typeChar();
protected:
    virtual void writeDataToSocket(boost::asio::ip::tcp::socket& socket);
};

class HandshakePacket : public NetvendPacket {
    CryptoPP::RSA::PublicKey pubkey_;
public:
    HandshakePacket(CryptoPP::RSA::PublicKey pubkey);
    static HandshakePacket* readFromSocket(boost::asio::ip::tcp::socket& socket);
    CryptoPP::RSA::PublicKey pubkey();
protected:
    void writeDataToSocket(boost::asio::ip::tcp::socket& socket);
};

class CommandBatchPacket : public NetvendPacket {//remember to check size
std::string agentAddress_;
boost::shared_ptr<std::vector<unsigned char> > commandBatchData_;
std::vector<unsigned char> sig_;
public:
    CommandBatchPacket(std::string agentAddress, boost::shared_ptr<std::vector<unsigned char> > commandBatchData, std::vector<unsigned char> &sig);
    static CommandBatchPacket* readFromSocket(boost::asio::ip::tcp::socket& socket);
    std::string agentAddress();
    boost::shared_ptr<std::vector<unsigned char> > commandBatchData();
    std::vector<unsigned char> sig();
protected:
    void writeDataToSocket(boost::asio::ip::tcp::socket& socket);
};

}//namespace networking

#endif