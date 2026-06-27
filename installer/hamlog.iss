[Setup]
AppName=HamLog
AppVersion=1.0.0
AppPublisher=IW8XOU
AppPublisherURL=https://github.com/IW8XOU/hamlog
DefaultDirName={autopf}\HamLog
DefaultGroupName=HamLog
OutputDir=C:\hamlog-cpp\installer\output
OutputBaseFilename=HamLog-1.0.0-Setup
Compression=lzma2/ultra
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
UninstallDisplayIcon={app}\HamLog.exe
SetupIconFile=
WizardStyle=modern
LicenseFile=
MinVersion=10.0

[Languages]
Name: "italian"; MessagesFile: "compiler:Languages\Italian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "C:\hamlog-cpp\dist\HamLog\HamLog.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\hamlog-cpp\dist\HamLog\*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\hamlog-cpp\dist\HamLog\generic\*"; DestDir: "{app}\generic"; Flags: ignoreversion recursesubdirs
Source: "C:\hamlog-cpp\dist\HamLog\iconengines\*"; DestDir: "{app}\iconengines"; Flags: ignoreversion recursesubdirs
Source: "C:\hamlog-cpp\dist\HamLog\imageformats\*"; DestDir: "{app}\imageformats"; Flags: ignoreversion recursesubdirs
Source: "C:\hamlog-cpp\dist\HamLog\networkinformation\*"; DestDir: "{app}\networkinformation"; Flags: ignoreversion recursesubdirs
Source: "C:\hamlog-cpp\dist\HamLog\platforms\*"; DestDir: "{app}\platforms"; Flags: ignoreversion recursesubdirs
Source: "C:\hamlog-cpp\dist\HamLog\sqldrivers\*"; DestDir: "{app}\sqldrivers"; Flags: ignoreversion recursesubdirs
Source: "C:\hamlog-cpp\dist\HamLog\styles\*"; DestDir: "{app}\styles"; Flags: ignoreversion recursesubdirs
Source: "C:\hamlog-cpp\dist\HamLog\tls\*"; DestDir: "{app}\tls"; Flags: ignoreversion recursesubdirs

[Icons]
Name: "{group}\HamLog"; Filename: "{app}\HamLog.exe"
Name: "{group}\{cm:UninstallProgram,HamLog}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\HamLog"; Filename: "{app}\HamLog.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\HamLog.exe"; Description: "{cm:LaunchProgram,HamLog}"; Flags: nowait postinstall skipifsilent
