#ifndef NETVEND_DATABASE_H
#define NETVEND_DATABASE_H

#include <iostream>
#include <stdexcept>
#include <pqxx/pqxx>

#include "database.h"
#include "crypto.h"
#include "netvend/exception.h"
#include "netvend/commands.h"

namespace database {

const std::string FETCH_FILE_FEES_SUPPORTED_PER_POCKET = "FetchFileFeesSupportedPerPocket";
    
const std::string CHECK_AGENT_EXISTS = "CheckAgentExists";
const std::string INSERT_AGENT = "InsertAgent";
const std::string FETCH_AGENT_PUBKEY = "FetchAgentPubkey";

const std::string INSERT_POCKET_WITH_DEPOSIT_ADDRESS = "InsertPocketWithDeposit";
const std::string INSERT_POCKET = "InsertPocket";
const std::string INSERT_POCKET_WITHOUT_OWNER = "InsertPocketWithoutOwner";
const std::string FETCH_POCKET_OWNER = "FetchPocketOwner";
const std::string UPDATE_POCKET_OWNER = "UpdatePocketOwner";
const std::string UPDATE_POCKET_DEPOSIT_ADDRESS = "UpdatePocketDepositAddress";
const std::string FETCH_POCKET_BALANCE = "FetchPocketBalance";
const std::string DEDUCT_FROM_POCKET_WITH_OWNER = "DeductFromPocketWithOwner";
const std::string ADD_TO_POCKET = "AddToPocket";

const std::string INSERT_FILE = "InsertFile";
const std::string FETCH_FILE_OWNER = "FetchFileOwner";
const std::string UPDATE_FILE_BY_ID = "UpdateFileByID";
const std::string READ_FILE_BY_ID = "ReadFileByID";

class NoRowFoundException : public std::runtime_error {
public:
    NoRowFoundException();
};

class DuplicateUniqueValueException : public std::runtime_error {
public:
    DuplicateUniqueValueException();
};

void prepareConnection(pqxx::connection **dbConn);


bool agentRowExists(pqxx::connection *dbConn, std::string agentAddress);
void insertAgent(pqxx::connection *dbConn, std::string agentAddress, std::vector<unsigned char> DEREncodedPubkey, unsigned long defaultPocketID);
crypto::RSAPubkey fetchAgentPubkey(pqxx::connection *dbConn, std::string agentAddress);

unsigned long insertPocket(pqxx::connection *dbConn, std::string ownerAddress, std::string depositAddress);
unsigned long insertPocket(pqxx::connection *dbConn, std::string ownerAddress);
unsigned long insertPocket(pqxx::connection *dbConn);
std::string fetchPocketOwner(pqxx::connection *dbConn, unsigned long pocketID);
void verifyPocketOwner(pqxx::connection *dbConn, unsigned long pocketID, std::string agentAddress);
void updatePocketOwner(pqxx::connection *dbConn, unsigned long pocketID, std::string newOwnerAddress);
void updatePocketDepositAddress(pqxx::connection* dbConn, std::string ownerAddress, unsigned long pocketID, std::string newDepositAddress);
void pocketTransfer(pqxx::connection *dbConn, std::string fromOwnerAddress, unsigned long fromPocketID, unsigned long toPocketID, signed long long amount);

unsigned long insertFile(pqxx::connection *dbConn, std::string ownerAddress, std::string name, unsigned long pocketID);
std::string fetchFileOwner(pqxx::connection *dbConn, unsigned long fileID);
void verifyFileOwner(pqxx::connection *dbConn, unsigned long fileID, std::string agentAddress);
void updateFileByID(pqxx::connection *dbConn, unsigned long fileID, unsigned char* data, unsigned short dataSize);
std::vector<unsigned char> readFileByID(pqxx::connection *dbConn, unsigned long fileID);

void chargeFileUpkeepFees(pqxx::connection *dbConn, int creditPerFile, int creditPerByte);

}//namespace database

#endif