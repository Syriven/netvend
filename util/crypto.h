#ifndef NETVEND_CRYPTO_H
#define NETVEND_CRYPTO_H

#include <cryptopp/osrng.h>
#include <cryptopp/rsa.h>
//#include <cryptopp/files.h>
//#include <cryptopp/base64.h>
//#include <cryptopp/hex.h>
#include <cryptopp/ripemd.h>
#include <string>

#include "netvend/common_constants.h"
#include "util/b58check.h"

namespace crypto {
    
typedef CryptoPP::RSA::PublicKey RSAPubkey;
typedef CryptoPP::RSA::PrivateKey RSAPrivkey;

std::string RSAPubkeyToNetvendAddress(RSAPubkey pubkey);

std::vector<unsigned char> cryptoSign(std::vector<unsigned char> &dataVch, RSAPrivkey &privkey, CryptoPP::AutoSeededRandomPool &rng);

bool verifySig(const RSAPubkey &pubkey, const std::vector<unsigned char> &dataVch, const std::vector<unsigned char> &sig);

std::vector<unsigned char> encodePubkey(RSAPubkey pubkey);

RSAPubkey decodePubkey(const unsigned char* pubkeyData, size_t pubkeySize);

}//namespace crypto

#endif