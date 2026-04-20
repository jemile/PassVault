// PassVault
// Copyright (c) 2026 Trevor W
// SPDX-License-Identifier: MIT

// ============================================================
//  password_manager.cpp  -  Saves and loads the vault file, manages entries in memory
// ============================================================

#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <chrono>
#include <random>
#include <ctime>
#include <iomanip>

#include "password_manager.h"
#include "crypto.h"

// ============================================================
//  Constructor – resolves / creates the data directory
// ============================================================
PasswordManager::PasswordManager()
{
    std::filesystem::path curPath = std::filesystem::current_path();
    std::filesystem::path dataDir = curPath / "data";

    if (!std::filesystem::exists(dataDir))
        std::filesystem::create_directories(dataDir);

    dataFilePath = (dataDir / "vault.dat").string();
}

// ============================================================
//  Private helpers – single-line escaping for the file format
// ============================================================
std::string PasswordManager::EscapeValue(const std::string& val)
{
    std::string out;
    out.reserve(val.size());
    for (char c : val)
    {
        if      (c == '\n') { out += "\\n";  }
        else if (c == '\r') { out += "\\r";  }
        else if (c == '\\') { out += "\\\\"; }
        else                { out += c;      }
    }
    return out;
}

std::string PasswordManager::UnescapeValue(const std::string& val)
{
    std::string out;
    out.reserve(val.size());
    for (size_t i = 0; i < val.size(); ++i)
    {
        if (val[i] == '\\' && i + 1 < val.size())
        {
            char next = val[i + 1];
            if      (next == 'n')  { out += '\n'; ++i; }
            else if (next == 'r')  { out += '\r'; ++i; }
            else if (next == '\\') { out += '\\'; ++i; }
            else                   { out += val[i];    }
        }
        else
        {
            out += val[i];
        }
    }
    return out;
}

// ============================================================
//  LoadFromFile
//  Format:
//      # comment
//      [ENTRY]
//      key=value
//      [/ENTRY]
// ============================================================
bool PasswordManager::LoadFromFile()
{
    LoadOrCreateEncryptionKey();   // make sure we have the key

    std::ifstream file(dataFilePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<unsigned char> encrypted(size);
    file.read(reinterpret_cast<char*>(encrypted.data()), size);

    if (encrypted.empty()) return true; // empty vault is ok

    try
    {
        std::string plaintext = CRYPTO::DecryptWithKey(encrypted, encryptionKey);

        // Parse the decrypted text
        std::istringstream iss(plaintext);
        std::string line;
        PasswordEntry current;
        bool inEntry = false;

        entries.clear();

        while (std::getline(iss, line))
        {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line[0] == '#') continue;

            if (line == "[ENTRY]") { current = PasswordEntry{}; inEntry = true; continue; }
            if (line == "[/ENTRY]") { if (inEntry && !current.id.empty()) entries.push_back(current); inEntry = false; continue; }
            if (!inEntry) continue;

            size_t sep = line.find('=');
            if (sep == std::string::npos) continue;

            std::string key = line.substr(0, sep);
            std::string val = UnescapeValue(line.substr(sep + 1));

            if (key == "id")       current.id = val;
            else if (key == "title")    current.title = val;
            else if (key == "website")  current.website = val;
            else if (key == "username") current.username = val;
            else if (key == "password") current.password = val;
            else if (key == "category") current.category = val;
            else if (key == "notes")    current.notes = val;
            else if (key == "created")  current.createdAt = val;
            else if (key == "modified") current.modifiedAt = val;
        }
        return true;
    }
    catch (...)
    {
        std::cerr << "Decryption failed - vault corrupted?\n";
        return false;
    }
}
// ============================================================
//  SaveToFile
// ============================================================
bool PasswordManager::SaveToFile()
{
    LoadOrCreateEncryptionKey();   // ensure key exists

    std::ostringstream oss;
    oss << "# PassVault Data File v2 (encrypted with libsodium)\n";
    oss << "# Entries: " << entries.size() << "\n\n";

    for (const auto& e : entries)
    {
        oss << "[ENTRY]\n";
        oss << "id=" << EscapeValue(e.id) << "\n";
        oss << "title=" << EscapeValue(e.title) << "\n";
        oss << "website=" << EscapeValue(e.website) << "\n";
        oss << "username=" << EscapeValue(e.username) << "\n";
        oss << "password=" << EscapeValue(e.password) << "\n";
        oss << "category=" << EscapeValue(e.category) << "\n";
        oss << "notes=" << EscapeValue(e.notes) << "\n";
        oss << "created=" << EscapeValue(e.createdAt) << "\n";
        oss << "modified=" << EscapeValue(e.modifiedAt) << "\n";
        oss << "[/ENTRY]\n\n";
    }

    std::string plaintext = oss.str();
    auto encrypted = CRYPTO::EncryptWithKey(plaintext, encryptionKey);

    std::ofstream file(dataFilePath, std::ios::binary);
    if (!file.is_open()) return false;

    file.write(reinterpret_cast<const char*>(encrypted.data()), encrypted.size());
    return true;
}

