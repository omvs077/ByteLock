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
  Select the .blocked file, enter your Master Recovery Key.

UNINSTALLING
  You will be asked for your Master Recovery Key. All locked folders
  are automatically unlocked back to plain folders before removal.
