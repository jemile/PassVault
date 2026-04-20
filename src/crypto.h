// PassVault
// Copyright (c) 2026 Trevor W
// SPDX-License-Identifier: MIT

#pragma once
#include <vector>
#include <string>

namespace CRYPTO
{
    bool Init();

    std::vector<unsigned char> GenerateRandomKey();
    std::vector<unsigned char> EncryptWithKey(const std::string& plaintext,
        const std::vector<unsigned char>& key);
    std::string DecryptWithKey(const std::vector<unsigned char>& ciphertext,
        const std::vector<unsigned char>& key);
}