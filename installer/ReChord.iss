; Windows installer for ReChord (Inno Setup 6). Built by GitHub Actions: iscc installer\ReChord.iss
; A new version replaces the installed one.
#define AppName "ReChord"
#define AppVersion "1.1.0"
#define Publisher "Pouria Motabean"

[Setup]
AppId={{8C2E41B7-5D93-4F0A-B6E1-7A4D2C9F3E58}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#Publisher}
DefaultDirName={commoncf64}\VST3\{#AppName}.vst3
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputDir=..\setup
OutputBaseFilename=ReChord-Setup-{#AppVersion}
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
UninstallDisplayName={#AppName} {#AppVersion} (VST3)
UninstallDisplayIcon={sys}\shell32.dll,168
WizardStyle=modern

; a host (Cubase) that has the plugin loaded keeps the .vst3 locked: setup offers to close it first
CloseApplications=yes
CloseApplicationsFilter=*.exe,*.dll,*.vst3
RestartApplications=no

[Files]
Source: "..\dist\ReChord.vst3\*"; DestDir: "{commoncf64}\VST3\ReChord.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion

[InstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\ReChord.vst3"

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\ReChord.vst3"

[Messages]
FinishedLabel=ReChord is installed in C:\Program Files\Common Files\VST3.%n%nIn Cubase: Studio > VST Plug-in Manager > Rescan, then add ReChord on an Instrument track. Rendered loops are saved in Documents\ReChord\Renders.
