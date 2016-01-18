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
        if (typeChar == COMMANDTYPECHAR_CREATE_PAGE) {
            return commands::CreatePage::consumeFromBuf(ptrPtr);
        }
        if (typeChar == COMMANDTYPECHAR_UPDATE_PAGE_BY_ID) {
            return commands::UpdatePageByID::consumeFromBuf(ptrPtr);
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
    
    

    CreatePocket::CreatePocket()
    : Command(COMMANDTYPECHAR_CREATE_POCKET)
    {}

    void CreatePocket::writeToVch(std::vector<unsigned char>* vch) {
        Command::writeToVch(vch);
    }

    commands::CreatePocket* CreatePocket::consumeFromBuf(unsigned char **ptrPtr) {
        commands::CreatePocket* command = new CreatePocket();
        
        return command;
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
    
    
    
    CreatePage::CreatePage(std::string name, unsigned long pocketID)
    : Command(COMMANDTYPECHAR_CREATE_PAGE), name_(name), pocketID_(pocketID)
    {}
    
    void CreatePage::writeToVch(std::vector<unsigned char>* vch) {
        Command::writeToVch(vch);
        
        unsigned char nameSize = name_.size();
        
        const size_t DATA_SIZE = PACK_C_SIZE + nameSize + PACK_L_SIZE;
        
        unsigned int place = vch->size();
        
        vch->resize(place + DATA_SIZE);
        
        place += pack(vch->data()+place, "C", nameSize);
        
        std::copy(name_.begin(), name_.end(), vch->data()+place);
        place += nameSize;
        
        place += pack(vch->data()+place, "L", pocketID_);
        
        assert(place == vch->size());
    }
    
    commands::CreatePage* CreatePage::consumeFromBuf(unsigned char **ptrPtr) {
        unsigned char nameSize;
        *ptrPtr += unpack(*ptrPtr, "C", &nameSize);
        
        std::string name;
        name.resize(nameSize);
        std::copy_n(*ptrPtr, nameSize, name.begin());
        *ptrPtr += nameSize;
        
        unsigned long pocketID;
        *ptrPtr += unpack(*ptrPtr, "L", &pocketID);
        
        return new commands::CreatePage(name, pocketID);
    }
    
    std::string CreatePage::name() {return name_;}
    unsigned long CreatePage::pocketID() {return pocketID_;}
    
    
    
    UpdatePageByID::UpdatePageByID(unsigned long pageID, unsigned char* data, unsigned short dataSize)
    : Command(COMMANDTYPECHAR_UPDATE_PAGE_BY_ID), pageID_(pageID), data_(data), dataSize_(dataSize)
    {
        mustFreeData_ = false;
    }
    
    UpdatePageByID::~UpdatePageByID() {
        if (mustFreeData_) {
            delete [] data_;
        }
    }
    
    void UpdatePageByID::allocSpace() {
        data_ = new unsigned char[(int)dataSize_];
        mustFreeData_ = true;
    }
    
    void UpdatePageByID::writeToVch(std::vector<unsigned char>* vch) {
        Command::writeToVch(vch);
        
        const size_t DATA_SIZE = PACK_L_SIZE + PACK_H_SIZE + dataSize_;
        
        unsigned int place = vch->size();
        
        vch->resize(place + DATA_SIZE);
        
        place += pack(vch->data()+place, "LH", pageID_, dataSize_);
        
        std::copy_n(data_, dataSize_, vch->data()+place);
        place += dataSize_;
        
        assert(place == vch->size());
    }
    
    commands::UpdatePageByID* UpdatePageByID::consumeFromBuf(unsigned char **ptrPtr) {
        unsigned long pageID;
        unsigned short dataSize;
        *ptrPtr += unpack(*ptrPtr, "LH", &pageID, &dataSize);
        
        commands::UpdatePageByID* newWriteCmd = new UpdatePageByID(pageID, NULL, dataSize);
        newWriteCmd->allocSpace();
        std::copy_n(*ptrPtr, dataSize, newWriteCmd->data());
        *ptrPtr += dataSize;
        
        return newWriteCmd;
    }
    
    unsigned long UpdatePageByID::pageID() {return pageID_;}
    unsigned char* UpdatePageByID::data() {return data_;}
    unsigned short UpdatePageByID::dataSize() {return dataSize_;}

namespace results {

    Result::Result(unsigned char error, unsigned long cost)
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
        unsigned long cost;
        *ptrPtr += unpack(*ptrPtr, "CL", &error, &cost);
        
        if (error) {
            return errors::Error::consumeFromBuf(error, cost, ptrPtr);
        }
        
        else if (commandType == commands::COMMANDTYPECHAR_CREATE_POCKET) {
            return results::CreatePocket::consumeFromBuf(cost, ptrPtr);
        }
        else if (commandType == commands::COMMANDTYPECHAR_REQUEST_POCKET_DEPOSIT_ADDRESS) {
            return results::RequestPocketDepositAddress::consumeFromBuf(cost, ptrPtr);
        }
        else if (commandType == commands::COMMANDTYPECHAR_CREATE_PAGE) {
            return results::CreatePage::consumeFromBuf(cost, ptrPtr);
        }
        else if (commandType == commands::COMMANDTYPECHAR_UPDATE_PAGE_BY_ID) {
            return results::UpdatePageByID::consumeFromBuf(cost, ptrPtr);
        }
        
        else {
            throw std::runtime_error("bad response; commandTypeChar " + boost::lexical_cast<std::string>(commandType) + " unrecognized.");
        }
        return NULL;
    }

    unsigned long Result::cost() {
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
    
    void Batch::consumeFromBuf(unsigned char **ptrPtr) {
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
    
    unsigned long Batch::cost() {
        return cost_;
    }

    
    
    CreatePocket::CreatePocket(unsigned long cost, unsigned long pocketID)
    : Result(errors::ERRORTYPECHAR_NONE, cost), pocketID_(pocketID)
    {}

    void CreatePocket::writeToVch(std::vector<unsigned char>* vch) {
        Result::writeToVch(vch);
        
        static const size_t DATA_SIZE = PACK_L_SIZE;
        
        unsigned int place = vch->size();
        
        vch->resize(place + DATA_SIZE);
        place += pack(vch->data() + place, "L", pocketID_);
        assert(place == vch->size());
    }

    results::CreatePocket* CreatePocket::consumeFromBuf(unsigned long cost, unsigned char **ptrPtr) {
        unsigned long pocketID;
        *ptrPtr += unpack(*ptrPtr, "L", &pocketID);
        
        return new results::CreatePocket(cost, pocketID);
    }

    unsigned long CreatePocket::pocketID() {
        return pocketID_;
    }
    
    
    
    RequestPocketDepositAddress::RequestPocketDepositAddress(unsigned long cost, std::string depositAddress)
    : Result(errors::ERRORTYPECHAR_NONE, cost), depositAddress_(depositAddress)
    {}
    
    void RequestPocketDepositAddress::writeToVch(std::vector<unsigned char>* vch) {
        Result::writeToVch(vch);
        
        static const size_t DATA_SIZE = MAX_ADDRESS_SIZE;
        
        unsigned long place = vch->size();
        
        vch->resize(place + DATA_SIZE);
        memset(vch->data()+place, '\0', DATA_SIZE);
        std::copy(depositAddress_.begin(), depositAddress_.end(), vch->begin()+place);
    }
    
    results::RequestPocketDepositAddress* RequestPocketDepositAddress::consumeFromBuf(unsigned long cost, unsigned char **ptrPtr) {
        unsigned char buf[MAX_ADDRESS_SIZE + 1];
        memset(buf, '\0', MAX_ADDRESS_SIZE + 1);
        
        std::copy_n(*ptrPtr, MAX_ADDRESS_SIZE, buf);
        *ptrPtr += MAX_ADDRESS_SIZE;
        
        //this will interpret the first \0 as the end, so
        //an address less than 34 chars will result in the correct length string
        std::string depositAddress((char*)buf);
        
        return new results::RequestPocketDepositAddress(cost, depositAddress);
    }
    
    std::string RequestPocketDepositAddress::depositAddress() {
        return depositAddress_;
    }
    
    
    
    CreatePage::CreatePage(unsigned long cost, unsigned long pageID)
    : Result(errors::ERRORTYPECHAR_NONE, cost), pageID_(pageID)
    {}
    
    void CreatePage::writeToVch(std::vector<unsigned char>* vch) {
        Result::writeToVch(vch);
        
        static const size_t DATA_SIZE = PACK_L_SIZE;
        
        unsigned long place = vch->size();
        
        vch->resize(place + DATA_SIZE);
        place += pack(vch->data() + place, "L", pageID_);
        assert(place == vch->size());
    }
    
    results::CreatePage* CreatePage::consumeFromBuf(unsigned long cost, unsigned char **ptrPtr) {
        unsigned long pageID;
        *ptrPtr += unpack(*ptrPtr, "L", &pageID);
        
        return new results::CreatePage(cost, pageID);
    }
    
    unsigned long CreatePage::pageID() {return pageID_;}
    
    
    
    UpdatePageByID::UpdatePageByID(unsigned long cost)
    : Result(errors::ERRORTYPECHAR_NONE, cost)
    {}
    
    void UpdatePageByID::writeToVch(std::vector<unsigned char>* vch) {
        Result::writeToVch(vch);
    }
    
    results::UpdatePageByID* UpdatePageByID::consumeFromBuf(unsigned long cost, unsigned char **ptrPtr) {
        return new results::UpdatePageByID(cost);
    }

}//namesace commands::results

namespace errors {

    Error::Error(unsigned char error, unsigned long cost, bool fatalToBatch)
    : results::Result(error, cost), std::runtime_error("Netvend agent command error"), fatalToBatch_(fatalToBatch)
    {
        what_ = "what_ member not set";
    }
    
    void Error::writeToVch(std::vector<unsigned char>* vch) {
        results::Result::writeToVch(vch);
        
        static const size_t DATA_SIZE = PACK_B_SIZE;
        
        unsigned int place = vch->size();
        
        vch->resize(place + DATA_SIZE);
        place += pack(vch->data()+place, "B", fatalToBatch_);
        
        assert(place == vch->size());
    }
    
    Error* Error::consumeFromBuf(unsigned char error, unsigned long cost, unsigned char **ptrPtr) {
        bool fatalToBatch;
        *ptrPtr += unpack(*ptrPtr, "B", &fatalToBatch);
        
        if (error == ERRORTYPECHAR_INVALID_TARGET) {
            return InvalidTarget::consumeFromBuf(cost, fatalToBatch, ptrPtr);
        }
        else if (error == ERRORTYPECHAR_TARGET_NOT_OWNED) {
            return TargetNotOwned::consumeFromBuf(cost, fatalToBatch, ptrPtr);
        }
        else {
            throw std::runtime_error("bad response; errorTypeChar " +  boost::lexical_cast<std::string>(error) + " unrecognized.");
        }
        return NULL;
    }
    
    bool Error::fatalToBatch() {
        return fatalToBatch_;
    }
    
    const char* Error::what() const noexcept {
        return what_.c_str();
    }
    
    
    
    InvalidTarget::InvalidTarget(std::string target, unsigned long cost, bool fatalToBatch)
    : Error(ERRORTYPECHAR_INVALID_TARGET, cost, fatalToBatch), target_(target)
    {
        setWhat();
    }
    
    void InvalidTarget::setWhat() {
        what_ = std::string("Invalid target '") + target_ + std::string("'");
    }
    
    void InvalidTarget::writeToVch(std::vector<unsigned char>* vch) {
        Error::writeToVch(vch);
        
        unsigned char targetSize = target_.size();
        const size_t DATA_SIZE = PACK_C_SIZE + targetSize;
        
        unsigned int place = vch->size();
        
        vch->resize(place + DATA_SIZE);
        place += pack(vch->data()+place, "C", targetSize);
        std::copy(target_.begin(), target_.end(), vch->begin()+place);
        
        assert(place+targetSize == vch->size());
    }
    
    InvalidTarget* InvalidTarget::consumeFromBuf(unsigned long cost, bool fatalToBatch, unsigned char **ptrPtr) {
        unsigned char targetSize;
        *ptrPtr += unpack(*ptrPtr, "C", &targetSize);
        
        std::string target;
        target.resize(targetSize);
        std::copy_n(*ptrPtr, targetSize, target.begin());
        *ptrPtr += targetSize;
        
        return new InvalidTarget(target, cost, fatalToBatch);
    }
    
    std::string InvalidTarget::target() {
        return target_;
    }
    
    
    
    TargetNotOwned::TargetNotOwned(std::string target, unsigned long cost, bool fatalToBatch)
    : Error(ERRORTYPECHAR_TARGET_NOT_OWNED, cost, fatalToBatch), target_(target)
    {
        setWhat();
    }
    
    void TargetNotOwned::setWhat() {
        what_ = std::string("Target '") + target_ + std::string("' is not owned by this agent.");
    }
    
    void TargetNotOwned::writeToVch(std::vector<unsigned char>* vch) {
        Error::writeToVch(vch);
        
        unsigned char targetSize = target_.size();
        const size_t DATA_SIZE = PACK_C_SIZE + targetSize;
        
        unsigned int place = vch->size();
        
        vch->resize(place + DATA_SIZE);
        place += pack(vch->data()+place, "C", targetSize);
        std::copy(target_.begin(), target_.end(), vch->begin()+place);
        
        assert(place+targetSize == vch->size());
    }
    
    TargetNotOwned* TargetNotOwned::consumeFromBuf(unsigned long cost, bool fatalToBatch, unsigned char **ptrPtr) {
        unsigned char targetSize;
        *ptrPtr += unpack(*ptrPtr, "C", &targetSize);
        
        std::string target;
        target.resize(targetSize);
        std::copy_n(*ptrPtr, targetSize, target.begin());
        *ptrPtr += targetSize;
        
        return new TargetNotOwned(target, cost, fatalToBatch);
    }
    
    std::string TargetNotOwned::target() {
        return target_;
    }
    
}//namespace commands::errors

}//namespace commands
