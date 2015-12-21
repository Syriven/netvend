#ifndef NETVEND_DATABASE_H
#define NETVEND_DATABASE_H

#include <iostream>
#include <stdexcept>
#include <pqxx/pqxx>

#include "database.h"
#include "crypto.h"

namespace database {

const std::string CHECK_AGENT_EXISTS = "CheckAgentExists";
const std::string INSERT_AGENT = "InsertAgent";
const std::string FETCH_AGENT_PUBKEY = "FetchAgentPubkey";

const std::string INSERT_POCKET_WITH_DEPOSIT_ADDRESS = "InsertPocketWithDeposit";
const std::string INSERT_POCKET = "InsertPocket";
const std::string INSERT_POCKET_WITHOUT_OWNER = "InsertPocketWithoutOwner";
const std::string FETCH_POCKET_OWNER = "FetchPocketOwner";
const std::string UPDATE_POCKET_OWNER = "UpdatePocketOwner";
const std::string UPDATE_POCKET_DEPOSIT_ADDRESS = "UpdatePocketDepositAddress";

const std::string INSERT_CHUNK = "InsertChunk";
const std::string FETCH_CHUNK_OWNER = "FetchChunkOwner";
const std::string UPDATE_CHUNK_BY_ID = "UpdateChunkByID";

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
void updatePocketOwner(pqxx::connection *dbConn, unsigned long pocketID, std::string newOwnerAddress);
void updatePocketDepositAddress(pqxx::connection* dbConn, std::string ownerAddress, unsigned long pocketID, std::string newDepositAddress);

unsigned long insertChunk(pqxx::connection *dbConn, std::string ownerAddress, std::string name, unsigned long pocketID);
std::string fetchChunkOwner(pqxx::connection *dbConn, unsigned long chunkID);
void updateChunkByID(pqxx::connection *dbConn, unsigned long chunkID, unsigned char* data, unsigned short dataSize);

}//namespace database

#endif