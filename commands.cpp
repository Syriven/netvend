
#include "commands.h"

namespace commands {

    Command::Command(unsigned char typeChar)
    : typeChar_(typeChar)
    {}
    
    void Command::writeToVch(std::vector<unsigned char>* vch) {
        static const size_t DATA_SIZE = PACK_C_SIZE;
        
        unsigned int place = vch->size();
        
        vch->resize(place + DATA_SIZE);
        place += pack(vch->data()+place, "c", typeChar_);
        assert(place == vch->size());
    }

    Command* Command::consumeFromBuf(unsigned char **ptrPtr) {
        unsigned char typeChar;
        *ptrPtr += unpack((*ptrPtr), "c", &typeChar);
        
        if (typeChar == COMMANDTYPECHAR_CREATE_POCKET) {
            return commands::CreatePocket::consumeFromBuf(ptrPtr);
        }
        if (typeChar == COMMANDTYPECHAR_REQUEST_POCKET_DEPOSIT_ADDRESS) {
            return commands::RequestPocketDepositAddress::consumeFromBuf(ptrPtr);
        }
        else {
            throw std::runtime_error("bad packet; unrecognized command typechar '" + boost::lexical_cast<std::string>(typeChar) + "'");
            return NULL;
        }
    }

    unsigned char Command::typeChar() {
        return typeChar_;
    }
    
    
    
    Batch::Batch()
    {}
    
    void Batch::writeToVch(std::vector<unsigned char>* vch) {
        assert(commands_.size() > 0);
        
        unsigned char numCmds = commands_.size();
        
        static const size_t DATA_SIZE = PACK_C_SIZE;
        
        unsigned int place = vch->size();
        
        vch->resize(place + DATA_SIZE);
        place += pack(vch->data()+place, "C", numCmds);
        assert(place == vch->size());
        
        for (int i=0; i<numCmds; i++) {
            commands_[i]->writeToVch(vch);
        }
    }
    
    commands::Batch* Batch::consumeFromBuf(unsigned char **ptrPtr) {
        unsigned char numCmds;
        *ptrPtr += unpack(*ptrPtr, "C", &numCmds);
        
        commands::Batch* cb = new Batch();
        for (int i=0; i<numCmds; i++) {
            Command* command = Command::consumeFromBuf(ptrPtr);
            cb->addCommand(boost::shared_ptr<Command>(command));
        }
        return cb;
    }
    
    void Batch::addCommand(boost::shared_ptr<Command> command) {
        assert(commands_.size() < 255);
        commands_.push_back(command);
    }
    
    std::vector<boost::shared_ptr<Command> >* Batch::commands() {
        return &commands_;
    }
    
    

    CreatePocket::CreatePocket(bool requestingDepositAddress)
    : Command(COMMANDTYPECHAR_CREATE_POCKET), requestingDepositAddress_(requestingDepositAddress)
    {}

    void CreatePocket::writeToVch(std::vector<unsigned char>* vch) {
        Command::writeToVch(vch);
        
        unsigned char requestingDepositAddressChar = (int)requestingDepositAddress_;
        
        static const size_t DATA_SIZE = PACK_C_SIZE;
        
        unsigned int place = vch->size();
        
        vch->resize(place + DATA_SIZE);
        place += pack(vch->data()+place, "C", requestingDepositAddressChar);
        assert(place == vch->size());
    }

    commands::CreatePocket* CreatePocket::consumeFromBuf(unsigned char **ptrPtr) {
        unsigned char requestingDepositAddressChar;
        *ptrPtr += unpack(*ptrPtr, "C", &requestingDepositAddressChar);
        
        commands::CreatePocket* command = new CreatePocket((bool)requestingDepositAddressChar);
        
        return command;
    }

    bool CreatePocket::requestingDepositAddress() {
        return requestingDepositAddress_;
    }

    
    
    RequestPocketDepositAddress::RequestPocketDepositAddress(unsigned long pocketID)
    : Command(COMMANDTYPECHAR_REQUEST_POCKET_DEPOSIT_ADDRESS), pocketID_(pocketID)
    {}
    
    void RequestPocketDepositAddress::writeToVch(std::vector<unsigned char>* vch) {
        Command::writeToVch(vch);
        
        static const size_t DATA_SIZE = PACK_L_SIZE;
        
        unsigned int place = vch->size();
        
        vch->resize(place + DATA_SIZE);
        place += pack(vch->data()+place, "L", pocketID_);
        assert(place == vch->size());
    }
    
    commands::RequestPocketDepositAddress* RequestPocketDepositAddress::consumeFromBuf(unsigned char **ptrPtr) {
        unsigned long pocketID;
        *ptrPtr += unpack(*ptrPtr, "L", &pocketID);
        
        commands::RequestPocketDepositAddress* command = new RequestPocketDepositAddress(pocketID);
        
        return command;
    }
    
    unsigned long RequestPocketDepositAddress::pocketID() {
        return pocketID_;
    }

namespace results {

    Result::Result(unsigned char error, unsigned int cost)
    : error_(error), cost_(cost)
    {}
    
    void Result::writeToVch(std::vector<unsigned char>* vch) {
        static const size_t DATA_SIZE = PACK_C_SIZE + PACK_L_SIZE;
        
        unsigned int place = vch->size();
        
        vch->resize(place + DATA_SIZE);
        place += pack(vch->data()+place, "CL", error_, cost_);
        assert(place == vch->size());
    }