// ============================================================
//  CRUD
// ============================================================
void PasswordManager::AddEntry(PasswordEntry entry)
{
    entry.id         = GenerateId();
    entry.createdAt  = GetCurrentTimestamp();
    entry.modifiedAt = entry.createdAt;
    entries.push_back(std::move(entry));
    SaveToFile();
}

void PasswordManager::UpdateEntry(const PasswordEntry& entry)
{
    for (auto& e : entries)
    {
        if (e.id == entry.id)
        {
            std::string created = e.createdAt; // preserve original creation date
            e            = entry;
            e.createdAt  = created;
            e.modifiedAt = GetCurrentTimestamp();
            break;
        }
    }
    SaveToFile();
}

void PasswordManager::RemoveEntry(const std::string& id)
{
    entries.erase(
        std::remove_if(entries.begin(), entries.end(),
                       [&id](const PasswordEntry& e) { return e.id == id; }),
        entries.end());
    SaveToFile();
}

int PasswordManager::FindIndexById(const std::string& id) const
{
    for (int i = 0; i < (int)entries.size(); ++i)
        if (entries[i].id == id) return i;
    return -1;
}

// ============================================================
//  Utilities
// ============================================================
std::string PasswordManager::GenerateId()
{
    static std::random_device rd;
    static std::mt19937       gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    static const char* hex = "0123456789abcdef";

    std::string id;
    id.reserve(16);
    for (int i = 0; i < 16; ++i)
        id += hex[dis(gen)];
    return id;
}

std::string PasswordManager::GetCurrentTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
    localtime_s(&tm_buf, &t);

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%b %d, %Y %H:%M");
    return oss.str();
}

// ============================================================
//  Password generator
// ============================================================
std::string PasswordManager::GeneratePassword(int length,
                                               bool upper,
                                               bool lower,
                                               bool digits,
                                               bool symbols)
{
    std::string charset;
    if (upper)   charset += "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    if (lower)   charset += "abcdefghijklmnopqrstuvwxyz";
    if (digits)  charset += "0123456789";
    if (symbols) charset += "!@#$%^&*()-_=+[]{}|;:,.<>?";

    if (charset.empty())
        charset = "abcdefghijklmnopqrstuvwxyz0123456789";

    static std::random_device rd;
    static std::mt19937       gen(rd());
    std::uniform_int_distribution<> dis(0, (int)charset.size() - 1);

    std::string result;
    result.reserve(length);
    for (int i = 0; i < length; ++i)
        result += charset[dis(gen)];
    return result;
}

// ============================================================
//  Password strength  (0 = very weak … 5 = very strong)
// ============================================================
int PasswordManager::PasswordStrength(const std::string& pwd)
{
    if (pwd.empty()) return 0;

    bool hasUpper = false, hasLower = false, hasDigit = false, hasSymbol = false;
    for (char c : pwd)
    {
        if      (isupper((unsigned char)c)) hasUpper  = true;
        else if (islower((unsigned char)c)) hasLower  = true;
        else if (isdigit((unsigned char)c)) hasDigit  = true;
        else                                hasSymbol = true;
    }

    int score = 0;
    int len   = (int)pwd.size();
    if (len >=  6) ++score;
    if (len >= 10) ++score;
    if (len >= 14) ++score;
    if (len >= 18) ++score;
    if (hasUpper)  ++score;
    if (hasLower)  ++score;
    if (hasDigit)  ++score;
    if (hasSymbol) ++score;

    return std::min(score, 5);
}

