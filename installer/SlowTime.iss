; SlowTime — Windows installer (Inno Setup). Installs the VST3 plugin.
; First page lets the user pick:
;   Automatic (recommended) -> installs straight to the standard VST3 folder
;   Custom                  -> shows a folder page to choose the location
;
; Prerequisites (local build step / CI):
;   1. Release build at ..\build\SlowTime_artefacts\Release\VST3\SlowTime.vst3
;   2. Branding assets in installer\assets\ (committed; regen via make-assets.ps1)
; Compile with:  ISCC.exe installer\SlowTime.iss

#define MyAppName "SlowTime"
#define MyAppVersion "0.1.5"
#define MyPublisher "LowHigh Sounds"
#define MyVst3Source "..\build\SlowTime_artefacts\Release\VST3\SlowTime.vst3"

[Setup]
AppId={{8F3D2A17-9C4E-4B6A-B1E2-7A5C9D0E4F13}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyPublisher}
AppPublisherURL=https://github.com/lowhighsounds/SlowTime-by-LOWHIGH-SOUNDS
; Default location used only in Custom mode. Automatic mode ignores {app} and
; installs to the standard VST3 folder (see [Code]).
DefaultDirName={commoncf64}\VST3
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
SelectDirLabel3=SlowTime (VST3) will be installed in the folder below. Keep the default so your DAW finds it automatically. If you choose another folder, add it to your DAW's VST3 search paths.

; Remove the old CLAP build left by v0.1.2 (SlowTime is now VST3-only), so DAWs
; don't keep loading the stale CLAP. No-op on fresh installs.
[InstallDelete]
Type: files; Name: "{commoncf64}\CLAP\SlowTime.clap"

[Files]
Source: "{#MyVst3Source}\*"; DestDir: "{code:GetVst3Bundle}"; \
    Flags: recursesubdirs createallsubdirs ignoreversion

[Code]
var
  ModePage: TInputOptionWizardPage;

procedure InitializeWizard;
begin
  ModePage := CreateInputOptionPage(wpWelcome,
    'Installation type', 'How should SlowTime be installed?',
    'Choose how the plugin location is decided:',
    True, False);
  ModePage.Add('Automatic (recommended)  —  install to the standard VST3 folder your DAW already scans');
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

{ VST3 bundle destination: standard folder (Automatic) or the chosen one (Custom). }
function GetVst3Bundle(Param: String): String;
begin
  if IsAutomatic then
    Result := ExpandConstant('{commoncf64}\VST3\SlowTime.vst3')
  else
    Result := ExpandConstant('{app}\VST3\SlowTime.vst3');
end;
