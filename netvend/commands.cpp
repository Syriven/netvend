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
        if (typeChar == COMMANDTYPECHAR_POCKET_TRANSFER) {
            return commands::PocketTransfer::consumeFromBuf(ptrPtr);
        }
        if (typeChar == COMMANDTYPECHAR_CREATE_FILE) {
            return commands::CreateFile::consumeFromBuf(ptrPtr);
        }
        if (typeChar == COMMANDTYPECHAR_UPDATE_FILE_BY_ID) {
            return commands::UpdateFileByID::consumeFromBuf(ptrPtr);
        }
        if (typeChar == COMMANDTYPECHAR_READ_FILE_BY_ID) {
            return commands::ReadFileByID::consumeFromBuf(ptrPtr);
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
    
    
    
    PocketTransfer::PocketTransfer(unsigned long fromPocketID, unsigned long toPocketID, unsigned long long amount)
    : Command(COMMANDTYPECHAR_POCKET_TRANSFER), fromPocketID_(fromPocketID), toPocketID_(toPocketID), amount_(amount)
    {}
    
    void PocketTransfer::writeToVch(std::vector<unsigned char>* vch) {
        Command::writeToVch(vch);
        
        static const size_t DATA_SIZE = PACK_L_SIZE*2 + PACK_Q_SIZE;
        
        unsigned int place = vch->size();
        
        vch->resize(place + DATA_SIZE);
        place += pack(vch->data()+place, "LLQ", fromPocketID_, toPocketID_, amount_);
        assert(place == vch->size());
    }
    
    commands::PocketTransfer* PocketTransfer::consumeFromBuf(unsigned char **ptrPtr) {
        unsigned long fromPocketID, toPocketID;
        unsigned long long amount;
        *ptrPtr += unpack(*ptrPtr, "LLQ", &fromPocketID, &toPocketID, &amount);
        
        commands::PocketTransfer* command = new PocketTransfer(fromPocketID, toPocketID, amount);
        
        return command;
    }
    
    unsigned long PocketTransfer::fromPocketID() {
        return fromPocketID_;
    }
    
    unsigned long PocketTransfer::toPocketID() {
        return toPocketID_;
    }
    
    unsigned long long PocketTransfer::amount() {
        return amount_;
    }
    
    
    
    CreateFile::CreateFile(std::string name, unsigned long pocketID)
    : Command(COMMANDTYPECHAR_CREATE_FILE), name_(name), pocketID_(pocketID)
    {}
    
    void CreateFile::writeToVch(std::vector<unsigned char>* vch) {
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
    
    commands::CreateFile* CreateFile::consumeFromBuf(unsigned char **ptrPtr) {
        unsigned char nameSize;
        *ptrPtr += unpack(*ptrPtr, "C", &nameSize);
        
        std::string name;
        name.resize(nameSize);
        std::copy_n(*ptrPtr, nameSize, name.begin());
        *ptrPtr += nameSize;
        
        unsigned long pocketID;
        *ptrPtr += unpack(*ptrPtr, "L", &pocketID);
        
        return new commands::CreateFile(name, pocketID);
    }
    
    std::string CreateFile::name() {return name_;}
    unsigned long CreateFile::pocketID() {return pocketID_;}
    
    
    
    UpdateFileByID::UpdateFileByID(unsigned long fileID, unsigned char* data, unsigned short dataSize)
    : Command(COMMANDTYPECHAR_UPDATE_FILE_BY_ID), fileID_(fileID), data_(data), dataSize_(dataSize)
    {
        mustFreeData_ = false;
    }
    
    UpdateFileByID::~UpdateFileByID() {
        if (mustFreeData_) {
            delete [] data_;
        }
    }
    
    void UpdateFileByID::allocSpace() {
        data_ = new unsigned char[(int)dataSize_];
        mustFreeData_ = true;
    }
    
    void UpdateFileByID::writeToVch(std::vector<unsigned char>* vch) {
        Command::writeToVch(vch);
        
        const size_t DATA_SIZE = PACK_L_SIZE + PACK_H_SIZE + dataSize_;
        
        unsigned int place = vch->size();
        
        vch->resize(place + DATA_SIZE);
        
        place += pack(vch->data()+place, "LH", fileID_, dataSize_);
        
        std::copy_n(data_, dataSize_, vch->data()+place);
        place += dataSize_;
        
        assert(place == vch->size());
    }
    
    commands::UpdateFileByID* UpdateFileByID::consumeFromBuf(unsigned char **ptrPtr) {
        unsigned long fileID;
        unsigned short dataSize;
        *ptrPtr += unpack(*ptrPtr, "LH", &fileID, &dataSize);
        
        commands::UpdateFileByID* newWriteCmd = new UpdateFileByID(fileID, NULL, dataSize);
        newWriteCmd->allocSpace();
        std::copy_n(*ptrPtr, dataSize, newWriteCmd->data());
        *ptrPtr += dataSize;
        
        return newWriteCmd;
    }
    
    unsigned long UpdateFileByID::fileID() {return fileID_;}
    unsigned char* UpdateFileByID::data() {return data_;}
    unsigned short UpdateFileByID::dataSize() {return dataSize_;}
    
    
    
    ReadFileByID::ReadFileByID(unsigned long fileID)
    : Command(COMMANDTYPECHAR_READ_FILE_BY_ID), fileID_(fileID)
    {}
    
    void ReadFileByID::writeToVch(std::vector<unsigned char>* vch) {
        Command::writeToVch(vch);
        
        const size_t DATA_SIZE = PACK_L_SIZE;
        
        unsigned int place = vch->size();
        
        vch->resize(place + DATA_SIZE);
        
        place += pack(vch->data()+place, "L", fileID_);
        
        assert(place == vch->size());
    }
    
    commands::ReadFileByID* ReadFileByID::consumeFromBuf(unsigned char **ptrPtr) {
        unsigned long fileID;
        *ptrPtr += unpack(*ptrPtr, "L", &fileID);
        
        commands::ReadFileByID* newReadCmd = new ReadFileByID(fileID);
        
        return newReadCmd;
    }
    
    unsigned long ReadFileByID::fileID() {return fileID_;}

namespace results {

    Result::Result(unsigned char error, unsigned long long cost)
    : error_(error), cost_(cost)
    {}
    
    void Result::writeToVch(std::vector<unsigned char>* vch) {
        static const size_t DATA_SIZE = PACK_C_SIZE + PACK_Q_SIZE;
        
        unsigned int place = vch->size();
        
        vch->resize(place + DATA_SIZE);
        place += pack(vch->data()+place, "CQ", error_, cost_);
        assert(place == vch->size());
    }

    Result* Result::consumeFromBuf(unsigned char** ptrPtr, unsigned char commandType) {
        unsigned char errorType;
        unsigned long long cost;
        *ptrPtr += unpack(*ptrPtr, "CQ", &errorType, &cost);
        
        if (errorType) {
            return errors::Error::consumeFromBuf(errorType, cost, ptrPtr);
        }
        
        else if (commandType == commands::COMMANDTYPECHAR_CREATE_POCKET) {
            return results::CreatePocket::consumeFromBuf(cost, ptrPtr);
        }
        else if (commandType == commands::COMMANDTYPECHAR_REQUEST_POCKET_DEPOSIT_ADDRESS) {
            return results::RequestPocketDepositAddress::consumeFromBuf(cost, ptrPtr);
        }
        else if (commandType == commands::COMMANDTYPECHAR_POCKET_TRANSFER) {
            return results::PocketTransfer::consumeFromBuf(cost, ptrPtr);
        }
        else if (commandType == commands::COMMANDTYPECHAR_CREATE_FILE) {
            return results::CreateFile::consumeFromBuf(cost, ptrPtr);
        }
        else if (commandType == commands::COMMANDTYPECHAR_UPDATE_FILE_BY_ID) {
            return results::UpdateFileByID::consumeFromBuf(cost, ptrPtr);
        }
        else if (commandType == commands::COMMANDTYPECHAR_READ_FILE_BY_ID) {
            return results::ReadFileByID::consumeFromBuf(cost, ptrPtr);
        }
        
        else {
            throw std::runtime_error("bad response; commandTypeChar " + boost::lexical_cast<std::string>(commandType) + " unrecognized.");
        }
        return NULL;
    }

    unsigned long long Result::cost() {
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
    
    unsigned long long Batch::cost() {
        return cost_;
    }

    
    
    CreatePocket::CreatePocket(unsigned long long cost, unsigned long pocketID)
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

    results::CreatePocket* CreatePocket::consumeFromBuf(unsigned long long cost, unsigned char **ptrPtr) {
        unsigned long pocketID;
        *ptrPtr += unpack(*ptrPtr, "L", &pocketID);
        
        return new results::CreatePocket(cost, pocketID);
    }

    unsigned long CreatePocket::pocketID() {
        return pocketID_;
    }
    
    
    
    RequestPocketDepositAddress::RequestPocketDepositAddress(unsigned long long cost, std::string depositAddress)
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
    
    results::RequestPocketDepositAddress* RequestPocketDepositAddress::consumeFromBuf(unsigned long long cost, unsigned char **ptrPtr) {
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
    
    
    
    PocketTransfer::PocketTransfer(unsigned long long cost)
    : Result(errors::ERRORTYPECHAR_NONE, cost)
    {}
    
    void PocketTransfer::writeToVch(std::vector<unsigned char>* vch) {
        Result::writeToVch(vch);
    }
    
    results::PocketTransfer* PocketTransfer::consumeFromBuf(unsigned long long cost, unsigned char **ptrPtr) {
        return new results::PocketTransfer(cost);
    }
    
    
    
    CreateFile::CreateFile(unsigned long long cost, unsigned long fileID)
    : Result(errors::ERRORTYPECHAR_NONE, cost), fileID_(fileID)
    {}
    
    void CreateFile::writeToVch(std::vector<unsigned char>* vch) {
        Result::writeToVch(vch);
        
        static const size_t DATA_SIZE = PACK_L_SIZE;
        
        unsigned long place = vch->size();
        
        vch->resize(place + DATA_SIZE);
        place += pack(vch->data()+place, "L", fileID_);
        
        assert(place == vch->size());
    }
    
    results::CreateFile* CreateFile::consumeFromBuf(unsigned long long cost, unsigned char **ptrPtr) {
        unsigned long fileID;
        *ptrPtr += unpack(*ptrPtr, "L", &fileID);
        
        return new results::CreateFile(cost, fileID);
    }
    
    unsigned long CreateFile::fileID() {return fileID_;}
    
    
    
    UpdateFileByID::UpdateFileByID(unsigned long long cost)
    : Result(errors::ERRORTYPECHAR_NONE, cost)
    {}
    
    void UpdateFileByID::writeToVch(std::vector<unsigned char>* vch) {
        Result::writeToVch(vch);
    }
    
    results::UpdateFileByID* UpdateFileByID::consumeFromBuf(unsigned long long cost, unsigned char **ptrPtr) {
        return new results::UpdateFileByID(cost);
    }
    
    
    
    ReadFileByID::ReadFileByID(unsigned long long cost, std::vector<unsigned char> fileData)
    : Result(errors::ERRORTYPECHAR_NONE, cost), fileData_(fileData)
    {}
    
    void ReadFileByID::writeToVch(std::vector<unsigned char>* vch) {
        Result::writeToVch(vch);
        
        unsigned short fileDataSize = fileData_.size();
        
        const size_t DATA_SIZE = PACK_H_SIZE + fileDataSize;
        
        unsigned int place = vch->size();
        
        vch->resize(place + DATA_SIZE);
        
        place += pack(vch->data()+place, "H", fileDataSize);
        
        std::copy(fileData_.begin(), fileData_.end(), vch->data()+place);
        place += fileDataSize;
        
        assert(place == vch->size());
    }
    
    results::ReadFileByID* ReadFileByID::consumeFromBuf(unsigned long long cost, unsigned char **ptrPtr) {
        unsigned short fileDataSize;
        *ptrPtr += unpack(*ptrPtr, "H", &fileDataSize);
        
        std::vector<unsigned char> fileData;
        fileData.resize(fileDataSize);
        std::copy_n(*ptrPtr, fileDataSize, fileData.begin());
        *ptrPtr += fileDataSize;
        
        return new results::ReadFileByID(cost, fileData);
    }
    
    std::vector<unsigned char>* ReadFileByID::fileData() {
        return &fileData_;
    }

}//namesace commands::results

namespace errors {

    Error::Error(unsigned char errorType, unsigned long long cost, bool fatalToBatch)
    : results::Result(errorType, cost), std::runtime_error("Netvend agent command error"), fatalToBatch_(fatalToBatch)
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
    
    Error* Error::consumeFromBuf(unsigned char errorType, unsigned long long cost, unsigned char **ptrPtr) {
        bool fatalToBatch;
        *ptrPtr += unpack(*ptrPtr, "B", &fatalToBatch);
        
        if (errorType == ERRORTYPECHAR_SERVER_LOGIC) {
            return ServerLogicError::consumeFromBuf(cost, fatalToBatch, ptrPtr);
        }
        else if (errorType == ERRORTYPECHAR_INVALID_TARGET) {
            return InvalidTargetError::consumeFromBuf(cost, fatalToBatch, ptrPtr);
        }
        else if (errorType == ERRORTYPECHAR_TARGET_NOT_OWNED) {
            return TargetNotOwnedError::consumeFromBuf(cost, fatalToBatch, ptrPtr);
        }
        else if (errorType == ERRORTYPECHAR_CREDIT_INSUFFICIENT) {
            return CreditInsufficientError::consumeFromBuf(cost, fatalToBatch, ptrPtr);
        }
        else if (errorType == ERRORTYPECHAR_CREDIT_OVERFLOW) {
            return CreditOverflowError::consumeFromBuf(cost, fatalToBatch, ptrPtr);
        }
        else {
            throw std::runtime_error("bad response; errorTypeChar " +  boost::lexical_cast<std::string>(errorType) + " unrecognized.");
        }
        return NULL;
    }
    
    bool Error::fatalToBatch() {
        return fatalToBatch_;
    }
    
    const char* Error::what() const noexcept {
        return what_.c_str();
    }
    
    
    
    ServerLogicError::ServerLogicError(std::string errorString, unsigned long long cost, bool fatalToBatch)
    : Error(ERRORTYPECHAR_SERVER_LOGIC, cost, fatalToBatch), errorString_(errorString)
    {
        setWhat();
    }
    
    void ServerLogicError::setWhat() {
        what_ = std::string("Server logic error: ") + errorString_;
    }
    
    void ServerLogicError::writeToVch(std::vector<unsigned char>* vch) {
        Error::writeToVch(vch);
        
        unsigned char errorStringSize = errorString_.size();
        const size_t DATA_SIZE = PACK_C_SIZE + errorStringSize;
        
        unsigned int place = vch->size();
        
        vch->resize(place + DATA_SIZE);
        place += pack(vch->data()+place, "C", errorStringSize);
        std::copy(errorString_.begin(), errorString_.end(), vch->begin()+place);
        place += errorStringSize;
        
        assert(place == vch->size());
    }
    
    ServerLogicError* ServerLogicError::consumeFromBuf(unsigned long long cost, bool fatalToBatch, unsigned char **ptrPtr) {
        unsigned char errorStringSize;
        *ptrPtr += unpack(*ptrPtr, "C", &errorStringSize);
        
        std::string errorString;
        errorString.resize(errorStringSize);
        std::copy_n(*ptrPtr, errorStringSize, errorString.begin());
        *ptrPtr += errorStringSize;
        
        return new ServerLogicError(errorString, cost, fatalToBatch);
    }
    
    std::string ServerLogicError::errorString() {
        return errorString_;
    }
    
    
    
    InvalidTargetError::InvalidTargetError(std::string target, unsigned long long cost, bool fatalToBatch)
    : Error(ERRORTYPECHAR_INVALID_TARGET, cost, fatalToBatch), target_(target)
    {
        setWhat();
    }
    
    void InvalidTargetError::setWhat() {
        what_ = std::string("Invalid target '") + target_ + std::string("'");
    }
    
    void InvalidTargetError::writeToVch(std::vector<unsigned char>* vch) {
        Error::writeToVch(vch);
        
        unsigned char targetSize = target_.size();
        const size_t DATA_SIZE = PACK_C_SIZE + targetSize;
        
        unsigned int place = vch->size();
        
        vch->resize(place + DATA_SIZE);
        place += pack(vch->data()+place, "C", targetSize);
        std::copy(target_.begin(), target_.end(), vch->begin()+place);
        place += targetSize;
        
        assert(place == vch->size());
    }
    
    InvalidTargetError* InvalidTargetError::consumeFromBuf(unsigned long long cost, bool fatalToBatch, unsigned char **ptrPtr) {
        unsigned char targetSize;
        *ptrPtr += unpack(*ptrPtr, "C", &targetSize);
        
        std::string target;
        target.resize(targetSize);
        std::copy_n(*ptrPtr, targetSize, target.begin());
        *ptrPtr += targetSize;
        
        return new InvalidTargetError(target, cost, fatalToBatch);
    }
    
    std::string InvalidTargetError::target() {
        return target_;
    }
    
    
    
    TargetNotOwnedError::TargetNotOwnedError(std::string target, unsigned long long cost, bool fatalToBatch)
    : Error(ERRORTYPECHAR_TARGET_NOT_OWNED, cost, fatalToBatch), target_(target)
    {
        setWhat();
    }
    
    void TargetNotOwnedError::setWhat() {
        what_ = std::string("Target '") + target_ + std::string("' is not owned by this agent.");
    }
    
    void TargetNotOwnedError::writeToVch(std::vector<unsigned char>* vch) {
        Error::writeToVch(vch);
        
        unsigned char targetSize = target_.size();
        const size_t DATA_SIZE = PACK_C_SIZE + targetSize;
        
        unsigned int place = vch->size();
        
        vch->resize(place + DATA_SIZE);
        place += pack(vch->data()+place, "C", targetSize);
        std::copy(target_.begin(), target_.end(), vch->begin()+place);
        
        assert(place+targetSize == vch->size());
    }
    
    TargetNotOwnedError* TargetNotOwnedError::consumeFromBuf(unsigned long long cost, bool fatalToBatch, unsigned char **ptrPtr) {
        unsigned char targetSize;
        *ptrPtr += unpack(*ptrPtr, "C", &targetSize);
        
        std::string target;
        target.resize(targetSize);
        std::copy_n(*ptrPtr, targetSize, target.begin());
        *ptrPtr += targetSize;
        
        return new TargetNotOwnedError(target, cost, fatalToBatch);
    }
    
    std::string TargetNotOwnedError::target() {
        return target_;
    }
    
    
    
    CreditInsufficientError::CreditInsufficientError(unsigned long long requiredCredit, unsigned long long availableCredit, unsigned long long cost, bool fatalToBatch)
    : Error(ERRORTYPECHAR_CREDIT_INSUFFICIENT, cost, fatalToBatch), requiredCredit_(requiredCredit), availableCredit_(availableCredit)
    {
        setWhat();
    }
    
    void CreditInsufficientError::setWhat() {
        what_ = std::string("Insufficient funds. Need ") + boost::lexical_cast<std::string>(requiredCredit_) + std::string("; only ") + boost::lexical_cast<std::string>(availableCredit_) + std::string(" in pocket.");
    }
    
    void CreditInsufficientError::writeToVch(std::vector<unsigned char>* vch) {
        Error::writeToVch(vch);
        
        static const size_t DATA_SIZE = PACK_Q_SIZE*2;
        
        unsigned long place = vch->size();
        
        vch->resize(place + DATA_SIZE);
        place += pack(vch->data()+place, "QQ", requiredCredit_, availableCredit_);
        
        assert(place == vch->size());
    }
    
    errors::CreditInsufficientError* CreditInsufficientError::consumeFromBuf(unsigned long long cost, bool fatalToBatch, unsigned char **ptrPtr) {
        unsigned long long requiredCredit, availableCredit;
        
        *ptrPtr += unpack(*ptrPtr, "QQ", &requiredCredit, &availableCredit);
        
        return new errors::CreditInsufficientError(requiredCredit, availableCredit, cost, fatalToBatch);
    }
    
    unsigned long long CreditInsufficientError::requiredCredit() {
        return requiredCredit_;
    }
    
    unsigned long long CreditInsufficientError::availableCredit() {
        return availableCredit_;
    }
    
    unsigned long long CreditInsufficientError::creditMissing() {
        return requiredCredit_ - availableCredit_;
    }
    
    
    
    CreditOverflowError::CreditOverflowError(unsigned long long pocketCredit, unsigned long long addedCredit, unsigned long long cost, bool fatalToBatch)
    : Error(ERRORTYPECHAR_CREDIT_OVERFLOW, cost, fatalToBatch), pocketCredit_(pocketCredit), addedCredit_(addedCredit)
    {
        setWhat();
    }
    
    void CreditOverflowError::setWhat() {
        what_ = std::string("Pocket credit overflow. Pocket has ") + boost::lexical_cast<std::string>(pocketCredit_) + std::string("credit; not enough room for ") + boost::lexical_cast<std::string>(addedCredit_) + std::string(" more.");
    }
    
    void CreditOverflowError::writeToVch(std::vector<unsigned char>* vch) {
        Error::writeToVch(vch);
        
        static const size_t DATA_SIZE = PACK_Q_SIZE*2;
        
        unsigned long place = vch->size();
        
        vch->resize(place + DATA_SIZE);
        place += pack(vch->data()+place, "QQ", pocketCredit_, addedCredit_);
        
        assert(place == vch->size());
    }
    
    errors::CreditOverflowError* CreditOverflowError::consumeFromBuf(unsigned long long cost, bool fatalToBatch, unsigned char **ptrPtr) {
        unsigned long long pocketCredit, addedCredit;
        
        *ptrPtr += unpack(*ptrPtr, "QQ", &pocketCredit, &addedCredit);
        
        return new errors::CreditOverflowError(pocketCredit, addedCredit, cost, fatalToBatch);
    }
    
    unsigned long long CreditOverflowError::pocketCredit() {
        return pocketCredit_;
    }
    
    unsigned long long CreditOverflowError::addedCredit() {
        return addedCredit_;
    }
    
    unsigned long long CreditOverflowError::totalCredit() {
        return (unsigned long long)pocketCredit_ + (unsigned long long)addedCredit_;
    }
    
}//namespace commands::errors

}//namespace commands
