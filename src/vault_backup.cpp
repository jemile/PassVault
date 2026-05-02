// PassVault
// Copyright (c) 2026 Trevor W
// SPDX-License-Identifier: MIT

// ============================================================
//  vault_backup.cpp  -  JSON export/import and encrypted backup
//  Contains: BuildJsonExport, ExportBackup, ParseJsonStream,
//            ImportBackup, and file-local JSON helper functions
//  See password_manager.cpp for vault serialization and CRUD
// ============================================================

#include <fstream>
#include <sstream>
#include <vector>

#include "password_manager.h"
#include "crypto.h"


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
        if (escaped)              { raw += line[i]; escaped = false; }
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
        o << "      \"folder\": \""   << JsonEscape(e.folder) << "\",\n";

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
//  ParseJsonStream  -  shared JSON parser (stream -> entries)
//  merge=true  -> skip entries whose id already exists
//  merge=false -> replace all entries
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
                v = ExtractJsonStr(line, "folder");   if (!v.empty()) current.folder   = v;
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