const char* PasswordManager::PasswordStrengthLabel(int score)
{
    switch (score)
    {
        case 0:  return "Very Weak";
        case 1:  return "Weak";
        case 2:  return "Fair";
        case 3:  return "Good";
        case 4:  return "Strong";
        case 5:  return "Very Strong";
        default: return "";
    }
}

// ============================================================
//  Auto key management  (legacy fallback – skipped when master password is active)
// ============================================================
void PasswordManager::LoadOrCreateEncryptionKey()
{
    // If the key was already set via UnlockWithMasterPassword, don't touch it.
    if (!encryptionKey.empty()) return;

    std::string keyPath = (std::filesystem::path(dataFilePath).parent_path() / "vault.key").string();

    std::ifstream keyFile(keyPath, std::ios::binary);
    if (keyFile.is_open())
    {
        encryptionKey.assign(std::istreambuf_iterator<char>(keyFile), {});
    }
    else
    {
        encryptionKey = CRYPTO::GenerateRandomKey();
        std::ofstream out(keyPath, std::ios::binary);
        out.write(reinterpret_cast<const char*>(encryptionKey.data()), encryptionKey.size());
    }
}

// ============================================================
//  Master password management
// ============================================================
bool PasswordManager::HasMasterPassword() const
{
    std::string authPath = (std::filesystem::path(dataFilePath).parent_path() / "master.auth").string();
    return std::filesystem::exists(authPath);
}

bool PasswordManager::SetupMasterPassword(const std::string& password)
{
    auto dataDir = std::filesystem::path(dataFilePath).parent_path();
    std::string authPath = (dataDir / "master.auth").string();
    std::string keyPath  = (dataDir / "vault.key").string();

    // 1. Hash the master password (Argon2id, salt embedded)
    std::string hash = CRYPTO::HashMasterPassword(password);
    if (hash.empty()) return false;

    // 2. Generate a fresh random vault key
    encryptionKey = CRYPTO::GenerateRandomKey();

    // 3. Wrap the vault key with the master password
    auto wrappedKey = CRYPTO::EncryptKeyWithPassword(password, encryptionKey);
    if (wrappedKey.empty()) return false;

    // 4. Persist both files
    {
        std::ofstream f(authPath, std::ios::binary);
        if (!f.is_open()) return false;
        f.write(hash.c_str(), hash.size());
    }
    {
        std::ofstream f(keyPath, std::ios::binary);
        if (!f.is_open()) return false;
        f.write(reinterpret_cast<const char*>(wrappedKey.data()), wrappedKey.size());
    }
    return true;
}

bool PasswordManager::UnlockWithMasterPassword(const std::string& password)
{
    auto dataDir = std::filesystem::path(dataFilePath).parent_path();
    std::string authPath = (dataDir / "master.auth").string();
    std::string keyPath  = (dataDir / "vault.key").string();

    // 1. Verify password against stored hash
    {
        std::ifstream f(authPath, std::ios::binary);
        if (!f.is_open()) return false;
        std::string hash(std::istreambuf_iterator<char>(f), {});
        if (!CRYPTO::VerifyMasterPassword(password, hash)) return false;
    }

    // 2. Decrypt the vault key
    {
        std::ifstream f(keyPath, std::ios::binary);
        if (!f.is_open()) return false;
        std::vector<unsigned char> wrapped(std::istreambuf_iterator<char>(f), {});
        encryptionKey = CRYPTO::DecryptKeyWithPassword(password, wrapped);
        if (encryptionKey.empty()) return false;
    }
    return true;
}

bool PasswordManager::VerifyMasterPassword(const std::string& password) const
{
    std::string authPath = (std::filesystem::path(dataFilePath).parent_path() / "master.auth").string();
    std::ifstream f(authPath, std::ios::binary);
    if (!f.is_open()) return false;
    std::string hash(std::istreambuf_iterator<char>(f), {});
    return CRYPTO::VerifyMasterPassword(password, hash);
}