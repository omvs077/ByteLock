# ByteLock

> Zero-knowledge AES-256-GCM folder encryption for Windows.

ByteLock is a native Windows desktop application for encrypting local folders with true byte-level AES-256-GCM encryption. It runs entirely offline, stores no plaintext secrets, and never phones home.

## Features

- **Right-click folder locking** - "Lock with ByteLock" in Explorer's context menu
- **AES-256-GCM + Argon2id** - authenticated encryption with memory-hard key derivation (OpenSSL)
- **Decoy files** - locked folders are replaced by a hidden `.blk` container and a visible `.blocked` placeholder
- **Master Recovery Key** - a one-time, DPAPI-escrowed recovery key generated on first run, letting you recover a folder if you forget its password, without ByteLock ever storing your password
- **Crash-safe** - tracks locked folders and self-heals missing decoy files if the app is interrupted mid-operation
- **No session cache** - every lock/unlock requires the password; nothing sensitive is kept in memory between actions
- **Per-user installer** - no admin rights required, installs to your local app data

## How it works

1. Right-click a folder -> **Lock with ByteLock** -> set a password.
2. The folder is packed into a hidden `.blk` container, encrypted with a key derived from your password via Argon2id, and sealed with AES-256-GCM.
3. A visible `.blocked` placeholder takes the folder's place. Double-click it and enter your password to unlock.
4. If you forget a password, use **Settings -> Master Recovery** with your one-time Master Recovery Key (shown once on first launch - save it somewhere safe).

## Installing

Download the latest installer from [Releases](https://github.com/omvs077/ByteLock/releases). It installs per-user (no admin required) and sets up the right-click menu, file association, and Start Menu shortcuts automatically.

On first launch you'll be shown a Master Recovery Key - save it before continuing. You can't recover a locked folder without it if you forget its password.

## Building from source

### Prerequisites

- Visual Studio 2022 with the "Desktop development with C++" workload
- [Qt6](https://www.qt.io/download-qt-installer-oss) (MSVC 2022 64-bit kit)
- [vcpkg](https://github.com/microsoft/vcpkg), with OpenSSL installed:
  ```
  vcpkg install openssl:x64-windows
  ```
- [NSIS](https://nsis.sourceforge.io/) (only needed to build the installer)

### Build

From the **x64 Native Tools Command Prompt for VS 2022**:

```
cmake --preset default
cmake --build build/default
```

For a release build (used for the installer):

```
cmake --preset release
cmake --build build/release --config Release
```

### Build the installer

```
makensis installer.nsi
```

## Architecture

| Layer | Responsibility |
|---|---|
| UI (`src/ui`, `src/*Dialog.cpp`) | Qt6 Widgets - no crypto logic |
| Engine (`src/engine`) | Framework-independent C++20: `CryptoEngine` (AES-256-GCM, Argon2id), `FolderPacker` (`.blk` container format), `SecureBytes` (RAII memory wipe) |
| `MasterConfig` | Two-salt Argon2id verifier + DPAPI-protected escrow key, locked-folder tracking, orphan repair |

### Security notes

- Argon2id uses **separate salts** for the stored password verifier and the derivable key - reusing one salt for both would leak the key deterministically.
- The Master Recovery Key never touches disk in plaintext; it only unwraps a DPAPI-protected escrow key, which in turn wraps each folder's data key.
- No password or derived key is cached between operations.

## License

MIT - see [LICENSE](LICENSE).

Built with Qt6 (LGPLv3, dynamically linked) and OpenSSL (Apache 2.0).
