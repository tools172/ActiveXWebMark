; ActiveXWebMark — NSIS installer script.
;
; Build from anywhere (VERSION comes from <build-dir>\version.txt, generated
; by CMake from the top-level project VERSION; see README):
;   makensis "/DVERSION=<major>.<minor>.<patch>" packaging/nsis/ActiveXWebMark.nsi
; Output: dist\ActiveXWebMark-<major>.<minor>.<patch>-setup.exe (at the repository root)
;
; NSIS resolves relative paths against this script's directory (packaging/nsis),
; so "..\.." reaches the repository root. CI overrides EXE/OUTDIR with absolute
; paths via /DEXE and /DOUTDIR.

!ifndef VERSION
  !error "VERSION is not defined; pass /DVERSION=<major>.<minor>.<patch>"
!endif

!ifndef EXE
  !define EXE "..\..\build\vs2022\src\app\Release\ActiveXWebMark.exe"
!endif

!ifndef OUTDIR
  !define OUTDIR "..\..\dist"
!endif

; Vendored Mermaid sidecar (Node script + official mermaid bundle).
!ifndef MERMAID_DIR
  !define MERMAID_DIR "..\..\third_party\mermaid"
!endif

; PlantUML headless native runtime, staged by tools\plantuml-native\fetch.ps1.
!ifndef PLANTUML_DIR
  !define PLANTUML_DIR "..\..\build\plantuml-native\staging"
!endif
!if ! /FileExists "${PLANTUML_DIR}\plantuml-headless.exe"
  !error "PlantUML native package missing. Run tools\plantuml-native\fetch.ps1 before packaging."
!endif

!define PRODUCT_NAME "ActiveXWebMark"
!define EXE_NAME "ActiveXWebMark.exe"
!define PUBLISHER "ActiveXWebMark"
!define PROGID "ActiveXWebMark.Markdown"
!define APP_KEY "Software\${PRODUCT_NAME}"

Unicode true

!include "MUI2.nsh"

