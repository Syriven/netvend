#include "packet.h"

namespace networking {

NetvendPacket::NetvendPacket(unsigned char typeChar) 
: typeChar_(typeChar)
{
}

NetvendPacket* NetvendPacket::readFromSocket(boost::asio::ip::tcp::socket& socket) {
    unsigned char buf[1];
    readToBufOrThrow(socket, buf, 1);
    
    unsigned char typeChar;
    unpack(buf, "c", &typeChar);
    
    if (typeChar == PACKETTYPECHAR_HANDSHAKE) {
        return HandshakePacket::readFromSocket(socket);
    }
    else if (typeChar == PACKETTYPECHAR_COMMANDBATCH) {
        return CommandBatchPacket::readFromSocket(socket);
    }
    return NULL;
}

void NetvendPacket::writeToSocket(boost::asio::ip::tcp::socket& socket) {
    unsigned char buf[1];
    pack(buf, "c", typeChar_);
    
    writeBufOrThrow(socket, buf, 1);
    
    writeDataToSocket(socket);//calls child's function to write more data
}

unsigned char NetvendPacket::typeChar() {return typeChar_;}

void NetvendPacket::writeDataToSocket(boost::asio::ip::tcp::socket& socket) {}



//Handshake

//HandshakePacket

HandshakePacket::HandshakePacket(CryptoPP::RSA::PublicKey pubkey)
: NetvendPacket(PACKETTYPECHAR_HANDSHAKE), pubkey_(pubkey)
{}

HandshakePacket* HandshakePacket::readFromSocket(boost::asio::ip::tcp::socket& socket) {
    unsigned char buf[DERENCODED_PUBKEY_SIZE];
    networking::readToBufOrThrow(socket, buf, DERENCODED_PUBKEY_SIZE);
    
    CryptoPP::ByteQueue bq;
    bq.Put(buf, DERENCODED_PUBKEY_SIZE);
    bq.MessageEnd();
    
    CryptoPP::RSA::PublicKey pubkey;
    pubkey.Load(bq);
    
    return new HandshakePacket(pubkey);
}

void HandshakePacket::writeDataToSocket(boost::asio::ip::tcp::socket& socket) {
    //get the pubkey into a bytequeue
    CryptoPP::ByteQueue bq;
    pubkey_.DEREncode(bq);
    size_t pubkeySize = bq.CurrentSize();
    assert(pubkeySize == DERENCODED_PUBKEY_SIZE);
    
    //copy to command vch
    std::vector<unsigned char> vch(pubkeySize);
    bq.Get(vch.data(), pubkeySize);
    
    //write data to socket
    networking::writeVchOrThrow(socket, vch);
}

CryptoPP::RSA::PublicKey HandshakePacket::pubkey() {return pubkey_;}

CommandBatchPacket::CommandBatchPacket(std::string agentAddress, boost::shared_ptr<std::vector<unsigned char> > commandBatchData, std::vector<unsigned char> &sig)
: NetvendPacket(PACKETTYPECHAR_COMMANDBATCH), agentAddress_(agentAddress), commandBatchData_(commandBatchData), sig_(sig)
{
    assert(commandBatchData_->size() < 65535);
}

CommandBatchPacket* CommandBatchPacket::readFromSocket(boost::asio::ip::tcp::socket& socket) {
    //leave an extra byte so even a full address is followed by a \0.
    //this allows the string(buf) constructor later to get the right size
    unsigned char addrbuf[MAX_ADDRESS_SIZE+1];
    memset(addrbuf, '\0', MAX_ADDRESS_SIZE+1);
    
    networking::readToBufOrThrow(socket, addrbuf, MAX_ADDRESS_SIZE);
    
    std::string agentAddress((char*)addrbuf);
    
    unsigned char cbsbuf[2];
    networking::readToBufOrThrow(socket, cbsbuf, 2);
    
    unsigned short commandBatchSize;
    int n = unpack(cbsbuf, "H", &commandBatchSize);
    assert(n==2);
    
    boost::shared_ptr<std::vector<unsigned char> > cbData(new std::vector<unsigned char>(commandBatchSize));
    networking::readToVchOrThrow(socket, cbData.get());
    
    std::vector<unsigned char> sig(MAX_SIG_SIZE);
    networking::readToVchOrThrow(socket, &sig);
    
    return new CommandBatchPacket(agentAddress, cbData, sig);
}

void CommandBatchPacket::writeDataToSocket(boost::asio::ip::tcp::socket& socket) {
    assert(agentAddress_.size() > 0 && agentAddress_.size() <= MAX_ADDRESS_SIZE);
    
    assert(commandBatchData_->size() > 0 && commandBatchData_->size() <= 65535);
    unsigned short cbDataSize = commandBatchData_->size();
    
    assert(sig_.size() == MAX_SIG_SIZE);
    
    unsigned char addrbuf[MAX_ADDRESS_SIZE];
    memset(addrbuf, '\0', MAX_ADDRESS_SIZE);
    agentAddress_.copy((char*)addrbuf, agentAddress_.size());
    
    unsigned char cbsbuf[2];
    int n = pack(cbsbuf, "H", cbDataSize);
    assert(n==2);
    
    networking::writeBufOrThrow(socket, addrbuf, MAX_ADDRESS_SIZE);
    networking::writeBufOrThrow(socket, cbsbuf, 2);
    networking::writeVchOrThrow(socket, *commandBatchData_);
    networking::writeVchOrThrow(socket, sig_);
}

std::string CommandBatchPacket::agentAddress() {
    return agentAddress_;
}

boost::shared_ptr<std::vector<unsigned char> > CommandBatchPacket::commandBatchData() {
    return commandBatchData_;
}

std::vector<unsigned char> CommandBatchPacket::sig() {
    return sig_;
}

}//namespace networking