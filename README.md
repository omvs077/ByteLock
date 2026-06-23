# ByteLock

> Production-grade, zero-knowledge AES-256 folder encryption for Windows.

**Status:** 🚧 Early development — project scaffold complete, core engine in progress.

## Overview

ByteLock is a native Windows desktop application for encrypting local folders using true byte-level AES-256-GCM encryption. It runs entirely offline with no cloud dependency, and is designed with a "simple by default, powerful when needed" philosophy - single-password onboarding for casual use, with advanced security extensions (anti-brute-force lockout, local P2P mobile recovery, hardware panic hotkeys) available for power users.

## Architecture

- **Pattern:** MVVM (Model-View-ViewModel)
- **UI:** Qt6 Widgets
- **Language:** C++20
- **Cryptography:** OpenSSL (AES-256-GCM + Argon2id key derivation)
- **Build system:** CMake + vcpkg

| Layer | Responsibility |
|---|---|
| View | Qt Widgets UI - no business logic |
| ViewModel | QObject-based controllers bridging UI and engine, background threading |
| Model/Engine | Pure C++20 - crypto, file I/O, Win32 APIs. Framework-independent and testable standalone. |

## Planned Features

- [x] Project scaffold (Qt6 + OpenSSL + CMake build verified)
- [ ] Core crypto engine: AES-256-GCM file encryption/decryption
- [ ] Argon2id key derivation from master password
- [ ] Recursive folder packing/unpacking into secure containers
- [ ] Dashboard UI: drag-and-drop folder locking, live status list
- [ ] Onboarding flow: master password + mandatory recovery key generation
- [ ] Settings: start with Windows, shell context menu integration, idle auto-lock
- [ ] Anti-brute-force exponential backoff with tamper-resistant lockout state
- [ ] Recovery: text key, .dat recovery file, local P2P mobile biometric auth
- [ ] NSIS installer packaging

## Building from source

### Prerequisites

- Visual Studio 2022+ with the "Desktop development with C++" workload
- Qt6 (MSVC 2022 64-bit kit) - https://www.qt.io/download-qt-installer-oss
- vcpkg (https://github.com/microsoft/vcpkg), with OpenSSL installed: vcpkg install openssl:x64-windows

### Build

1. Open the repository folder in Visual Studio (File -> Open -> Folder)
2. Visual Studio will auto-configure via CMakePresets.json
3. Select the default configure preset and ByteLock.exe as the startup item
4. Build (Ctrl+Shift+B) and run (F5)

## License

This project uses Qt6 under the LGPLv3 license (dynamically linked). See https://www.qt.io/licensing/ for details.

Application source code license: TBD.
'@ | Set-Content -Path "$root\README.md" -Encoding UTF8

Write-Host "Created .gitignore and README.md" -ForegroundColor Green