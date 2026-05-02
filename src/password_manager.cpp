// PassVault
// Copyright (c) 2026 Trevor W
// SPDX-License-Identifier: MIT

// ============================================================
//  password_manager.cpp  -  Vault serialization, CRUD, and utilities
//  See vault_backup.cpp for JSON export/import and backup functions
// ============================================================

#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <chrono>
#include <random>
#include <ctime>
#include <iomanip>
#include <algorithm>

#include "password_manager.h"
#include "crypto.h"


// ============================================================
//  Constructor - resolves / creates the data directory
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
//  Private helpers - single-line escaping for the file format
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
//  Supports both the legacy format (category=) and the new
//  format (tags=, favorite=, histN_ts=, histN_pw=).
// ============================================================
bool PasswordManager::LoadFromFile()
{
    LoadOrCreateEncryptionKey();

    std::ifstream file(dataFilePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<unsigned char> encrypted(size);
    file.read(reinterpret_cast<char*>(encrypted.data()), size);

    if (encrypted.empty()) return true;

    try
    {
        std::string plaintext = CRYPTO::DecryptWithKey(encrypted, encryptionKey);

        std::istringstream iss(plaintext);
        std::string line;

        PasswordEntry current;
        bool   inEntry      = false;
        bool   inFolder     = false;
        bool   hasTags      = false;  // distinguishes new vs legacy format
        std::string legacyCat;        // old "category" field fallback
        std::string folderNameBuf;    // accumulates name= inside a [FOLDER] block

        entries.clear();
        knownFolders.clear();

        while (std::getline(iss, line))
        {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line[0] == '#') continue;

            if (line == "[FOLDER]")
            {
                folderNameBuf.clear();
                inFolder = true;
                continue;
            }

            if (line == "[/FOLDER]")
            {
                if (!folderNameBuf.empty())
                {
                    if (std::find(knownFolders.begin(), knownFolders.end(), folderNameBuf)
                            == knownFolders.end())
                        knownFolders.push_back(folderNameBuf);
                }
                inFolder = false;
                continue;
            }

            if (inFolder)
            {
                size_t sep2 = line.find('=');
                if (sep2 != std::string::npos && line.substr(0, sep2) == "name")
                    folderNameBuf = UnescapeValue(line.substr(sep2 + 1));
                continue;
            }

            if (line == "[ENTRY]")
            {
                current    = PasswordEntry{};
                hasTags    = false;
                legacyCat.clear();
                inEntry    = true;
                continue;
            }

            if (line == "[/ENTRY]")
            {
                if (inEntry && !current.id.empty())
                {
                    // Backward compat: convert old single-category to first tag
                    if (!hasTags && !legacyCat.empty())
                        current.tags.push_back(legacyCat);

                    entries.push_back(current);
                }
                inEntry = false;
                continue;
            }

            if (!inEntry) continue;

            size_t sep = line.find('=');
            if (sep == std::string::npos) continue;

            std::string key = line.substr(0, sep);
            std::string val = UnescapeValue(line.substr(sep + 1));

            if      (key == "id")       current.id       = val;
            else if (key == "title")    current.title    = val;
            else if (key == "website")  current.website  = val;
            else if (key == "username") current.username = val;
            else if (key == "password") current.password = val;
            else if (key == "notes")    current.notes    = val;
            else if (key == "created")  current.createdAt  = val;
            else if (key == "modified") current.modifiedAt = val;

            // New fields
            else if (key == "folder")   current.folder     = val;
            else if (key == "favorite") current.isFavorite = (val == "1");
            else if (key == "tags")
            {
                hasTags = true;
                std::istringstream ss(val);
                std::string tag;
                while (std::getline(ss, tag, ','))
                {
                    while (!tag.empty() && tag.front() == ' ') tag = tag.substr(1);
                    while (!tag.empty() && tag.back()  == ' ') tag.pop_back();
                    if (!tag.empty()) current.tags.push_back(tag);
                }
            }

            // Legacy single-category (kept for backward compat)
            else if (key == "category") legacyCat = val;

            // Password history  (histN_ts / histN_pw)
            else if (key.size() > 4 && key.rfind("hist", 0) == 0)
            {
                size_t underscore = key.rfind('_');
                if (underscore != std::string::npos && underscore > 4)
                {
                    std::string field  = key.substr(underscore + 1);
                    std::string idxStr = key.substr(4, underscore - 4);
                    try
                    {
                        int idx = std::stoi(idxStr);
                        while ((int)current.passwordHistory.size() <= idx)
                            current.passwordHistory.push_back({"", ""});

                        if      (field == "ts") current.passwordHistory[idx].first  = val;
                        else if (field == "pw") current.passwordHistory[idx].second = val;
                    }
                    catch (...) {}
                }
            }
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
    LoadOrCreateEncryptionKey();

    std::ostringstream oss;
    oss << "# PassVault Data File v3 (encrypted with libsodium)\n";
    oss << "# Entries: " << entries.size() << "\n\n";

    // Write folder names first (knownFolders + any referenced by entries)
    for (const auto& fn : GetAllFolders())
    {
        oss << "[FOLDER]\n";
        oss << "name=" << EscapeValue(fn) << "\n";
        oss << "[/FOLDER]\n\n";
    }

    for (const auto& e : entries)
    {
        oss << "[ENTRY]\n";
        oss << "id="       << EscapeValue(e.id)       << "\n";
        oss << "title="    << EscapeValue(e.title)    << "\n";
        oss << "website="  << EscapeValue(e.website)  << "\n";
        oss << "username=" << EscapeValue(e.username) << "\n";
        oss << "password=" << EscapeValue(e.password) << "\n";
        oss << "notes="    << EscapeValue(e.notes)    << "\n";
        oss << "created="  << EscapeValue(e.createdAt)  << "\n";
        oss << "modified=" << EscapeValue(e.modifiedAt) << "\n";
        oss << "favorite=" << (e.isFavorite ? 1 : 0) << "\n";
        if (!e.folder.empty())
            oss << "folder=" << EscapeValue(e.folder) << "\n";

        // Tags (comma-separated; tag names must not contain commas)
        if (!e.tags.empty())
        {
            std::string tagStr;
            for (size_t i = 0; i < e.tags.size(); ++i)
            {
                if (i > 0) tagStr += ',';
                tagStr += e.tags[i];
            }
            oss << "tags=" << EscapeValue(tagStr) << "\n";
        }

        // Password history
        for (int i = 0; i < (int)e.passwordHistory.size(); ++i)
        {
            oss << "hist" << i << "_ts=" << EscapeValue(e.passwordHistory[i].first)  << "\n";
            oss << "hist" << i << "_pw=" << EscapeValue(e.passwordHistory[i].second) << "\n";
        }

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

void PasswordManager::UpdateEntry(const PasswordEntry& newEntry)
{
    for (auto& e : entries)
    {
        if (e.id != newEntry.id) continue;

        std::string preserved_createdAt = e.createdAt;
        auto        history             = e.passwordHistory;  // preserve existing history

        // Auto-record old password when it changes
        if (!e.password.empty() && e.password != newEntry.password)
        {
            history.push_back({ e.modifiedAt, e.password });
            if (history.size() > 10)
                history.erase(history.begin());  // drop oldest if over limit
        }

        e            = newEntry;
        e.createdAt  = preserved_createdAt;
        e.modifiedAt = GetCurrentTimestamp();
        e.passwordHistory = history;  // restore + possibly extended history
        break;
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
//  Auto key management
// ============================================================
void PasswordManager::LoadOrCreateEncryptionKey()
{
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

    std::string hash = CRYPTO::HashMasterPassword(password);
    if (hash.empty()) return false;

    encryptionKey = CRYPTO::GenerateRandomKey();

    auto wrappedKey = CRYPTO::EncryptKeyWithPassword(password, encryptionKey);
    if (wrappedKey.empty()) return false;

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

    {
        std::ifstream f(authPath, std::ios::binary);
        if (!f.is_open()) return false;
        std::string hash(std::istreambuf_iterator<char>(f), {});
        if (!CRYPTO::VerifyMasterPassword(password, hash)) return false;
    }
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


// ============================================================
//  Folder management
// ============================================================
void PasswordManager::AddFolder(const std::string& name)
{
    if (name.empty()) return;
    if (std::find(knownFolders.begin(), knownFolders.end(), name) == knownFolders.end())
    {
        knownFolders.push_back(name);
        SaveToFile();
    }
}

bool PasswordManager::RemoveFolder(const std::string& name)
{
    // Refuse if any entry still lives in this folder
    for (const auto& e : entries)
        if (e.folder == name) return false;

    auto it = std::find(knownFolders.begin(), knownFolders.end(), name);
    if (it != knownFolders.end())
    {
        knownFolders.erase(it);
        SaveToFile();
    }
    return true;
}

std::vector<std::string> PasswordManager::GetAllFolders() const
{
    std::vector<std::string> result = knownFolders;
    // Also include folders referenced by entries that aren't in knownFolders
    // (e.g. imported vaults from older format)
    for (const auto& e : entries)
        if (!e.folder.empty())
            if (std::find(result.begin(), result.end(), e.folder) == result.end())
                result.push_back(e.folder);
    return result;
}


// ============================================================
//  ChangeMasterPassword
//  Verifies the current password, derives a new hash + wrapped
//  key, and atomically replaces both on-disk files.
//  Strategy: back up vault.key first so we can restore it if
//  the auth write fails (keeps the vault openable in any case).
// ============================================================
bool PasswordManager::ChangeMasterPassword(const std::string& currentPw,
                                           const std::string& newPw)
{
    // Vault must already be unlocked (encryptionKey must be in memory)
    if (encryptionKey.empty()) return false;
    if (!VerifyMasterPassword(currentPw)) return false;

    auto dataDir  = std::filesystem::path(dataFilePath).parent_path();
    std::string authPath = (dataDir / "master.auth").string();
    std::string keyPath  = (dataDir / "vault.key").string();
    std::string keyBak   = keyPath + ".bak";

    // Pre-compute both outputs before touching any files
    std::string newHash = CRYPTO::HashMasterPassword(newPw);
    if (newHash.empty()) return false;

    auto wrappedKey = CRYPTO::EncryptKeyWithPassword(newPw, encryptionKey);
    if (wrappedKey.empty()) return false;

    // Back up the current vault.key so we can restore on partial failure
    std::error_code ec;
    std::filesystem::copy_file(keyPath, keyBak,
        std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) return false;

    // Write new vault.key
    {
        std::ofstream f(keyPath, std::ios::binary | std::ios::trunc);
        if (!f.is_open()) { std::filesystem::rename(keyBak, keyPath, ec); return false; }
        f.write(reinterpret_cast<const char*>(wrappedKey.data()), wrappedKey.size());
        if (!f.good())    { std::filesystem::rename(keyBak, keyPath, ec); return false; }
    }

    // Write new master.auth
    {
        std::ofstream f(authPath, std::ios::binary | std::ios::trunc);
        if (!f.is_open()) { std::filesystem::rename(keyBak, keyPath, ec); return false; }
        f.write(newHash.c_str(), newHash.size());
        if (!f.good())    { std::filesystem::rename(keyBak, keyPath, ec); return false; }
    }

    // Both files written; remove the backup
    std::filesystem::remove(keyBak, ec);
    return true;
}


void PasswordManager::RemoveTag(const std::string& tag)
{
    if (tag.empty()) return;

    bool changed = false;

    for (auto& entry : entries)
    {
        // Remove all occurrences of the tag
        auto newEnd = std::remove(entry.tags.begin(), entry.tags.end(), tag);
        if (newEnd != entry.tags.end())
        {
            entry.tags.erase(newEnd, entry.tags.end());
            changed = true;
        }
    }

    if (changed)
        SaveToFile();
}