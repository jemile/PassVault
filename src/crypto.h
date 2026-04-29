// PassVault
// Copyright (c) 2026 Trevor W
// SPDX-License-Identifier: MIT

#pragma once
#include <vector>
#include <string>

namespace CRYPTO
{
    bool Init();

    // Vault data encryption
    std::vector<unsigned char> GenerateRandomKey();
    std::vector<unsigned char> EncryptWithKey(const std::string& plaintext,
        const std::vector<unsigned char>& key);
    std::string DecryptWithKey(const std::vector<unsigned char>& ciphertext,
        const std::vector<unsigned char>& key);

    // Master password  (Argon2id via libsodium)
    // HashMasterPassword: returns an opaque hash string (includes salt); empty on failure
    std::string HashMasterPassword(const std::string& password);
    // VerifyMasterPassword: true if password matches the stored hash
    bool VerifyMasterPassword(const std::string& password, const std::string& storedHash);
    // EncryptKeyWithPassword: wraps a raw key in [pwhash-salt | nonce | ciphertext]
    std::vector<unsigned char> EncryptKeyWithPassword(const std::string& password,
        const std::vector<unsigned char>& key);
    // DecryptKeyWithPassword: reverses the above; returns empty vector on failure
    std::vector<unsigned char> DecryptKeyWithPassword(const std::string& password,
        const std::vector<unsigned char>& data);

}