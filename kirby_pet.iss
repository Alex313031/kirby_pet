; Kirby Pet Inno Setup Script File
; Only for Inno Setup 5.x to support Windows 2000/XP
; Tested with ISS 5.6.1

#define AppVer "0.0.5"
#define AppName "Kirby Pet"
#define ExeName "kirby_pet"
#define ScrName "kirby_scr"
#define Developer "Alex313031"
#define CopyRightYear "© 2026"
#define GitURL "https://github.com/Alex313031/kirby_pet"

[Setup]
MinVersion=5.0
AppName={#AppName}
AppVersion={#AppVer}
AppVerName={#AppName} v{#AppVer}
OutputBaseFilename={#ExeName}_{#AppVer}_setup
UninstallDisplayName={#AppName} {#AppVer}
DefaultDirName={pf}\{#AppName}
DefaultGroupName={#AppName}
UninstallDisplayIcon={app}\{#ExeName}.exe
DisableWelcomePage=no
AlwaysShowDirOnReadyPage=yes
Compression=lzma
SolidCompression=yes
VersionInfoVersion={#AppVer}
AppPublisher={#Developer}
AppPublisherURL={#GitURL}
AppSupportURL={#GitURL}/#readme
AppUpdatesURL={#GitURL}/releases
VersionInfoCompany={#Developer}
AppCopyright={#CopyRightYear}
VersionInfoCopyright={#CopyRightYear} {#Developer}
VersionInfoProductName={#AppName}
SetupIconFile="src\app\res\main.ico"
WizardImageFile="assets\installer_background.bmp"
;WizardStyle=modern
UseSetupLdr=yes

[Files]
Source: "Release\{#ExeName}.exe"; DestDir: "{app}"
; The screensaver and its INF sit next to the app so the right-click -> Install
; route (and a manual copy elsewhere) keep working after setup...
Source: "Release\{#ScrName}.scr"; DestDir: "{app}"
Source: "Release\{#ScrName}.inf"; DestDir: "{app}"
; ...and the .scr also goes into the system directory, the only place Display
; Properties enumerates screensavers from (SysWOW64 on 64-bit Windows, where a
; 32-bit saver is still listed). ignoreversion so a same-version reinstall
; still overwrites; uninsrestartdelete in case it is the running saver at
; uninstall time.
Source: "Release\{#ScrName}.scr"; DestDir: "{sys}"; Flags: ignoreversion uninsrestartdelete

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#ExeName}.exe"

[Run]
Filename: "{app}\{#ExeName}.exe"; Description: "Launch app when finished"; Flags: postinstall nowait skipifsilent
