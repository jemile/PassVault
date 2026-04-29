// PassVault
// Copyright (c) 2026 Trevor W
// SPDX-License-Identifier: MIT

// ============================================================
//  updater.cpp  -  GitHub release check
//
//  Rendering lives in screens.cpp (RenderSettingsScreen).
// ============================================================

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "updater.h"

#include <vector>
#include <windows.h>
#include <wininet.h>
#include <shellapi.h>
#include <thread>
#include <cstdio>
#include <string>

#pragma comment(lib, "wininet.lib")

namespace UPDATER {
    // ── Storage ────────────────────────────────────────────────────
    std::atomic<State> state { State::Idle };
    std::string        latestVersion;
    std::string        downloadUrl;
    std::string        errorMessage;
    std::atomic<float> downloadProgress { 0.0f };
    static std::string s_extractDir;   // set by DownloadThread, read by ApplyUpdate

    // Used in both FetchUrl + DownloadToFile
    static const char* const USER_AGENT = "User-Agent: PassVault/1.1\r\n";
    static const DWORD       USER_AGENT_LEN = sizeof("User-Agent: PassVault/1.1\r\n") - 1;

    // ── HTTP ───────────────────────────────────────────────────────
    static std::string FetchUrl(const std::string& url)
    {
        std::string result;

        HINTERNET hSession = InternetOpenA(
            "PassVault/1.1",
            INTERNET_OPEN_TYPE_PRECONFIG,
            NULL, NULL, 0);
        if (!hSession) return "";

        // GitHub API requires a User-Agent header.
        HINTERNET hConnect = InternetOpenUrlA(
            hSession, url.c_str(),
            USER_AGENT, USER_AGENT_LEN,
            INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE |
            INTERNET_FLAG_NO_CACHE_WRITE, 0);

        if (hConnect) {
            char  buf[4096];
            DWORD read = 0;
            while (InternetReadFile(hConnect, buf, sizeof(buf), &read) && read > 0)
                result.append(buf, read);
            InternetCloseHandle(hConnect);
        }
        InternetCloseHandle(hSession);
        return result;
    }


    // ── JSON helper ────────────────────────────────────────────────
    static std::string JsonString(const std::string& json, const char* key)
    {
        std::string needle = std::string("\"") + key + "\"";
        size_t pos = json.find(needle);
        if (pos == std::string::npos) return "";
        pos = json.find('"', pos + needle.size());
        if (pos == std::string::npos) return "";
        size_t end = json.find('"', pos + 1);
        if (end == std::string::npos) return "";
        return json.substr(pos + 1, end - pos - 1);
    }

    static std::string GetVersionedExeName()
    {
        if (latestVersion.empty()) return "PassVault.exe";
        return "PassVault-" + latestVersion + "-Win64.exe";
    }

    // ── Version comparison ─────────────────────────────────────────
    static bool ParseVersion(const std::string& v, int& maj, int& min, int& pat)
    {
        const char* s = v.c_str();
        if (*s == 'v' || *s == 'V') ++s;
        return sscanf_s(s, "%d.%d.%d", &maj, &min, &pat) == 3;
    }

    static bool IsNewer(const std::string& candidate, const std::string& current)
    {
        int nmaj=0, nmin=0, npat=0, cmaj=0, cmin=0, cpat=0;
        if (!ParseVersion(candidate, nmaj, nmin, npat)) return false;
        if (!ParseVersion(current,   cmaj, cmin, cpat)) return false;
        if (nmaj != cmaj) return nmaj > cmaj;
        if (nmin != cmin) return nmin > cmin;
        return npat > cpat;
    }


    // ── Background worker ──────────────────────────────────────────
    static void CheckThread()
    {
        std::string json = FetchUrl(RELEASES_API);
        if (json.empty()) {
            errorMessage = "Could not reach GitHub. Check your connection.";
            state.store(State::Error, std::memory_order_release);
            return;
        }

        // GitHub returns {"message":"..."} when something goes wrong
        // (rate limit, repo not found, auth required, etc.)
        std::string apiError = JsonString(json, "message");
        if (!apiError.empty()) {
            // Give a friendlier label for the most common case
            if (json.find("rate limit") != std::string::npos ||
                json.find("rate_limit") != std::string::npos)
            {
                errorMessage = "GitHub rate limit hit. Wait a few minutes and try again.";
            }
            else
            {
                // Truncate long GitHub messages to something displayable
                errorMessage = apiError.size() > 80
                    ? apiError.substr(0, 77) + "..."
                    : apiError;
            }
            state.store(State::Error, std::memory_order_release);
            return;
        }

        std::string tag = JsonString(json, "tag_name");
        std::string url = JsonString(json, "html_url");

        if (tag.empty()) {
            errorMessage = "Unexpected response from GitHub. No release tag found.";
            state.store(State::Error, std::memory_order_release);
            return;
        }

        latestVersion = tag;
        downloadUrl   = url.empty() ? RELEASES_PAGE : url;
        errorMessage.clear();

        std::atomic_thread_fence(std::memory_order_seq_cst);

        state.store(
            IsNewer(tag, CURRENT_VERSION) ? State::Available : State::UpToDate,
            std::memory_order_release);
    }