    Result* Result::consumeFromBuf(unsigned char** ptrPtr, unsigned char commandType) {
        unsigned char error;
        unsigned int cost;
        *ptrPtr += unpack(*ptrPtr, "CL", &error, &cost);
        
        if (error) {
            //return errors::Error::consumeFromBuf(error, cost, ptrPtr);
        }
        
        else if (commandType == commands::COMMANDTYPECHAR_CREATE_POCKET) {
            return results::CreatePocket::consumeFromBuf(cost, ptrPtr);
        }
        else if (commandType == commands::COMMANDTYPECHAR_REQUEST_POCKET_DEPOSIT_ADDRESS) {
            return results::RequestPocketDepositAddress::consumeFromBuf(cost, ptrPtr);
        }
        else {
            throw std::runtime_error("bad response; commandTypeChar " + boost::lexical_cast<std::string>(commandType) + " unrecognized.");
            return NULL;
        }
    }

    unsigned int Result::cost() {
        return cost_;
    }
    
    bool Result::error() {
        return error_;
    }
    
    
    
    Batch::Batch(commands::Batch* initiatingCommandBatch)
    : initiatingCommandBatch_(initiatingCommandBatch)
    {}
    
    void Batch::addResult(boost::shared_ptr<Result> result) {
        results_.push_back(result);
        cost_ += result->cost();
    }
    
    void Batch::writeToVch(std::vector<unsigned char>* vch) {
        assert(results_.size() <= 255);
        unsigned char numCmds = results_.size();
        
        static const size_t DATA_SIZE = PACK_C_SIZE;
        
        unsigned int place = vch->size();
        
        //std::cout << "num cmds written: " << (int)numCmds << std::endl;
        
        vch->resize(place + DATA_SIZE);
        place += pack(vch->data()+place, "C", numCmds);
        assert(place == vch->size());
        
        for (int i=0; i<numCmds; i++) {
            results_[i]->writeToVch(vch);
        }
    }
    
    void Batch::consumeDataFromBuf(unsigned char **ptrPtr) {
        unsigned char numCmds;
        *ptrPtr += unpack(*ptrPtr, "C", &numCmds);
        
        //std::cout << "num cmds read: " << (int)numCmds << std::endl;
        
        for (int i=0; i<numCmds; i++) {
            unsigned char typeChar = (*(initiatingCommandBatch_->commands()))[i]->typeChar();
            
            boost::shared_ptr<Result> result(Result::consumeFromBuf(ptrPtr, typeChar));
            addResult(result);
        }
    }
    
    std::vector<boost::shared_ptr<Result> >* Batch::results() {
        return &results_;
    }
    
    unsigned int Batch::cost() {
        return cost_;
    }

    
    
    CreatePocket::CreatePocket(int cost, unsigned long pocketID)
    : Result(false, cost), pocketID_(pocketID)
    {}

    void CreatePocket::writeToVch(std::vector<unsigned char>* vch) {
        Result::writeToVch(vch);
        
        static const size_t DATA_SIZE = PACK_L_SIZE;
        
        unsigned int place = vch->size();
        
        vch->resize(place + DATA_SIZE);
        place += pack(vch->data() + place, "L", pocketID_);
        assert(place == vch->size());
    }

    commands::results::CreatePocket* CreatePocket::consumeFromBuf(int cost, unsigned char **ptrPtr) {
        unsigned long pocketID;
        *ptrPtr += unpack(*ptrPtr, "L", &pocketID);
        
        return new commands::results::CreatePocket(cost, pocketID);
    }

    unsigned long CreatePocket::pocketID() {
        return pocketID_;
    }
    
    
    
    RequestPocketDepositAddress::RequestPocketDepositAddress(int cost, std::string depositAddress)
    : Result(false, cost), depositAddress_(depositAddress)
    {}
    
    void RequestPocketDepositAddress::writeToVch(std::vector<unsigned char>* vch) {
        Result::writeToVch(vch);
        
        static const size_t DATA_SIZE = MAX_ADDRESS_SIZE;
        
        unsigned int place = vch->size();
        
        vch->resize(place + DATA_SIZE);
        memset(vch->data()+place, '\0', DATA_SIZE);
        std::copy(depositAddress_.begin(), depositAddress_.end(), vch->begin()+place);
    }
    
    commands::results::RequestPocketDepositAddress* RequestPocketDepositAddress::consumeFromBuf(int cost, unsigned char **ptrPtr) {
        unsigned char buf[MAX_ADDRESS_SIZE + 1];
        memset(buf, '\0', MAX_ADDRESS_SIZE + 1);
        
        std::copy_n(*ptrPtr, MAX_ADDRESS_SIZE, buf);
        
        //this will interpret the first \0 as the end, so
        //an address less than 34 chars will result in the correct length string
        std::string depositAddress((char*)buf);
        
        return new commands::results::RequestPocketDepositAddress(cost, depositAddress);
    }
    
    std::string RequestPocketDepositAddress::depositAddress() {
        return depositAddress_;
    }

}//namesace commands::results

namespace errors {

//     Error::Error(unsigned char error, unsigned int cost, bool fatalToBatch)
//     : results::Result(error, cost), fatalToBatch_(fatalToBatch)
//     {}
//     
//     void Error::writeToVch(std::vector<unsigned char>* vch) {
//         results::Result::writeToVch(vch);
//         
//         static const size_t DATA_SIZE = PACK_C_SIZE;
//         
//         unsigned int place = vch->size();
//         
//         vch->resize(place + DATA_SIZE);
//         place += pack(vch->data()+place, "B", fatalToBatch_);
//         
//         assert(place == vch->size());
//     }
//     
//     static Error* Error::consumeFromBuf(unsigned char error, unsigned int cost, unsigned char **ptrPtr) {
//         bool fatalToBatch;
//         ptrPtr += unpack(ptrPtr, "B", &fatalToBatch);
//         
//         if (error == ERRORTYPECHAR_INVALID_TARGET) {
//             return InvalidTarget(
//         }
//     }
    
}//namespace commands::errors

}//namespace commands
