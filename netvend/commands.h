#ifndef NETVEND_NV_COMMANDS_H
#define NETVEND_NV_COMMANDS_H

#include <boost/shared_ptr.hpp>
#include <boost/lexical_cast.hpp>
#include <string>
#include <iostream>

#include "netvend/common_constants.h"
#include "util/pack.h"

namespace commands {

    const unsigned char COMMANDBATCH_COMPLETION_NONE = 0;
    const unsigned char COMMANDBATCH_COMPLETION_SOME = 1;
    const unsigned char COMMANDBATCH_COMPLETION_ALL = 2;
    
    const char COMMANDTYPECHAR_CREATE_POCKET = 0;
    const char COMMANDTYPECHAR_REQUEST_POCKET_DEPOSIT_ADDRESS = 1;
    const char COMMANDTYPECHAR_POCKET_TRANSFER = 2;
    const char COMMANDTYPECHAR_CREATE_FILE = 3;
    const char COMMANDTYPECHAR_UPDATE_FILE_BY_ID = 4;
    const char COMMANDTYPECHAR_READ_FILE_BY_ID = 5;

    class Command {
        unsigned char typeChar_;
    public:
        Command(unsigned char typeChar);
        virtual void writeToVch(std::vector<unsigned char>* vch);
        static Command* consumeFromBuf(unsigned char **ptrPtr);
        unsigned char typeChar();
    };
    
    class Batch {
        std::vector<boost::shared_ptr<Command> > commands_;
    public:
        Batch();
        virtual void writeToVch(std::vector<unsigned char>* vch);
        static commands::Batch* consumeFromBuf(unsigned char **ptrPtr);
        void addCommand(boost::shared_ptr<Command> command);
        std::vector<boost::shared_ptr<Command> >* commands();
    };

    class CreatePocket : public Command {
    public:
        CreatePocket();
        void writeToVch(std::vector<unsigned char>* vch);
        static commands::CreatePocket* consumeFromBuf(unsigned char **ptrPtr);
    };
    
    class RequestPocketDepositAddress : public Command {
        unsigned long pocketID_;
    public:
        RequestPocketDepositAddress(unsigned long pocketID);
        void writeToVch(std::vector<unsigned char>* vch);
        static commands::RequestPocketDepositAddress* consumeFromBuf(unsigned char **ptrPtr);
        unsigned long pocketID();
    };
    
    class PocketTransfer : public Command {
        unsigned long fromPocketID_;
        unsigned long toPocketID_;
        unsigned long amount_;
    public:
        PocketTransfer(unsigned long fromPocketID, unsigned long toPocketID, unsigned long amount);
        void writeToVch(std::vector<unsigned char>* vch);
        static commands::PocketTransfer* consumeFromBuf(unsigned char **ptrPtr);
        unsigned long fromPocketID();
        unsigned long toPocketID();
        unsigned long amount();
    };
    
    class CreateFile : public Command {
        std::string name_;
        unsigned long pocketID_;
    public:
        CreateFile(std::string name, unsigned long pocketID);
        void writeToVch(std::vector<unsigned char>* vch);
        static commands::CreateFile* consumeFromBuf(unsigned char **ptrPtr);
        std::string name();
        unsigned long pocketID();
    };
    
    class UpdateFileByID : public Command {
        unsigned long fileID_;
        unsigned char* data_;
        unsigned short dataSize_;
        bool mustFreeData_;
    public:
        UpdateFileByID(unsigned long fileID, unsigned char* data, unsigned short dataSize);
        ~UpdateFileByID();
        void allocSpace();
        void writeToVch(std::vector<unsigned char>* vch);
        static commands::UpdateFileByID* consumeFromBuf(unsigned char **ptrPtr);
        unsigned long fileID();
        unsigned char* data();
        unsigned short dataSize();
    };
    
    class ReadFileByID : public Command {
	unsigned long fileID_;
    public:
	ReadFileByID(unsigned long fileID);
	void writeToVch(std::vector<unsigned char>* vch);
	static commands::ReadFileByID* consumeFromBuf(unsigned char **ptrPtr);
	unsigned long fileID();
    };

namespace results {

    class Result {
        unsigned char error_;
        unsigned long cost_;
    public:
        Result(unsigned char error, unsigned long cost);
        virtual void writeToVch(std::vector<unsigned char>* vch);
        static Result* consumeFromBuf(unsigned char **ptrPtr, unsigned char commandType);
        unsigned long cost();
        bool error();
    };
    
    class Batch {
        commands::Batch* initiatingCommandBatch_;
        std::vector<boost::shared_ptr<Result> > results_;
        unsigned long cost_;
    public:
        Batch(commands::Batch* initiatingCommandBatch);
        void addResult(boost::shared_ptr<Result> result);
        void writeToVch(std::vector<unsigned char>* vch);
        void consumeFromBuf(unsigned char **ptrPtr);
        std::vector<boost::shared_ptr<Result> >* results();
        unsigned long cost();
    };

    class CreatePocket : public Result {
        unsigned long pocketID_;
    public:
        CreatePocket(unsigned long cost, unsigned long pocketID);
        void writeToVch(std::vector<unsigned char>* vch);
        static results::CreatePocket* consumeFromBuf(unsigned long cost, unsigned char **ptrPtr);
        unsigned long pocketID();
    };
    
