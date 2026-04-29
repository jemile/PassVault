// PassVault
// Copyright (c) 2026 Trevor W
// SPDX-License-Identifier: MIT

// ============================================================
//  password_manager.cpp  -  Vault serialization, CRUD, and utilities
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
        bool   hasTags      = false;  // distinguishes new vs legacy format
        std::string legacyCat;        // old "category" field fallback

        entries.clear();

        while (std::getline(iss, line))
        {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line[0] == '#') continue;

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
//  JSON helpers (file-local)
// ============================================================
static std::string JsonEscape(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s)
    {
        switch (c)
        {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if ((unsigned char)c < 0x20)
                {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                    out += buf;
                }
                else out += c;
        }
    }
    return out;
}

static std::string JsonUnescape(const std::string& s)
{
    std::string out;
    for (size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] == '\\' && i + 1 < s.size())
        {
            char n = s[++i];
            if      (n == 'n')  out += '\n';
            else if (n == 'r')  out += '\r';
            else if (n == 't')  out += '\t';
            else if (n == '"')  out += '"';
            else if (n == '\\') out += '\\';
            else                out += n;
        }
        else out += s[i];
    }
    return out;
}

// Extract value from:  "key": "value"
// Returns empty string if key not found.
static std::string ExtractJsonStr(const std::string& line, const std::string& key)
{
    std::string search = "\"" + key + "\": \"";
    size_t pos = line.find(search);
    if (pos == std::string::npos) return "";
    pos += search.size();

    std::string raw;
    bool escaped = false;
    for (size_t i = pos; i < line.size(); ++i)
    {
        if (escaped)        { raw += line[i]; escaped = false; }
        else if (line[i] == '\\') { raw += '\\'; escaped = true; }
        else if (line[i] == '"')  break;
        else                      raw += line[i];
    }
    return JsonUnescape(raw);
}

// Extract value from:  "key": true/false
static bool ExtractJsonBool(const std::string& line, const std::string& key)
{
    std::string search = "\"" + key + "\": ";
    size_t pos = line.find(search);
    if (pos == std::string::npos) return false;
    return line.substr(pos + search.size(), 4) == "true";
}

// Extract array from:  "tags": ["a", "b", "c"]
static std::vector<std::string> ExtractJsonTags(const std::string& line)
{
    std::vector<std::string> result;
    size_t start = line.find('[');
    size_t end   = line.rfind(']');
    if (start == std::string::npos || end == std::string::npos || start >= end)
        return result;

    size_t i = start + 1;
    while (i < end)
    {
        size_t q1 = line.find('"', i);
        if (q1 == std::string::npos || q1 >= end) break;
        size_t q2 = line.find('"', q1 + 1);
        if (q2 == std::string::npos || q2 > end)  break;
        std::string tag = JsonUnescape(line.substr(q1 + 1, q2 - q1 - 1));
        if (!tag.empty()) result.push_back(tag);
        i = q2 + 1;
    }
    return result;
}


// ============================================================
//  BuildJsonExport  -  shared JSON builder (in-memory string)
// ============================================================
std::string PasswordManager::BuildJsonExport() const
{
    std::ostringstream o;
    o << "{\n";
    o << "  \"passvault_version\": 1,\n";
    o << "  \"exported_at\": \"" << JsonEscape(GetCurrentTimestamp()) << "\",\n";
    o << "  \"entries\": [\n";

    for (size_t ei = 0; ei < entries.size(); ++ei)
    {
        const auto& e    = entries[ei];
        bool        last = (ei == entries.size() - 1);

        o << "    {\n";
        o << "      \"id\": \""       << JsonEscape(e.id)        << "\",\n";
        o << "      \"title\": \""    << JsonEscape(e.title)     << "\",\n";
        o << "      \"website\": \""  << JsonEscape(e.website)   << "\",\n";
        o << "      \"username\": \"" << JsonEscape(e.username)  << "\",\n";
        o << "      \"password\": \"" << JsonEscape(e.password)  << "\",\n";
        o << "      \"notes\": \""    << JsonEscape(e.notes)     << "\",\n";
        o << "      \"created\": \""  << JsonEscape(e.createdAt)  << "\",\n";
        o << "      \"modified\": \"" << JsonEscape(e.modifiedAt) << "\",\n";
        o << "      \"isFavorite\": " << (e.isFavorite ? "true" : "false") << ",\n";

        o << "      \"tags\": [";
        for (size_t ti = 0; ti < e.tags.size(); ++ti)
        {
            if (ti > 0) o << ", ";
            o << "\"" << JsonEscape(e.tags[ti]) << "\"";
        }
        o << "],\n";

        o << "      \"history\": [\n";
        for (size_t hi = 0; hi < e.passwordHistory.size(); ++hi)
        {
            bool lastH = (hi == e.passwordHistory.size() - 1);
            o << "        { \"ts\": \""
              << JsonEscape(e.passwordHistory[hi].first)
              << "\", \"pw\": \""
              << JsonEscape(e.passwordHistory[hi].second)
              << "\" }" << (lastH ? "" : ",") << "\n";
        }
        o << "      ]\n";
        o << "    }" << (last ? "" : ",") << "\n";
    }

    o << "  ]\n}\n";
    return o.str();
}


