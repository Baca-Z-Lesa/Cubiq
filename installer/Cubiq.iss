; Installer pre Cubiq (Inno Setup 6). Zostavenie: installer\build-installer.ps1

#define AppName "Cubiq"
#define AppVersion "0.11.0"
#define BuildDir "..\build\Release"

[Setup]
AppId={{7C3E2B1A-5D4F-4E8B-9A61-C0B1C2D3E4F5}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=Cubiq
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
; Bez administrátora sa inštaluje len pre aktuálneho používateľa (dá sa zmeniť v dialógu).
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\dist
OutputBaseFilename=Cubiq-{#AppVersion}-setup
SetupIconFile=Cubiq.ico
UninstallDisplayIcon={app}\Cubiq.exe
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern

[Languages]
Name: "slovak"; MessagesFile: "compiler:Languages\Slovak.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#BuildDir}\Cubiq.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\assets\*"; DestDir: "{app}\assets"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion isreadme

[Icons]
; Pracovný priečinok = {app}, aby hra našla assets\shaders.
Name: "{group}\{#AppName}"; Filename: "{app}\Cubiq.exe"; WorkingDir: "{app}"
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\Cubiq.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\Cubiq.exe"; WorkingDir: "{app}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent

; Uložené svety v %APPDATA%\Cubiq ostávajú aj po odinštalovaní.
