#include "nv_response.h"

namespace networking {

HandshakeResponse::HandshakeResponse(bool isNewAgent, unsigned long defaultPocketID)
: isNewAgent_(isNewAgent), defaultPocketID_(defaultPocketID)
{}

HandshakeResponse::HandshakeResponse(bool isNewAgent)
: isNewAgent_(isNewAgent)
{
    assert(!isNewAgent);//if new agent, should specify defaultPocketID.
    defaultPocketID_ = 0;
}

HandshakeResponse* HandshakeResponse::readFromSocket(boost::asio::ip::tcp::socket& socket) {
    static const int BUFSIZE = PACK_C_SIZE + PACK_L_SIZE;
    unsigned char buf[BUFSIZE];
    
    networking::readToBufOrThrow(socket, buf, BUFSIZE);
    
    unsigned char isNewAgentChar;
    unsigned long defaultPocketID;
    
    int place = 0;
    place += unpack(buf+place, "C", &isNewAgentChar);
    place += unpack(buf+place, "L", &defaultPocketID);
    assert(place == BUFSIZE);
    
    return new HandshakeResponse((bool)isNewAgentChar, defaultPocketID);
}

void HandshakeResponse::writeToSocket(boost::asio::ip::tcp::socket& socket) {
    static const int BUFSIZE = PACK_C_SIZE + PACK_L_SIZE;
    unsigned char buf[BUFSIZE];
    
    unsigned char isNewAgentChar = (unsigned char)isNewAgent_;
    
    int place = 0;
    place += pack(buf+place, "C", isNewAgentChar);
    place += pack(buf+place, "L", defaultPocketID_);
    assert(place == BUFSIZE);
    
    networking::writeBufOrThrow(socket, buf, BUFSIZE);
}

bool HandshakeResponse::isNewAgent() {
    return isNewAgent_;
}

unsigned long HandshakeResponse::defaultPocketID() {
    return defaultPocketID_;
}

CommandBatchResponse::CommandBatchResponse(boost::shared_ptr<commands::results::Batch> commandResultBatch, unsigned char completion)
: commandResultBatch_(commandResultBatch), completion_(completion)
{}

void CommandBatchResponse::writeToSocket(boost::asio::ip::tcp::socket& socket) {
    unsigned char completionbuf[1];
    pack(completionbuf, "C", completion_);
    
    std::vector<unsigned char> dataVch;
    commandResultBatch_->writeToVch(&dataVch);
    
    unsigned short dataVchSize = dataVch.size();
    unsigned char dvsbuf[2];
    pack(dvsbuf, "H", dataVchSize);
    
    networking::writeBufOrThrow(socket, completionbuf, 1);
    networking::writeBufOrThrow(socket, dvsbuf, 2);
    networking::writeVchOrThrow(socket, dataVch);
}

CommandBatchResponse* CommandBatchResponse::readFromSocket(boost::asio::ip::tcp::socket& socket, commands::Batch* initiatingCommandBatch) {
    unsigned char completionbuf[1];
    unsigned char dvsbuf[2];
    
    networking::readToBufOrThrow(socket, completionbuf, 1);
    networking::readToBufOrThrow(socket, dvsbuf, 2);
    
    unsigned char completion;
    unsigned short dataVchSize;
    
    unpack(completionbuf, "C", &completion);
    unpack(dvsbuf, "H", &dataVchSize);
    
    std::vector<unsigned char> dataVch(dataVchSize);
    networking::readToVchOrThrow(socket, &dataVch);
    
    boost::shared_ptr<commands::results::Batch> commandResultBatch(new commands::results::Batch(initiatingCommandBatch));
    unsigned char* ptr = dataVch.data();
    commandResultBatch->consumeDataFromBuf(&ptr);
    
    CommandBatchResponse* cbr = new CommandBatchResponse(commandResultBatch, completion);
    
    return cbr;
}

boost::shared_ptr<commands::results::Batch> CommandBatchResponse::commandResultBatch() {
    return commandResultBatch_;
}

}//namespace networking