    // ── Strip trailing backslashes from a path ────────────────────
    static std::string StripSlash(std::string p)
    {
        while (!p.empty() && (p.back() == '\\' || p.back() == '/'))
            p.pop_back();
        return p;
    }


    // ── Binary file download with progress ────────────────────────
    // GitHub release assets redirect to objects.githubusercontent.com;
    // INTERNET_FLAG_RELOAD + no cache ensures we follow the redirect.
    static bool DownloadToFile(const std::string& url, const std::string& outPath)
    {
        HINTERNET hSession = InternetOpenA("PassVault/1.1",
            INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
        if (!hSession) return false;

        HINTERNET hConn = InternetOpenUrlA(hSession, url.c_str(),
            USER_AGENT, USER_AGENT_LEN,
            INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE |
            INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_KEEP_CONNECTION, 0);
        if (!hConn) { InternetCloseHandle(hSession); return false; }

        // Try to get content-length for progress (may be 0 on redirect)
        DWORD totalSize = 0, szDW = sizeof(totalSize);
        HttpQueryInfoA(hConn, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER,
            &totalSize, &szDW, nullptr);

        FILE* f = nullptr;
        fopen_s(&f, outPath.c_str(), "wb");
        if (!f) { InternetCloseHandle(hConn); InternetCloseHandle(hSession); return false; }

		std::vector<char> buf(65536);   // On heap to avoid stack overflow on large files
        DWORD read = 0, received = 0;
        bool  ok = true;

        while (InternetReadFile(hConn, buf.data(), (DWORD)buf.size(), &read) && read > 0)
        {
            if (fwrite(buf.data(), 1, read, f) != read) { ok = false; break; }
            received += read;
            if (totalSize > 0)
                downloadProgress.store((float)received / (float)totalSize,
                    std::memory_order_relaxed);
        }

        fclose(f);
        InternetCloseHandle(hConn);
        InternetCloseHandle(hSession);

        if (!ok) return false;

        // A valid zip is always several hundred KB at minimum.
        // If we got < 50 KB the server returned an error page, not the zip.
        HANDLE hFile = CreateFileA(outPath.c_str(), GENERIC_READ, FILE_SHARE_READ,
            nullptr, OPEN_EXISTING, 0, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) return false;
        DWORD fileSize = GetFileSize(hFile, nullptr);
        CloseHandle(hFile);
        if (fileSize < 50 * 1024)
        {
            DeleteFileA(outPath.c_str());
            errorMessage = "Download returned an invalid file (too small). "
                "Check the release exists on GitHub.";
            return false;
        }

        return true;
    }


    // ── Zip extraction via a temp .ps1 script ─────────────────────
    // Writing a .ps1 file avoids all command-line escaping issues.
    // $ErrorActionPreference = 'Stop' ensures a non-zero exit on failure.
    static bool ExtractZip(const std::string& zipPath, const std::string& destDir)
    {
        std::string dest = StripSlash(destDir);
        CreateDirectoryA(dest.c_str(), nullptr);

        // tar.exe is guaranteed in System32 on modern Windows
        const std::string tarPath = "C:\\Windows\\System32\\tar.exe";
        std::string cmd = tarPath + " -xf \"" + zipPath + "\" -C \"" + dest + "\"";

        char cmdBuf[2048] = {};
        if (cmd.size() >= sizeof(cmdBuf))
            return false;
        memcpy(cmdBuf, cmd.c_str(), cmd.size());

        STARTUPINFOA si = {};
        PROCESS_INFORMATION pi = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        bool launched = CreateProcessA(nullptr, cmdBuf, nullptr, nullptr,
            FALSE, CREATE_NO_WINDOW,
            nullptr, nullptr, &si, &pi) != 0;

        if (!launched) {
            errorMessage = "Could not launch tar.exe for extraction.";
            return false;
        }

        WaitForSingleObject(pi.hProcess, 90000);  // 90 s max
        DWORD exitCode = 1;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        if (exitCode != 0) {
            errorMessage = "Extraction failed (tar.exe returned error). "
                "Update package may be corrupt.";
            return false;
        }

        // Final sanity check
        std::string exeCheck = dest + "\\" + GetVersionedExeName();
        if (GetFileAttributesA(exeCheck.c_str()) == INVALID_FILE_ATTRIBUTES) {
            errorMessage = "Extraction succeeded but " + GetVersionedExeName() + " is missing.";
            return false;
        }

        return true;
    }


    // ── Write + launch the self-deleting updater batch script ─────
    static bool LaunchUpdaterScript(const std::string& srcDir,
                                    const std::string& curExePath)
    {
        // Paths with no trailing backslash so the batch copy is unambiguous
        std::string src = StripSlash(srcDir);

        char tmpBuf[MAX_PATH];
        GetTempPathA(MAX_PATH, tmpBuf);
        std::string scriptPath = std::string(tmpBuf) + "pv_update.bat";

        FILE* f = nullptr;
        fopen_s(&f, scriptPath.c_str(), "w");
        if (!f) return false;

        fprintf(f, "@echo off\n");
        // Give the old process a moment to fully exit
        fprintf(f, "timeout /t 3 /nobreak > nul\n");
        // Copy the new exe over the current one (paths in quotes handle spaces)
        std::string versionedExe = GetVersionedExeName();
        fprintf(f, "copy /Y \"%s\\%s\" \"%s\"\n",
            src.c_str(), versionedExe.c_str(), curExePath.c_str());
        // Relaunch from the same location
        fprintf(f, "start \"\" \"%s\"\n", curExePath.c_str());
        // Clean up the extracted files
        fprintf(f, "rmdir /S /Q \"%s\"\n", src.c_str());
        // Self-delete the script
        fprintf(f, "(goto) 2>nul & del \"%%~f0\"\n");
        fclose(f);

        // Run the script hidden and detached
        SHELLEXECUTEINFOA sei = {};
        sei.cbSize    = sizeof(sei);
        sei.fMask     = SEE_MASK_NOCLOSEPROCESS;
        sei.lpVerb    = "open";
        sei.lpFile    = scriptPath.c_str();
        sei.nShow     = SW_HIDE;
        if (!ShellExecuteExA(&sei)) return false;

		// Close the process handle immediately since we don't need to wait on it
        // Check for 0 
        if (sei.hProcess != NULL)
            CloseHandle(sei.hProcess);

        return true;
    }


    // ── Download + extract thread ──────────────────────────────────
    static void DownloadThread()
    {
        downloadProgress.store(0.0f, std::memory_order_relaxed);

        // Asset URL is fully deterministic from the tag name:
        // https://github.com/owner/repo/releases/download/vX.Y.Z/PassVault-vX.Y.Z-Win64.zip
        std::string zipName  = "PassVault-" + latestVersion + "-Win64.zip";
        std::string assetUrl =
            "https://github.com/jemile/PassVault/releases/download/"
            + latestVersion + "/" + zipName;

        char tmpBuf[MAX_PATH];
        GetTempPathA(MAX_PATH, tmpBuf);
        std::string zipPath  = std::string(tmpBuf) + zipName;
        s_extractDir         = std::string(tmpBuf) + "PassVault_update";  // no trailing slash

        if (!DownloadToFile(assetUrl, zipPath))
        {
            // errorMessage set
            if (errorMessage.empty())
                errorMessage = "Download failed. Check your connection.";
            state.store(State::Error, std::memory_order_release);
            return;
        }

        downloadProgress.store(1.0f, std::memory_order_relaxed);

        if (!ExtractZip(zipPath, s_extractDir))
        {
            DeleteFileA(zipPath.c_str());
            state.store(State::Error, std::memory_order_release);
            return;
        }

        DeleteFileA(zipPath.c_str());  // zip no longer needed

        std::atomic_thread_fence(std::memory_order_seq_cst);
        state.store(State::ReadyToApply, std::memory_order_release);
    }


    // ── Public API ─────────────────────────────────────────────────
    void StartCheck()
    {
        if (state.load() == State::Checking) return;
        state.store(State::Checking);
        std::thread(CheckThread).detach();
    }

    void StartDownload()
    {
        if (state.load() == State::Downloading) return;
        state.store(State::Downloading);
        std::thread(DownloadThread).detach();
    }

    void ApplyUpdate()
    {
        char exePath[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        LaunchUpdaterScript(s_extractDir, std::string(exePath));
    }

} 