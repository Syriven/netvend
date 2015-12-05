#ifndef NETVEND_NETWORKING_H
#define NETVEND_NETWORKING_H

#include <iostream>
#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
//#include <exception>
#include <stdexcept>
//#include <string>

namespace networking {

class NetvendDecodeException : public std::runtime_error {
public:
    NetvendDecodeException(const char* message);
};

class NetworkingException : public std::runtime_error {
    boost::system::error_code error;
public:
    NetworkingException(boost::system::error_code& error, const char* message);
    boost::system::error_code getError();
};

class ReadException : public NetworkingException {
public:
    ReadException(boost::system::error_code& error);
};

class WriteException : public NetworkingException {
public:
    WriteException(boost::system::error_code& error);
};

bool connectSocket(boost::asio::ip::tcp::socket& socket, std::string ip, unsigned int port);
bool disconnectSocket(boost::asio::ip::tcp::socket& socket);

void readToBufOrThrow(boost::asio::ip::tcp::socket& socket, unsigned char* buf, size_t size);
void writeBufOrThrow(boost::asio::ip::tcp::socket& socket, unsigned char* buf, size_t size);
void readToVchOrThrow(boost::asio::ip::tcp::socket& socket, std::vector<unsigned char>* vch);
void writeVchOrThrow(boost::asio::ip::tcp::socket& socket, std::vector<unsigned char>& vch);
    
}//namespace networking

#endif