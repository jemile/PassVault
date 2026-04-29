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

    // ============================================================
    //  Master password  (Argon2id)
    // ============================================================
    std::string HashMasterPassword(const std::string& password)
    {
        // crypto_pwhash_str produces a null-terminated ASCII string that
        // embeds the salt, algorithm id, and parameters - safe to store as-is.
        char hash[crypto_pwhash_STRBYTES] = {};
        if (crypto_pwhash_str(hash, password.c_str(), password.size(),
                crypto_pwhash_OPSLIMIT_INTERACTIVE,
                crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0)
            return {};                          // out of memory
        return std::string(hash, crypto_pwhash_STRBYTES);
    }

    bool VerifyMasterPassword(const std::string& password, const std::string& storedHash)
    {
        if (storedHash.size() < crypto_pwhash_STRBYTES) return false;
        return crypto_pwhash_str_verify(
            storedHash.c_str(),
            password.c_str(), password.size()) == 0;
    }

    std::vector<unsigned char> EncryptKeyWithPassword(const std::string& password,
        const std::vector<unsigned char>& key)
    {
        // Derive a 32-byte wrapping key from the master password + fresh salt
        unsigned char salt[crypto_pwhash_SALTBYTES];
        randombytes_buf(salt, sizeof(salt));

        unsigned char derived[crypto_secretbox_KEYBYTES];
        if (crypto_pwhash(derived, sizeof(derived),
                password.c_str(), password.size(), salt,
                crypto_pwhash_OPSLIMIT_INTERACTIVE,
                crypto_pwhash_MEMLIMIT_INTERACTIVE,
                crypto_pwhash_ALG_DEFAULT) != 0)
            return {};

        // Encrypt the vault key with the derived key
        unsigned char nonce[crypto_secretbox_NONCEBYTES];
        randombytes_buf(nonce, sizeof(nonce));

        std::vector<unsigned char> ct(key.size() + crypto_secretbox_MACBYTES);
        crypto_secretbox_easy(ct.data(), key.data(), key.size(), nonce, derived);

        // Layout: [salt | nonce | ciphertext]
        std::vector<unsigned char> result;
        result.insert(result.end(), salt,  salt  + sizeof(salt));
        result.insert(result.end(), nonce, nonce + sizeof(nonce));
        result.insert(result.end(), ct.begin(), ct.end());
        return result;
    }

    std::vector<unsigned char> DecryptKeyWithPassword(const std::string& password,
        const std::vector<unsigned char>& data)
    {
        const size_t hdr = crypto_pwhash_SALTBYTES + crypto_secretbox_NONCEBYTES;
        if (data.size() < hdr + crypto_secretbox_MACBYTES) return {};

        unsigned char salt[crypto_pwhash_SALTBYTES];
        std::copy(data.begin(), data.begin() + crypto_pwhash_SALTBYTES, salt);

        unsigned char nonce[crypto_secretbox_NONCEBYTES];
        std::copy(data.begin() + crypto_pwhash_SALTBYTES,
                  data.begin() + hdr, nonce);

        unsigned char derived[crypto_secretbox_KEYBYTES];
        if (crypto_pwhash(derived, sizeof(derived),
                password.c_str(), password.size(), salt,
                crypto_pwhash_OPSLIMIT_INTERACTIVE,
                crypto_pwhash_MEMLIMIT_INTERACTIVE,
                crypto_pwhash_ALG_DEFAULT) != 0)
            return {};

        const unsigned char* ct = data.data() + hdr;
        size_t ct_len           = data.size() - hdr;

        std::vector<unsigned char> plain(ct_len - crypto_secretbox_MACBYTES);
        if (crypto_secretbox_open_easy(plain.data(), ct, ct_len, nonce, derived) != 0)
            return {};

        return plain;
    }

}