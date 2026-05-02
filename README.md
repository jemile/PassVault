# PassVault 🔒

**A modern, lightweight, and fully offline password manager for Windows.**

Built in C++ with Dear ImGui. No cloud. No accounts. No telemetry. Your data never leaves your machine.

### Screenshots
![PassVault Screenshot I](screenshot.png)
![PassVault Screenshot II](screenshot2.png)
![PassVault Screenshot III](screenshot3.png)

### ✨ Key Features

- **Beautiful custom dark/light UI** with drag-and-drop title bar
- **Strong XSalsa20-Poly1305 authenticated encryption** (libsodium crypto_secretbox)
- **Master password uses Argon2id key derivation** (libsodium)
- **Change master password** Can change master password in settings
- **Built-in password generator** with real-time strength preview
- **Smart search + filtering** by category or custom tags or folder
- **Custom folders** with drag and drop functionality
- **Custom tags** with fully customizable colors
- **Star / Favorite** important logins
- **Export / Import** encrypted vaults (secure backup & restore)
- **Toast Notifcations** success or error messages shown in a clean way
- **Self-updater** — checks for new versions automatically, user can choose to update from app
- **Copy buttons** with toast notifications
- Fully resizable + remembers window position
- 100% local storage — vaults saved as `.pv` files

### 🚀 Downloads (v1.4.0)

- **[PassVault-v1.4.0-Setup.exe]** (Recommended - Installer)  
  [Download](https://github.com/jemile/PassVault/releases/download/v1.4.0/PassVault-v1.4.0-Setup.exe)

- **[PassVault-v1.4.0-Win64.zip]** (Portable)  
  [Download](https://github.com/jemile/PassVault/releases/download/v1.4.0/PassVault-v1.4.0-Win64.zip)

**System Requirements:**
- Windows 10 or 11 (64-bit)
- Installation or just extract & run

> **Note:** x86 (32-bit) build is currently not supported.

### Tech Stack

- C++17
- Dear ImGui + custom renderer
- GLFW + OpenGL 3.3
- libsodium (crypto)
- Embedded Hatten font
- Embedded Sun and Moon font

### Getting Started

1. Download and run the installer or portable version
2. Create a new vault and set a **strong master password**
3. Start adding your logins

Your vault is automatically encrypted and saved locally.

### License

This project is licensed under the **MIT License** — see the [LICENSE](LICENSE) file for details.

Third-party licenses are listed in [THIRD_PARTY_LICENSES.txt](THIRD_PARTY_LICENSES.txt).

---

**Made with ❤️ in San Antonio, Texas.**

---

**Questions?** Feel free to open an issue on GitHub!
