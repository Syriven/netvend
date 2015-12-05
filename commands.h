
#ifndef NETVEND_NV_COMMANDS_H
#define NETVEND_NV_COMMANDS_H

#include <boost/shared_ptr.hpp>
#include <boost/lexical_cast.hpp>
#include <string>
#include <stdexcept>

#include "common_constants.h"
#include "pack.h"

namespace commands {

    const unsigned char COMMANDBATCH_COMPLETION_NONE = 0;
    const unsigned char COMMANDBATCH_COMPLETION_SOME = 1;
    const unsigned char COMMANDBATCH_COMPLETION_ALL = 2;
    
    const char COMMANDTYPECHAR_CREATE_POCKET = 'p';
    const char COMMANDTYPECHAR_REQUEST_POCKET_DEPOSIT_ADDRESS = 'd';

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
        bool requestingDepositAddress_;
    public:
        CreatePocket(bool requestingDepositAddress);
        void writeToVch(std::vector<unsigned char>* vch);
        static commands::CreatePocket* consumeFromBuf(unsigned char **ptrPtr);
        bool requestingDepositAddress();
    };
    
    class RequestPocketDepositAddress : public Command {
        unsigned long pocketID_;
    public:
        RequestPocketDepositAddress(unsigned long pocketID);
        void writeToVch(std::vector<unsigned char>* vch);
        static commands::RequestPocketDepositAddress* consumeFromBuf(unsigned char **ptrPtr);
        unsigned long pocketID();
    };

namespace results {

    class Result {
        unsigned char error_;
        unsigned int cost_;
    public:
        Result(unsigned char error, unsigned int cost);
        virtual void writeToVch(std::vector<unsigned char>* vch);
        static Result* consumeFromBuf(unsigned char **ptrPtr, unsigned char commandType);
        unsigned int cost();
        bool error();
    };
    
    class Batch {
        commands::Batch* initiatingCommandBatch_;
        std::vector<boost::shared_ptr<Result> > results_;
        unsigned int cost_;
    public:
        Batch(commands::Batch* initiatingCommandBatch);
        void addResult(boost::shared_ptr<Result> result);
        void writeToVch(std::vector<unsigned char>* vch);
        void consumeDataFromBuf(unsigned char **ptrPtr);
        std::vector<boost::shared_ptr<Result> >* results();
        unsigned int cost();
    };

    class CreatePocket : public Result {
        unsigned long pocketID_;
    public:
        CreatePocket(int cost, unsigned long pocketID);
        void writeToVch(std::vector<unsigned char>* vch);
        static results::CreatePocket* consumeFromBuf(int cost, unsigned char **ptrPtr);
        unsigned long pocketID();
    };
    
    class RequestPocketDepositAddress : public Result {
        std::string depositAddress_;
    public:
        RequestPocketDepositAddress(int cost, std::string depositAddress);
        void writeToVch(std::vector<unsigned char>* vch);
        static results::RequestPocketDepositAddress* consumeFromBuf(int cost, unsigned char **ptrPtr);
        std::string depositAddress();
    };

}//namespace commands::results

namespace errors {
    
//     class Error : public results::Result {
//         bool fatalToBatch_;
//     public:
//         Error(unsigned char error, unsigned int cost, bool fatalToBatch);
//         void writeToVch(std::vector<unsigned char>* vch);
//         static Error* consumeFromBuf(unsigned char error, unsigned int cost, unsigned char **ptrPtr);
//         bool fatalToBatch();
//     };
//     
//     class IvalidTarget : public Error {
//         std::string target_;
//     public:
//         InvalidTarget(std::string target);
//         void writeToVch(std::vector<unsigned char>* vch);
//         static InvalidTarget* consumeFromBuf(int cost, unsigned char **ptrPtr);
//         std::string target();
//     };
//     
//     class TargetNotOwned : public Error {
//         std::string target_;
//     public:
//         TargetNotOwned(std::string target);
//         void writeToVch(std::vector<unsigned char>* vch);
//         static TargetNotOwned* consumeFromBuf(int cost, unsigned char **ptrPtr);
//         std::string target();
//     };
    
}//namespace commands::errors

}//namespace commands

#endif
