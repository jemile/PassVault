// PassVault
// Copyright (c) 2026 Trevor W
// SPDX-License-Identifier: MIT

#pragma once
#include <string>
#include <vector>

// ============================================================
//  PasswordEntry  -  one stored credential
// ============================================================
struct PasswordEntry
{
    std::string id;
    std::string title;      // Display name  (e.g. "Gmail")
    std::string website;    // URL           (e.g. "https://gmail.com")
    std::string username;   // Login / email
    std::string password;   // Plaintext (vault file is encrypted at rest)
    std::vector<std::string> tags;  // User-defined tags; replaces single category
    std::string notes;      // Freeform notes
    std::string folder;     // Optional folder name (flat; empty = no folder)
    std::string createdAt;  // ISO-ish timestamp string
    std::string modifiedAt;

    // ---- New fields ----
    bool isFavorite = false;

    // Previous passwords: each element is { timestamp, old_password }
    // Capped at 10 entries (oldest is dropped when limit is reached).
    std::vector<std::pair<std::string, std::string>> passwordHistory;
};

// ============================================================
//  PasswordManager  -  in-memory vault + file I/O
// ============================================================
class PasswordManager
{
public:
    std::vector<PasswordEntry> entries;
    std::vector<std::string>  knownFolders;   // explicitly created folders (persisted)
    std::string dataFilePath;

    PasswordManager();

    // Master password management
    bool HasMasterPassword() const;
    bool SetupMasterPassword(const std::string& password);
    bool UnlockWithMasterPassword(const std::string& password);
    bool VerifyMasterPassword(const std::string& password) const;

    // Encrypted vault file I/O
    bool LoadFromFile();
    bool SaveToFile();

    // CRUD
    void AddEntry(PasswordEntry entry);             // assigns id + timestamps
    void UpdateEntry(const PasswordEntry& entry);   // matches on id; auto-saves old password to history
    void RemoveEntry(const std::string& id);
    int  FindIndexById(const std::string& id) const;

    // Encrypted backup (.pvbackup) - uses the vault's own key; no extra password needed
    bool ExportBackup(const std::string& path) const;
    bool ImportBackup(const std::string& path, bool merge);  // merge=true keeps existing entries

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

    // Removes a tag from ALL entries that have it and saves the vault
    void RemoveTag(const std::string& tag);

    // Folder management
    void AddFolder(const std::string& name);
    // Returns false (and does nothing) if any entry still references the folder.
    bool RemoveFolder(const std::string& name);
    // All folder names: knownFolders + any referenced by entries (for import compat).
    std::vector<std::string> GetAllFolders() const;

    // Re-hashes and re-wraps the vault key under a new master password.
    // Returns false if currentPw is wrong or the write fails.
    bool ChangeMasterPassword(const std::string& currentPw, const std::string& newPw);

private:
    // Single-line escaping for the encrypted vault file format
    static std::string EscapeValue(const std::string& val);
    static std::string UnescapeValue(const std::string& val);

    // Shared JSON helpers used by ExportBackup
    std::string BuildJsonExport() const;
    // Shared JSON parser used by ImportBackup
    bool ParseJsonStream(std::istream& stream, bool merge);

    std::vector<unsigned char> encryptionKey;

    void LoadOrCreateEncryptionKey();
};
