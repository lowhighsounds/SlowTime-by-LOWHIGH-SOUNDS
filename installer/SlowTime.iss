; SlowTime — Windows installer (Inno Setup)
; Builds SlowTime-Installer-Windows.exe. First page lets the user pick:
;   Automatic (recommended) -> installs straight to the standard plugin folders
;   Custom                  -> shows a folder page to choose the location
; A components page lets them pick VST3 and/or CLAP.
;
; Prerequisites (local build step / CI):
;   1. Release builds at ..\build\SlowTime_artefacts\Release\{VST3,CLAP}\
;   2. Branding assets in installer\assets\ (committed; regen via make-assets.ps1)
; Compile with:  ISCC.exe installer\SlowTime.iss

#define MyAppName "SlowTime"
#define MyAppVersion "0.1.2"
#define MyPublisher "LowHigh Sounds"
#define MyVst3Source "..\build\SlowTime_artefacts\Release\VST3\SlowTime.vst3"
#define MyClapSource "..\build\SlowTime_artefacts\Release\CLAP\SlowTime.clap"

[Setup]
AppId={{8F3D2A17-9C4E-4B6A-B1E2-7A5C9D0E4F13}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyPublisher}
AppPublisherURL=https://github.com/lowhighsounds/SlowTime-by-LOWHIGH-SOUNDS
; Default location used only in Custom mode (the folder page). Automatic mode
; ignores {app} and installs to the standard shared folders (see [Code]).
DefaultDirName={commoncf64}
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
SetupIconFile=assets\slowtime.ico
WizardImageFile=assets\wizard-large.bmp
WizardSmallImageFile=assets\wizard-small.bmp
UninstallDisplayIcon={code:GetVst3Bundle}
UninstallDisplayName={#MyAppName}
OutputDir=output
OutputBaseFilename=SlowTime-Installer-Windows
Compression=lzma2
SolidCompression=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Messages]
SelectDirDesc=Where should SlowTime be installed?
SelectDirLabel3=SlowTime will install the selected formats under the folder below (in VST3\ and CLAP\ subfolders). Add this folder to your DAW's plugin search paths so it's found.

[Types]
Name: "full";   Description: "All formats (recommended)"
Name: "custom"; Description: "Choose formats"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3  —  FL Studio, Ableton, Cubase, Studio One, Reaper, Bitwig, ..."; Types: full custom
Name: "clap"; Description: "CLAP  —  Bitwig, Reaper, FL Studio 2024+, Studio One 6.5+, Cubase 14+";  Types: full custom

[Files]
; VST3 is a bundle (folder); CLAP is a single .clap file. Destinations come from
; [Code] so Automatic uses the standard folders and Custom uses the chosen one.
Source: "{#MyVst3Source}\*"; DestDir: "{code:GetVst3Bundle}"; \
    Components: vst3; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "{#MyClapSource}"; DestDir: "{code:GetClapDir}"; \
    Components: clap; Flags: ignoreversion

[Code]
var
  ModePage: TInputOptionWizardPage;

procedure InitializeWizard;
begin
  ModePage := CreateInputOptionPage(wpWelcome,
    'Installation type', 'How should SlowTime be installed?',
    'Choose how the plugin location is decided:',
    True, False);
  ModePage.Add('Automatic (recommended)  —  install to the standard folders your DAW already scans');
  ModePage.Add('Custom  —  let me choose the install folder');
  ModePage.SelectedValueIndex := 0;
end;

function IsAutomatic: Boolean;
begin
  Result := ModePage.SelectedValueIndex = 0;
end;

{ Skip the folder page unless the user picked Custom. }
function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := (PageID = wpSelectDir) and IsAutomatic;
end;

{ VST3 bundle destination. }
function GetVst3Bundle(Param: String): String;
begin
  if IsAutomatic then
    Result := ExpandConstant('{commoncf64}\VST3\SlowTime.vst3')
  else
    Result := ExpandConstant('{app}\VST3\SlowTime.vst3');
end;

{ CLAP destination folder. }
function GetClapDir(Param: String): String;
begin
  if IsAutomatic then
    Result := ExpandConstant('{commoncf64}\CLAP')
  else
    Result := ExpandConstant('{app}\CLAP');
end;