; Installer and uninstaller use the application icon.
!define MUI_ICON "..\..\src\app\resources\app.ico"
!define MUI_UNICON "..\..\src\app\resources\app.ico"

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
  ; 64-bit application: write the 64-bit registry view. A 32-bit installer
  ; would otherwise put the association keys under Wow6432Node, where the
  ; 64-bit Explorer never looks.
  SetRegView 64

  SetOutPath "$INSTDIR"
  File "${EXE}"

  ; Mermaid renderer: the app launches Node with this script when a diagram
  ; is present. Missing Node/Edge simply leaves the fenced block as text.
  SetOutPath "$INSTDIR\mermaid"
  File "${MERMAID_DIR}\sidecar.cjs"
  File "${MERMAID_DIR}\mermaid.min.js"
  SetOutPath "$INSTDIR"

  ; PlantUML headless native runtime (no user Java installation needed).
  SetOutPath "$INSTDIR\plantuml"
  File /r "${PLANTUML_DIR}\*.*"
  SetOutPath "$INSTDIR"

  WriteUninstaller "$INSTDIR\uninstall.exe"

  CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}"
  CreateShortcut "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME}.lnk" "$INSTDIR\${EXE_NAME}" "" "$INSTDIR\${EXE_NAME}" 0
  CreateShortcut "$DESKTOP\${PRODUCT_NAME}.lnk" "$INSTDIR\${EXE_NAME}" "" "$INSTDIR\${EXE_NAME}" 0

  WriteRegStr HKLM "Software\${PRODUCT_NAME}" "InstallDir" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "DisplayName" "${PRODUCT_NAME}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "DisplayIcon" "$INSTDIR\${EXE_NAME},0"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "Publisher" "${PUBLISHER}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "UninstallString" '"$INSTDIR\uninstall.exe"'

  ; Markdown file association. Registers the application as a candidate
  ; handler; Windows 10 does not let an installer force the default, the user
  ; confirms it in Settings > Default apps.
  WriteRegStr HKLM "Software\Classes\${PROGID}" "" "Markdown 文档"
  WriteRegStr HKLM "Software\Classes\${PROGID}" "FriendlyTypeName" "Markdown 文档"
  WriteRegStr HKLM "Software\Classes\${PROGID}\DefaultIcon" "" "$INSTDIR\${EXE_NAME},0"
  WriteRegStr HKLM "Software\Classes\${PROGID}\shell\open\command" "" '"$INSTDIR\${EXE_NAME}" "%1"'
  WriteRegStr HKLM "Software\Classes\${PROGID}\shell\open" "MultiSelectModel" "Player"

  WriteRegStr HKLM "Software\Classes\Applications\${EXE_NAME}" "FriendlyAppName" "${PRODUCT_NAME}"
  WriteRegStr HKLM "Software\Classes\Applications\${EXE_NAME}" "ApplicationCompany" "${PUBLISHER}"
  WriteRegStr HKLM "Software\Classes\Applications\${EXE_NAME}\DefaultIcon" "" "$INSTDIR\${EXE_NAME},0"
  WriteRegStr HKLM "Software\Classes\Applications\${EXE_NAME}\shell\open" "MultiSelectModel" "Player"
  WriteRegStr HKLM "Software\Classes\Applications\${EXE_NAME}\shell\open\command" "" '"$INSTDIR\${EXE_NAME}" "%1"'
  WriteRegStr HKLM "Software\Classes\Applications\${EXE_NAME}\SupportedTypes" ".md" ""
  WriteRegStr HKLM "Software\Classes\Applications\${EXE_NAME}\SupportedTypes" ".markdown" ""
  WriteRegStr HKLM "Software\Classes\Applications\${EXE_NAME}\SupportedTypes" ".mdown" ""

  WriteRegStr HKLM "${APP_KEY}\Capabilities" "ApplicationName" "${PRODUCT_NAME}"
  WriteRegStr HKLM "${APP_KEY}\Capabilities" "ApplicationDescription" "Markdown 阅读与编辑"
  WriteRegStr HKLM "${APP_KEY}\Capabilities\FileAssociations" ".md" "${PROGID}"
  WriteRegStr HKLM "${APP_KEY}\Capabilities\FileAssociations" ".markdown" "${PROGID}"
  WriteRegStr HKLM "${APP_KEY}\Capabilities\FileAssociations" ".mdown" "${PROGID}"
  WriteRegStr HKLM "Software\RegisteredApplications" "${PRODUCT_NAME}" "${APP_KEY}\Capabilities"

  WriteRegStr HKLM "Software\Classes\.md\OpenWithProgids" "${PROGID}" ""
  WriteRegStr HKLM "Software\Classes\.markdown\OpenWithProgids" "${PROGID}" ""
  WriteRegStr HKLM "Software\Classes\.mdown\OpenWithProgids" "${PROGID}" ""

  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, i 0, i 0)'

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
  SetRegView 64

  Delete "$INSTDIR\${EXE_NAME}"
  Delete "$INSTDIR\uninstall.exe"
  Delete "$INSTDIR\mermaid\sidecar.cjs"
  Delete "$INSTDIR\mermaid\mermaid.min.js"
  RMDir "$INSTDIR\mermaid"
  RMDir /r "$INSTDIR\plantuml"
  RMDir "$INSTDIR"

  Delete "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME}.lnk"
  RMDir "$SMPROGRAMS\${PRODUCT_NAME}"
  Delete "$DESKTOP\${PRODUCT_NAME}.lnk"

  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
  DeleteRegKey HKLM "Software\${PRODUCT_NAME}"

  ; Remove only our own association entries; other applications may share
  ; OpenWithProgids, and .md itself belongs to the system.
  DeleteRegKey HKLM "Software\Classes\${PROGID}"
  DeleteRegKey HKLM "Software\Classes\Applications\${EXE_NAME}"
  DeleteRegValue HKLM "Software\RegisteredApplications" "${PRODUCT_NAME}"
  DeleteRegValue HKLM "Software\Classes\.md\OpenWithProgids" "${PROGID}"
  DeleteRegValue HKLM "Software\Classes\.markdown\OpenWithProgids" "${PROGID}"
  DeleteRegValue HKLM "Software\Classes\.mdown\OpenWithProgids" "${PROGID}"

  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, i 0, i 0)'
SectionEnd
