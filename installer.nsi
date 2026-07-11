!include "MUI2.nsh"
!include "LogicLib.nsh"

Name "ByteLock"
OutFile "ByteLock-Setup.exe"
InstallDir "$LocalAppData\ByteLock"
InstallDirRegKey HKCU "Software\ByteLock" "InstallDir"
RequestExecutionLevel user

!define MUI_ICON "resources\icons\app.ico"
!define MUI_FINISHPAGE_RUN "$INSTDIR\ByteLock.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch ByteLock now"
!define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\README.txt"
!define MUI_FINISHPAGE_SHOWREADME_TEXT "View usage guide"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Section "Install"
    SetOutPath "$INSTDIR"
    File /r "build\release\*.*"
    File "README.txt"

    WriteRegStr HKCU "Software\ByteLock" "InstallDir" "$INSTDIR"
    WriteUninstaller "$INSTDIR\Uninstall.exe"

    CreateDirectory "$SMPROGRAMS\ByteLock"
    CreateShortCut "$SMPROGRAMS\ByteLock\ByteLock.lnk" "$INSTDIR\ByteLock.exe"
    CreateShortCut "$SMPROGRAMS\ByteLock\Usage Guide.lnk" "$INSTDIR\README.txt"
    CreateShortCut "$SMPROGRAMS\ByteLock\Uninstall ByteLock.lnk" "$INSTDIR\Uninstall.exe"

    WriteRegStr HKCU "Software\Classes\.blocked" "" "ByteLock.blocked"
    WriteRegStr HKCU "Software\Classes\ByteLock.blocked" "NeverShowExt" ""
    WriteRegStr HKCU "Software\Classes\ByteLock.blocked\DefaultIcon" "" "$INSTDIR\ByteLock.exe,1"
    WriteRegStr HKCU "Software\Classes\ByteLock.blocked\shell\open\command" "" '"$INSTDIR\ByteLock.exe" "%1"'

    WriteRegStr HKCU "Software\Classes\Directory\shell\ByteLock" "" "Lock with ByteLock"
    WriteRegStr HKCU "Software\Classes\Directory\shell\ByteLock" "Icon" "$INSTDIR\ByteLock.exe,0"
    WriteRegStr HKCU "Software\Classes\Directory\shell\ByteLock\command" "" '"$INSTDIR\ByteLock.exe" --lock "%1"'
SectionEnd

Section "Uninstall"
    ExecWait '"$INSTDIR\ByteLock.exe" --verify-recovery' $0
    ${If} $0 != 0
        MessageBox MB_OK|MB_ICONSTOP "Incorrect recovery key. Uninstall aborted."
        Abort
    ${EndIf}

    ExecWait '"$INSTDIR\ByteLock.exe" --unlock-all'

    DeleteRegKey HKCU "Software\Classes\.blocked"
    DeleteRegKey HKCU "Software\Classes\ByteLock.blocked"
    DeleteRegKey HKCU "Software\Classes\Directory\shell\ByteLock"
    DeleteRegKey HKCU "Software\ByteLock"

    RMDir /r "$SMPROGRAMS\ByteLock"
    RMDir /r "$INSTDIR"
SectionEnd