// ============================================================
//  ExportBackup / ImportBackup
//  Encrypted .pvbackup using the vault's own key - no extra
//  password needed.  Layout: PVBKVLT(8) | nonce(24) | MAC+ct
// ============================================================
static const unsigned char VAULT_BACKUP_MAGIC[8] =
    { 'P','V','B','K','V','L','T','\0' };

bool PasswordManager::ExportBackup(const std::string& path) const
{
    std::string json      = BuildJsonExport();
    auto        encrypted = CRYPTO::EncryptWithKey(json, encryptionKey);
    if (encrypted.empty()) return false;

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    file.write(reinterpret_cast<const char*>(VAULT_BACKUP_MAGIC), 8);
    file.write(reinterpret_cast<const char*>(encrypted.data()), encrypted.size());
    return true;
}


// ============================================================
//  ParseJsonStream  -  shared JSON parser (stream → entries)
//  merge=true  → skip entries whose id already exists
//  merge=false → replace all entries
// ============================================================
bool PasswordManager::ParseJsonStream(std::istream& stream, bool merge)
{
    std::vector<PasswordEntry> imported;
    PasswordEntry current;
    bool inEntry   = false;
    bool inHistory = false;

    std::string line;
    while (std::getline(stream, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();

        size_t ns = line.find_first_not_of(" \t");
        if (ns != std::string::npos) line = line.substr(ns);

        if (!inEntry && !inHistory)
        {
            if (line == "{")
            {
                current  = PasswordEntry{};
                inEntry  = true;
            }
        }
        else if (inHistory)
        {
            if (line == "]") { inHistory = false; }
            else if (line.find("\"ts\"") != std::string::npos)
            {
                std::string ts = ExtractJsonStr(line, "ts");
                std::string pw = ExtractJsonStr(line, "pw");
                if (!ts.empty())
                    current.passwordHistory.push_back({ ts, pw });
            }
        }
        else  // inEntry
        {
            if (line == "}," || line == "}")
            {
                if (!current.id.empty() || !current.title.empty())
                    imported.push_back(current);
                inEntry = false;
            }
            else if (line.find("\"history\"") != std::string::npos
                     && line.find('[') != std::string::npos)
            {
                inHistory = true;
            }
            else
            {
                std::string v;
                v = ExtractJsonStr(line, "id");       if (!v.empty()) current.id       = v;
                v = ExtractJsonStr(line, "title");    if (!v.empty()) current.title    = v;
                v = ExtractJsonStr(line, "website");  if (!v.empty()) current.website  = v;
                v = ExtractJsonStr(line, "username"); if (!v.empty()) current.username = v;
                v = ExtractJsonStr(line, "password"); if (!v.empty()) current.password = v;
                v = ExtractJsonStr(line, "notes");    if (!v.empty()) current.notes    = v;
                v = ExtractJsonStr(line, "created");  if (!v.empty()) current.createdAt  = v;
                v = ExtractJsonStr(line, "modified"); if (!v.empty()) current.modifiedAt = v;

                if (line.find("\"isFavorite\"") != std::string::npos)
                    current.isFavorite = ExtractJsonBool(line, "isFavorite");
                if (line.find("\"tags\"") != std::string::npos)
                    current.tags = ExtractJsonTags(line);
            }
        }
    }

    if (merge)
    {
        for (const auto& imp : imported)
        {
            bool found = false;
            for (const auto& ex : entries)
                if (ex.id == imp.id) { found = true; break; }
            if (!found)
            {
                PasswordEntry e = imp;
                if (e.id.empty()) e.id = GenerateId();
                entries.push_back(e);
            }
        }
    }
    else
    {
        entries = imported;
        for (auto& e : entries)
            if (e.id.empty()) e.id = GenerateId();
    }

    return SaveToFile();
}


// ============================================================
//  ImportBackup  -  decrypt with vault key, then parse
// ============================================================
bool PasswordManager::ImportBackup(const std::string& path, bool merge)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;
    std::streamsize sz = file.tellg();
    file.seekg(0, std::ios::beg);
    if (sz < 8) return false;

    std::vector<unsigned char> data(sz);
    if (!file.read(reinterpret_cast<char*>(data.data()), sz)) return false;

    // Validate magic header
    if (memcmp(data.data(), VAULT_BACKUP_MAGIC, 8) != 0) return false;

    // Strip magic, decrypt remainder
    std::vector<unsigned char> payload(data.begin() + 8, data.end());
    std::string json;
    try { json = CRYPTO::DecryptWithKey(payload, encryptionKey); }
    catch (...) { return false; }

    if (json.empty()) return false;

    std::istringstream ss(json);
    return ParseJsonStream(ss, merge);
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