    class RequestPocketDepositAddress : public Result {
        std::string depositAddress_;
    public:
        RequestPocketDepositAddress(unsigned long cost, std::string depositAddress);
        void writeToVch(std::vector<unsigned char>* vch);
        static results::RequestPocketDepositAddress* consumeFromBuf(unsigned long cost, unsigned char **ptrPtr);
        std::string depositAddress();
    };
    
    class PocketTransfer : public Result {
    public:
        PocketTransfer(unsigned long cost);
        void writeToVch(std::vector<unsigned char>* vch);
        static results::PocketTransfer* consumeFromBuf(unsigned long cost, unsigned char **ptrPtr);
    };
    
    class CreateFile : public Result {
        unsigned long fileID_;
    public:
        CreateFile(unsigned long cost, unsigned long fileID);
        void writeToVch(std::vector<unsigned char>* vch);
        static results::CreateFile* consumeFromBuf(unsigned long cost, unsigned char **ptrPtr);
        unsigned long fileID();
    };
    
    class UpdateFileByID : public Result {
    public:
        UpdateFileByID(unsigned long cost);
        void writeToVch(std::vector<unsigned char>* vch);
        static results::UpdateFileByID* consumeFromBuf(unsigned long cost, unsigned char **ptrPtr);
    };
    
    class ReadFileByID : public Result {
        std::vector<unsigned char> fileData_;
    public:
        ReadFileByID(unsigned long cost, std::vector<unsigned char> fileData);
        void writeToVch(std::vector<unsigned char>* vch);
        static results::ReadFileByID* consumeFromBuf(unsigned long cost, unsigned char **ptrPtr);
        std::vector<unsigned char>* fileData();
    };

}//namespace commands::results

namespace errors {
    
    const unsigned char ERRORTYPECHAR_NONE = 0;
    const unsigned char ERRORTYPECHAR_SERVER_LOGIC = 1;
    const unsigned char ERRORTYPECHAR_INVALID_TARGET = 2;
    const unsigned char ERRORTYPECHAR_TARGET_NOT_OWNED = 3;
    const unsigned char ERRORTYPECHAR_CREDIT_INSUFFICIENT = 4;
    const unsigned char ERRORTYPECHAR_CREDIT_OVERFLOW = 5;
    
    const unsigned char ERRORSUBTYPECHAR_NONE = 0;
    const unsigned char ERRORSUBTYPECHAR_AGENT = 1;
    const unsigned char ERRORSUBTYPECHAR_POCKET = 2;
    const unsigned char ERRORSUBTYPECHAR_FILE = 3;
    
    class Error : public results::Result, public std::runtime_error {
        bool fatalToBatch_;
    protected:
        std::string what_;
    public:
        Error(unsigned char errorType, unsigned long cost, bool fatalToBatch);
        void writeToVch(std::vector<unsigned char>* vch);
        static Error* consumeFromBuf(unsigned char errorType, unsigned long cost, unsigned char **ptrPtr);
        bool fatalToBatch();
        virtual ~Error() throw() {}
        const char* what() const noexcept;
    };
    
    class ServerLogicError : public Error {
        std::string errorString_;
    public:
        ServerLogicError(std::string errorString, unsigned long cost, bool fatalToBatch);
        void setWhat();
        void writeToVch(std::vector<unsigned char>* vch);
        static ServerLogicError* consumeFromBuf(unsigned long cost, bool fatalToBatch, unsigned char **ptrPtr);
        std::string errorString();
    };
    
    class InvalidTargetError : public Error {
        std::string target_;
    public:
        InvalidTargetError(std::string target, unsigned long cost, bool fatalToBatch);
        void setWhat();
        void writeToVch(std::vector<unsigned char>* vch);
        static InvalidTargetError* consumeFromBuf(unsigned long cost, bool fatalToBatch, unsigned char **ptrPtr);
        std::string target();
    };
    
    class TargetNotOwnedError : public Error {
        std::string target_;
    public:
        TargetNotOwnedError(std::string target, unsigned long cost, bool fatalToBatch);
        void setWhat();
        void writeToVch(std::vector<unsigned char>* vch);
        static TargetNotOwnedError* consumeFromBuf(unsigned long cost, bool fatalToBatch, unsigned char **ptrPtr);
        std::string target();
    };
    
    class CreditInsufficientError : public Error {
        unsigned long requiredCredit_;
        unsigned long availableCredit_;
    public:
        CreditInsufficientError(unsigned long requiredCredit, unsigned long availableCredit, unsigned long cost, bool fatalToBatch);
        void setWhat();
        void writeToVch(std::vector<unsigned char>* vch);
        static CreditInsufficientError* consumeFromBuf(unsigned long cost, bool fatalToBatch, unsigned char **ptrPtr);
        unsigned long requiredCredit();
        unsigned long availableCredit();
        unsigned long creditMissing();
    };
    
    class CreditOverflowError : public Error {
        unsigned long pocketCredit_;
        unsigned long addedCredit_;
    public:
        CreditOverflowError(unsigned long pocketCredit, unsigned long addedCredit, unsigned long cost, bool fatalToBatch);
        void setWhat();
        void writeToVch(std::vector<unsigned char>* vch);
        static CreditOverflowError* consumeFromBuf(unsigned long cost, bool fatalToBatch, unsigned char **ptrPtr);
        unsigned long pocketCredit();
        unsigned long addedCredit();
        unsigned long long totalCredit();
    };
    
}//namespace commands::errors

}//namespace commands

#endif
