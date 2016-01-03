#include "networking.h"

namespace networking {

NetvendDecodeException::NetvendDecodeException(const char* message)
: std::runtime_error(message)
{}

NetworkingException::NetworkingException(boost::system::error_code& error, const char* message)
: std::runtime_error(message), error(error)
{}
    
boost::system::error_code NetworkingException::getError() {return error;}

ReadException::ReadException(boost::system::error_code& error)
  : NetworkingException(error, "Networking Read Exception")
{}

WriteException::WriteException(boost::system::error_code& error)
  : NetworkingException(error, "Networking Write Exception")
{}

bool connectSocket(boost::asio::ip::tcp::socket& socket, std::string ip, unsigned int port) {
    boost::asio::ip::tcp::resolver resolver(socket.get_io_service());
    boost::asio::ip::tcp::resolver::query query(ip, boost::lexical_cast<std::string>(port));
    
    boost::asio::ip::tcp::resolver::iterator endpointIterator = resolver.resolve(query);
    boost::asio::ip::tcp::resolver::iterator end;
    boost::system::error_code error = boost::asio::error::host_not_found;
    while (error && endpointIterator != end) {
        socket.close();
        socket.connect(*endpointIterator++, error);
    }
    
    if (error) {
        std::cerr << "failed to connect. error " << error << std::endl;
        return false;
    }
    else return true;
}

bool disconnectSocket(boost::asio::ip::tcp::socket& socket) {
    boost::system::error_code error;

    socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, error);
    if (error) {
        std::cerr << "socket shutdown failed with error " << error << std::endl;
        return false;
    }

    socket.close(error);
    if (error) {
        std::cerr << "socket close failed with error " << error << std::endl;
        return false;
    }
    
    return true;
}

void readToBufOrThrow(boost::asio::ip::tcp::socket& socket, unsigned char* buf, size_t size) {
    boost::system::error_code error;
    boost::asio::read(socket, boost::asio::buffer(buf, size), boost::asio::transfer_all(), error);
    if (error) {
        throw ReadException(error);
    }
}

void writeBufOrThrow(boost::asio::ip::tcp::socket& socket, unsigned char* buf, size_t size) {
    boost::system::error_code error;
    boost::asio::write(socket, boost::asio::buffer(buf, size), boost::asio::transfer_all(), error);
    if (error) {
        throw WriteException(error);
    }
}

void writeVchOrThrow(boost::asio::ip::tcp::socket& socket, std::vector<unsigned char>& vch) {
    writeBufOrThrow(socket, vch.data(), vch.size());
}

void readToVchOrThrow(boost::asio::ip::tcp::socket& socket, std::vector<unsigned char>* vch) {
    readToBufOrThrow(socket, vch->data(), vch->size());
}

}//namespace networking