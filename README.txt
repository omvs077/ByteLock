ByteLock - Quick Start

LOCKING A FOLDER
  Right-click any folder in File Explorer -> "Lock with ByteLock"
  Set a password. The folder is replaced by a hidden .blk container
  and a visible .blocked placeholder file.

UNLOCKING
  Double-click the .blocked file and enter your password.

FIRST RUN - MASTER RECOVERY KEY
  On first launch, ByteLock generates a one-time Master Recovery Key.
  SAVE THIS KEY SOMEWHERE SAFE (password manager, printed copy).
  It is the only way to recover a folder if you forget its password.
  It is never stored by ByteLock and cannot be regenerated.

RECOVERING A FOLDER (forgot password)
  Open ByteLock -> Settings -> Master Recovery -> "Recover a Folder..."
  Select the .blocked file, then either enter your Master Recovery Key
  or, if you have paired a phone (see below), recover using your phone instead.

MOBILE RECOVERY (optional)
  Pair a phone once so you can recover folders without typing your
  Master Recovery Key each time.

  To pair:
    Open ByteLock -> Settings -> Master Recovery -> "Pair Mobile Device..."
    Enter your Master Recovery Key, then scan the QR code with your
    phone's camera. Your phone and this computer must be on the same
    WiFi network. No app install is required on the phone.

  To recover using your paired phone:
    Open ByteLock -> Settings -> Master Recovery -> "Recover a Folder..."
    Select the .blocked file, choose "Yes" when asked to use your phone,
    then scan the QR code with your phone.

  To disconnect a paired phone:
    Open ByteLock -> Settings -> Master Recovery -> the pairing button
    will show "Mobile Device Paired - Tap to Disconnect". Tap it and
    enter your Master Recovery Key to disconnect.

UNINSTALLING
  You will be asked for your Master Recovery Key. All locked folders
  are automatically unlocked back to plain folders before removal.
