#ifndef NETVEND_NV_RESPONSE_H
#define NETVEND_NV_RESPONSE_H

#include <string>

#include "pack.h"
#include "networking.h"
#include "commands.h"
#include "common_constants.h"

namespace networking {

class HandshakeResponse {
    bool isNewAgent_;
    unsigned long defaultPocketID_;
public:
    HandshakeResponse(bool isNewAgent, unsigned long defaultPocketID);
    HandshakeResponse(bool isNewAgent);
    static HandshakeResponse* readFromSocket(boost::asio::ip::tcp::socket& socket);
    bool isNewAgent();
    unsigned long defaultPocketID();
    void writeToSocket(boost::asio::ip::tcp::socket& socket);
};

class CommandBatchResponse {
    boost::shared_ptr<commands::results::Batch> commandResultBatch_;
    unsigned char completion_;
public:
    CommandBatchResponse(boost::shared_ptr<commands::results::Batch> commandResultBatch, unsigned char completion);
    static CommandBatchResponse* readFromSocket(boost::asio::ip::tcp::socket& socket, commands::Batch* initiatingCommandBatch);
    void writeToSocket(boost::asio::ip::tcp::socket& socket);
    boost::shared_ptr<commands::results::Batch> commandResultBatch();
};

}//namespace networking

#endif