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
    const char COMMANDTYPECHAR_CREATE_PAGE = 2;
    const char COMMANDTYPECHAR_UPDATE_PAGE_BY_ID = 3;

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
    
    class CreatePage : public Command {
        std::string name_;
        unsigned long pocketID_;
    public:
        CreatePage(std::string name, unsigned long pocketID);
        void writeToVch(std::vector<unsigned char>* vch);
        static commands::CreatePage* consumeFromBuf(unsigned char **ptrPtr);
        std::string name();
        unsigned long pocketID();
    };
    
    class UpdatePageByID : public Command {
        unsigned long chunkID_;
        unsigned char* data_;
        unsigned short dataSize_;
        bool mustFreeData_;
    public:
        UpdatePageByID(unsigned long chunkID, unsigned char* data, unsigned short dataSize);
        ~UpdatePageByID();
        void allocSpace();
        void writeToVch(std::vector<unsigned char>* vch);
        static commands::UpdatePageByID* consumeFromBuf(unsigned char **ptrPtr);
        unsigned long chunkID();
        unsigned char* data();
        unsigned short dataSize();
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
    
    class CreatePage : public Result {
        unsigned long chunkID_;
    public:
        CreatePage(unsigned long cost, unsigned long chunkID);
        void writeToVch(std::vector<unsigned char>* vch);
        static results::CreatePage* consumeFromBuf(unsigned long cost, unsigned char **ptrPtr);
        unsigned long chunkID();
    };
    
    class UpdatePageByID : public Result {
    public:
        UpdatePageByID(unsigned long cost);
        void writeToVch(std::vector<unsigned char>* vch);
        static results::UpdatePageByID* consumeFromBuf(unsigned long cost, unsigned char **ptrPtr);
    };

}//namespace commands::results

namespace errors {
    
    const unsigned char ERRORTYPECHAR_NONE = 0;
    const unsigned char ERRORTYPECHAR_INVALID_TARGET = 1;
    const unsigned char ERRORTYPECHAR_TARGET_NOT_OWNED = 2;
    
    class Error : public results::Result, public std::runtime_error {
        bool fatalToBatch_;
    protected:
        std::string what_;
    public:
        Error(unsigned char error, unsigned long cost, bool fatalToBatch);
        void writeToVch(std::vector<unsigned char>* vch);
        static Error* consumeFromBuf(unsigned char error, unsigned long cost, unsigned char **ptrPtr);
        bool fatalToBatch();
        virtual ~Error() throw() {}
        const char* what() const noexcept;
    };
    
    class InvalidTarget : public Error {
        std::string target_;
    public:
        InvalidTarget(std::string target, unsigned long cost, bool fatalToBatch);
        void setWhat();
        void writeToVch(std::vector<unsigned char>* vch);
        static InvalidTarget* consumeFromBuf(unsigned long cost, bool fatalToBatch, unsigned char **ptrPtr);
        std::string target();
    };
    
    class TargetNotOwned : public Error {
        std::string target_;
    public:
        TargetNotOwned(std::string target, unsigned long cost, bool fatalToBatch);
        void setWhat();
        void writeToVch(std::vector<unsigned char>* vch);
        static TargetNotOwned* consumeFromBuf(unsigned long cost, bool fatalToBatch, unsigned char **ptrPtr);
        std::string target();
    };
    
}//namespace commands::errors

}//namespace commands

#endif
