#include "crypto.h"

namespace crypto {

std::string RSAPubkeyToNetvendAddress(RSAPubkey pubkey) {
    std::string pubkeyString;
    CryptoPP::RIPEMD160 hash;
    pubkey.DEREncode(CryptoPP::StringSink(pubkeyString).Ref());
    std::string hashedPubkeyString;
    CryptoPP::StringSource(pubkeyString, true, new CryptoPP::HashFilter(hash, new CryptoPP::StringSink(hashedPubkeyString)));
    std::vector<unsigned char> pubkeyVChr;
    std::copy_n(hashedPubkeyString.begin(), hashedPubkeyString.size(), back_inserter(pubkeyVChr));
    pubkeyVChr.insert(pubkeyVChr.begin(), AGENT_ADDRESS_VERSION_BYTE);
    return EncodeBase58Check(pubkeyVChr);
}

std::vector<unsigned char> cryptoSign(std::vector<unsigned char> &dataVch, RSAPrivkey &privkey, CryptoPP::AutoSeededRandomPool &rng) {
    CryptoPP::RSASSA_PKCS1v15_SHA_Signer signer(privkey);
    size_t maxSize = signer.MaxSignatureLength();
    assert(maxSize == MAX_SIG_SIZE);
    std::vector<unsigned char> sig(maxSize);
    size_t size = signer.SignMessage(rng, dataVch.data(), dataVch.size(), sig.data());
    sig.resize(size);
    return sig;
}

bool verifySig(const RSAPubkey &pubkey, const std::vector<unsigned char> &dataVch, const std::vector<unsigned char> &sig) {
    CryptoPP::RSASSA_PKCS1v15_SHA_Verifier verifier(pubkey);
    return verifier.VerifyMessage(dataVch.data(), dataVch.size(), sig.data(), sig.size());
}

std::vector<unsigned char> encodePubkey(RSAPubkey pubkey) {
    CryptoPP::ByteQueue bq;
    pubkey.DEREncode(bq);
    assert(bq.CurrentSize() == DERENCODED_PUBKEY_SIZE);

    std::vector<unsigned char> DEREncodedPubkey(DERENCODED_PUBKEY_SIZE);
    bq.Get(DEREncodedPubkey.data(), DERENCODED_PUBKEY_SIZE);
    
    return DEREncodedPubkey;
}

RSAPubkey decodePubkey(const unsigned char* pubkeyData, size_t pubkeySize) {
    CryptoPP::ByteQueue bq;
    bq.Put(pubkeyData, pubkeySize);
    bq.MessageEnd();
    assert(bq.CurrentSize() == DERENCODED_PUBKEY_SIZE);
    
    crypto::RSAPubkey pubkey;
    pubkey.Load(bq);
    return pubkey;
}

}//namespace crypto