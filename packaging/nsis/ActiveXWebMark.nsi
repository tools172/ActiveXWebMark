; ActiveXWebMark — NSIS installer script.
;
; Build from anywhere:
;   makensis /DVERSION=0.2.0 packaging/nsis/ActiveXWebMark.nsi
; Output: dist\ActiveXWebMark-0.2.0-setup.exe (at the repository root)
;
; NSIS resolves relative paths against this script's directory (packaging/nsis),
; so "..\.." reaches the repository root. CI overrides EXE/OUTDIR with absolute
; paths via /DEXE and /DOUTDIR.

!ifndef VERSION
  !define VERSION "0.0.0"
!endif

!ifndef EXE
  !define EXE "..\..\build\vs2022\src\app\Release\ActiveXWebMark.exe"
!endif

!ifndef OUTDIR
  !define OUTDIR "..\..\dist"
!endif

!define PRODUCT_NAME "ActiveXWebMark"
!define EXE_NAME "ActiveXWebMark.exe"
!define PUBLISHER "ActiveXWebMark"

Unicode true

!include "MUI2.nsh"

Name "${PRODUCT_NAME}"
OutFile "${OUTDIR}\${PRODUCT_NAME}-${VERSION}-setup.exe"
InstallDir "$PROGRAMFILES64\${PRODUCT_NAME}"
InstallDirRegKey HKLM "Software\${PRODUCT_NAME}" "InstallDir"
RequestExecutionLevel admin

; --- Pages ---------------------------------------------------------------
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "SimpChinese"
!insertmacro MUI_LANGUAGE "English"

; --- Install -------------------------------------------------------------
Section "Install" SEC01
  SetOutPath "$INSTDIR"
  File "${EXE}"
  WriteUninstaller "$INSTDIR\uninstall.exe"

  CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}"
  CreateShortcut "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME}.lnk" "$INSTDIR\${EXE_NAME}"
  CreateShortcut "$DESKTOP\${PRODUCT_NAME}.lnk" "$INSTDIR\${EXE_NAME}"

  WriteRegStr HKLM "Software\${PRODUCT_NAME}" "InstallDir" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "DisplayName" "${PRODUCT_NAME}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "Publisher" "${PUBLISHER}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "UninstallString" '"$INSTDIR\uninstall.exe"'

  ; Non-fatal warning if the VC++ runtime is missing.
  IfFileExists "$WINDIR\SysNative\vcruntime140.dll" have_crt no_crt
have_crt:
  Goto done_crt
no_crt:
  MessageBox MB_ICONEXCLAMATION|MB_OK "需要 Microsoft Visual C++ 2015-2022 运行库（vcruntime140.dll）。若程序无法启动，请先安装 x64 运行库。"
done_crt:
SectionEnd

; --- Uninstall -----------------------------------------------------------
Section "Uninstall"
  Delete "$INSTDIR\${EXE_NAME}"
  Delete "$INSTDIR\uninstall.exe"
  RMDir "$INSTDIR"

  Delete "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME}.lnk"
  RMDir "$SMPROGRAMS\${PRODUCT_NAME}"
  Delete "$DESKTOP\${PRODUCT_NAME}.lnk"

  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
  DeleteRegKey HKLM "Software\${PRODUCT_NAME}"
SectionEnd
