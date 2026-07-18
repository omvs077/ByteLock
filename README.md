# ByteLock

> Zero-knowledge AES-256-GCM folder encryption for Windows.

ByteLock is a native Windows desktop application for encrypting local folders with true byte-level AES-256-GCM encryption. It runs entirely offline, stores no plaintext secrets, and never phones home.

## Features

- **Right-click folder locking** - "Lock with ByteLock" in Explorer's context menu
- **AES-256-GCM + Argon2id** - authenticated encryption with memory-hard key derivation (OpenSSL)
- **Decoy files** - locked folders are replaced by a hidden `.blk` container and a visible `.blocked` placeholder
- **Master Recovery Key** - a one-time, DPAPI-escrowed recovery key generated on first run, letting you recover a folder if you forget its password, without ByteLock ever storing your password
- **Mobile recovery** - optionally pair a phone over your local WiFi network (no app install, no camera on the desktop) to recover folders with a QR scan instead of typing your Master Recovery Key
- **Secure delete** - the original folder contents are overwritten with random data before removal, not just unlinked
- **Crash-safe** - tracks locked folders and self-heals missing decoy files if the app is interrupted mid-operation
- **No session cache** - every lock/unlock requires the password; nothing sensitive is kept in memory between actions
- **Per-user installer** - no admin rights required, installs to your local app data

## How it works

1. Right-click a folder -> **Lock with ByteLock** -> set a password.
2. The folder is packed into a hidden `.blk` container, encrypted with a key derived from your password via Argon2id, and sealed with AES-256-GCM. The original files are securely overwritten before deletion.
3. A visible `.blocked` placeholder takes the folder's place. Double-click it and enter your password to unlock.
4. If you forget a password, use **Settings -> Master Recovery** with your one-time Master Recovery Key (shown once on first launch - save it somewhere safe), or recover using a paired phone instead.

## Mobile recovery

Pair a phone once (Settings -> Master Recovery -> **Pair Mobile Device...**) so you can recover folders later with a QR scan instead of typing your Master Recovery Key.

- Desktop self-hosts a temporary local HTTP server on your LAN (only while a pairing/recovery dialog is open) and shows a QR code.
- Your phone scans the QR with its own camera - no camera use on the desktop, no app install on the phone.
- The phone generates an Ed25519 keypair client-side (via a bundled copy of TweetNaCl.js) and stores the private key in its own browser storage; only the public key is sent back to the desktop.
- Recovery works the same way: desktop shows a QR with a fresh one-time challenge, the phone signs it, and the desktop verifies the signature (OpenSSL Ed25519) against the stored public key.
- Both devices must be on the same WiFi network. Nothing secret (passwords, keys) ever crosses the network - only a public key and a signature.

## Installing

Download the latest installer from [Releases](https://github.com/omvs077/ByteLock/releases). It installs per-user (no admin required) and sets up the right-click menu, file association, and Start Menu shortcuts automatically.

On first launch you'll be shown a Master Recovery Key - save it before continuing. You can't recover a locked folder without it if you forget its password (unless you've paired a phone for mobile recovery).

## Building from source

### Prerequisites

- Visual Studio 2022 with the "Desktop development with C++" workload
- [Qt6](https://www.qt.io/download-qt-installer-oss) (MSVC 2022 64-bit kit)
- [vcpkg](https://github.com/microsoft/vcpkg), with OpenSSL and ZXing installed:
```
vcpkg install openssl:x64-windows zxing-cpp:x64-windows
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
| Engine (`src/engine`) | Framework-independent C++20: `CryptoEngine` (AES-256-GCM, Argon2id), `FolderPacker` (`.blk` container format, secure wipe), `SecureBytes` (RAII memory wipe) |
| `MasterConfig` | Two-salt Argon2id verifier + DPAPI-protected escrow key, locked-folder tracking, orphan repair |
| `LocalPairingServer`, `MobilePairing` | Hand-rolled local HTTP server (QTcpServer) for phone pairing/recovery, QR generation (ZXing), Ed25519 signature verification (OpenSSL) |

### Security notes

- Argon2id uses **separate salts** for the stored password verifier and the derivable key - reusing one salt for both would leak the key deterministically.
- The Master Recovery Key never touches disk in plaintext; it only unwraps a DPAPI-protected escrow key, which in turn wraps each folder's data key.
- No password or derived key is cached between operations.
- The local pairing/recovery server binds a per-installation random port, accepts single-use tokens, and validates the Host header on every request. Nothing secret crosses the network during pairing or recovery.

## License

MIT - see [LICENSE](LICENSE).

Built with Qt6 (LGPLv3, dynamically linked), OpenSSL (Apache 2.0), ZXing-C++ (Apache 2.0), and TweetNaCl.js (public domain / Unlicense).