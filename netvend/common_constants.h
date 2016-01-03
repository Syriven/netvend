#ifndef NETVEND_COMMON_CONSTANTS_H
#define NETVEND_COMMON_CONSTANTS_H

#include <string>

const std::string DEFAULT_SERVER_IP = "127.0.0.1";
const unsigned int DEFAULT_SERVER_PORT = 8395;

const unsigned int MAX_ADDRESS_SIZE = 34;
const unsigned int DERENCODED_PUBKEY_SIZE = 420;
const unsigned int MAX_SIG_SIZE = 384;

const unsigned int AGENT_KEYSIZE = 3072;
const unsigned char AGENT_ADDRESS_VERSION_BYTE = 61;

#endif
