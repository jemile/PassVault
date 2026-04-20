// PassVault
// Copyright (c) 2026 Trevor W
// SPDX-License-Identifier: MIT

#pragma once
#include <string>
#include <vector>
#include <algorithm>

// ============================================================
//  PasswordEntry  -  one stored credential
// ============================================================
struct PasswordEntry
{
    std::string id;
    std::string title;      // Display name  (e.g. "Gmail")
    std::string website;    // URL           (e.g. "https://gmail.com")
    std::string username;   // Login / email
    std::string password;   // Plaintext for now; ready for AES later
    std::string category;   // "Personal" | "Work" | "Finance" | "Social" | "Other"
    std::string notes;      // Freeform notes
    std::string createdAt;  // ISO-ish timestamp string
    std::string modifiedAt;
};

// ============================================================
//  PasswordManager  -  in-memory vault + file I/O
// ============================================================
class PasswordManager
{
public:
    std::vector<PasswordEntry> entries;
    std::string dataFilePath;

    PasswordManager();

    // File I/O  (format is encryption-ready: swap SaveToFile/LoadFromFile
    //            to call an AES routine before writing / after reading)
    bool LoadFromFile();
    bool SaveToFile();

    // CRUD
    void AddEntry(PasswordEntry entry);           // assigns id + timestamps
    void UpdateEntry(const PasswordEntry& entry); // matches on id
    void RemoveEntry(const std::string& id);
    int  FindIndexById(const std::string& id) const;

    // Utilities
    static std::string GenerateId();
    static std::string GetCurrentTimestamp();

    // Password tools
    static std::string GeneratePassword(int length,
                                        bool upper,
                                        bool lower,
                                        bool digits,
                                        bool symbols);

    // Returns 0-5 strength score
    static int         PasswordStrength(const std::string& pwd);
    static const char* PasswordStrengthLabel(int score);

private:
    // Escapes newlines and backslashes so values stay single-line in the file
    static std::string EscapeValue(const std::string& val);
    static std::string UnescapeValue(const std::string& val);

    std::vector<unsigned char> encryptionKey;   

    void LoadOrCreateEncryptionKey();           
};
