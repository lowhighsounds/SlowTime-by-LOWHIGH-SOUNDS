; SlowTime — Windows installer (Inno Setup)
; Builds SlowTime-Installer-Windows.exe. Lets the user choose formats (VST3/CLAP)
; and the install location (default = the standard shared plugin folder).
;
; Prerequisites (done by the local build step / CI):
;   1. Release builds at ..\build\SlowTime_artefacts\Release\{VST3,CLAP}\
;   2. Branding assets in installer\assets\ (committed; regen via make-assets.ps1)
; Compile with:  ISCC.exe installer\SlowTime.iss

#define MyAppName "SlowTime"
#define MyAppVersion "0.1.1"
#define MyPublisher "LowHigh Sounds"
#define MyVst3Source "..\build\SlowTime_artefacts\Release\VST3\SlowTime.vst3"
#define MyClapSource "..\build\SlowTime_artefacts\Release\CLAP\SlowTime.clap"

[Setup]
AppId={{8F3D2A17-9C4E-4B6A-B1E2-7A5C9D0E4F13}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyPublisher}
AppPublisherURL=https://github.com/lowhighsounds/SlowTime-by-LOWHIGH-SOUNDS
; Default to the standard shared plugin root (C:\Program Files\Common Files);
; the VST3/CLAP subfolders below resolve to the exact paths DAWs scan. The user
; can change this on the directory page (see the explanatory message).
DefaultDirName={commoncf64}
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
SetupIconFile=assets\slowtime.ico
WizardImageFile=assets\wizard-large.bmp
WizardSmallImageFile=assets\wizard-small.bmp
UninstallDisplayIcon={app}\VST3\SlowTime.vst3,0
UninstallDisplayName={#MyAppName}
OutputDir=output
OutputBaseFilename=SlowTime-Installer-Windows
Compression=lzma2
SolidCompression=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Messages]
SelectDirDesc=Where should SlowTime be installed?
SelectDirLabel3=SlowTime will install the selected plugin formats under the folder below (in VST3\ and CLAP\ subfolders). Keep the default so your DAW finds them automatically. If you choose another folder, add it to your DAW's plugin search paths.

[Types]
Name: "full";   Description: "All formats (recommended)"
Name: "custom"; Description: "Choose formats"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3  —  FL Studio, Ableton, Cubase, Studio One, Reaper, Bitwig, ..."; Types: full custom
Name: "clap"; Description: "CLAP  —  Bitwig, Reaper, FL Studio 2024+, Studio One 6.5+, Cubase 14+";  Types: full custom

[Files]
; VST3 is a bundle (folder) -> install recursively into <dir>\VST3\SlowTime.vst3
Source: "{#MyVst3Source}\*"; DestDir: "{app}\VST3\SlowTime.vst3"; \
    Components: vst3; Flags: recursesubdirs createallsubdirs ignoreversion
; CLAP on Windows is a single .clap file -> <dir>\CLAP\SlowTime.clap
Source: "{#MyClapSource}"; DestDir: "{app}\CLAP"; \
    Components: clap; Flags: ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{app}\VST3\SlowTime.vst3"
Type: files;          Name: "{app}\CLAP\SlowTime.clap"
