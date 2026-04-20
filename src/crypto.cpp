// PassVault
// Copyright (c) 2026 Trevor W
// SPDX-License-Identifier: MIT

// ============================================================
//  crypto.cpp  -  PassVault Encryption
//  Requires: Libsodium
// ============================================================

#include "crypto.h"
#include <sodium.h>
#include <stdexcept>

namespace CRYPTO
{
    bool Init()
    {
        return sodium_init() >= 0;
    }

    std::vector<unsigned char> GenerateRandomKey()
    {
        std::vector<unsigned char> key(crypto_secretbox_KEYBYTES);
        randombytes_buf(key.data(), key.size());
        return key;
    }

    std::vector<unsigned char> EncryptWithKey(const std::string& plaintext,
        const std::vector<unsigned char>& key)
    {
        std::vector<unsigned char> ciphertext(plaintext.size() + crypto_secretbox_MACBYTES);
        unsigned char nonce[crypto_secretbox_NONCEBYTES];
        randombytes_buf(nonce, sizeof(nonce));

        crypto_secretbox_easy(ciphertext.data(),
            (const unsigned char*)plaintext.data(), plaintext.size(),
            nonce, key.data());

        // Format: [nonce (24 bytes)] + [ciphertext]
        std::vector<unsigned char> result;
        result.insert(result.end(), nonce, nonce + sizeof(nonce));
        result.insert(result.end(), ciphertext.begin(), ciphertext.end());
        return result;
    }

    std::string DecryptWithKey(const std::vector<unsigned char>& data,
        const std::vector<unsigned char>& key)
    {
        if (data.size() < crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES)
            throw std::runtime_error("Invalid encrypted data");

        unsigned char nonce[crypto_secretbox_NONCEBYTES];
        std::copy(data.begin(), data.begin() + crypto_secretbox_NONCEBYTES, nonce);

        const unsigned char* ciphertext = data.data() + crypto_secretbox_NONCEBYTES;
        size_t ciphertext_len = data.size() - crypto_secretbox_NONCEBYTES;

        std::vector<unsigned char> plaintext(ciphertext_len - crypto_secretbox_MACBYTES);

        if (crypto_secretbox_open_easy(plaintext.data(), ciphertext, ciphertext_len,
            nonce, key.data()) != 0)
        {
            throw std::runtime_error("Decryption failed - corrupted data");
        }

        return std::string(plaintext.begin(), plaintext.end());
    }
}