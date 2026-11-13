; Katsodi desktop installer (Inno Setup 6).
;
;   powershell -File native-desktop/tools/native.ps1 installer windows-release
;
; Packs the staged release (native.ps1 install) into one per-user setup:
; no admin rights, installs to %LOCALAPPDATA%\Programs\Katsodi, adds a
; Start menu entry and an uninstaller. Unsigned: signing needs a code-signing
; certificate, which is the owner's decision (Windows SmartScreen will warn).
; Local data (settings in HKCU, library.sqlite, the Credential Manager entry)
; is kept on uninstall unless the user removes it in Settings.

#ifndef StageDir
  #error StageDir must point at the staged release folder
#endif
#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#ifndef OutputDir
  #define OutputDir "."
#endif

[Setup]
AppId={{5E0C9A4B-3B1E-4C8E-9A57-6B2D41C7F0A1}
AppName=Katsodi
AppVersion={#AppVersion}
AppVerName=Katsodi {#AppVersion}
AppPublisher=Katsodi
AppPublisherURL=https://katsodi.watch
AppSupportURL=https://katsodi.watch
DefaultDirName={autopf}\Katsodi
DefaultGroupName=Katsodi
DisableProgramGroupPage=yes
DisableDirPage=auto
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
OutputDir={#OutputDir}
OutputBaseFilename=Katsodi-Setup-{#AppVersion}
SetupIconFile=..\resources\icons\katsodi.ico
UninstallDisplayIcon={app}\bin\Katsodi.exe
UninstallDisplayName=Katsodi
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
; Close a running Katsodi (and its player helper) before replacing files.
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "ru"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
ru.DesktopIcon=Создать значок на рабочем столе
en.DesktopIcon=Create a desktop shortcut
ru.Launch=Запустить Katsodi
en.Launch=Launch Katsodi
ru.NoWebView2=Не найден компонент Microsoft Edge WebView2. Katsodi будет работать, но видео и музыка в комнатах откроются в браузере. WebView2 входит в Windows 11 и устанавливается с сайта Microsoft.
en.NoWebView2=Microsoft Edge WebView2 was not found. Katsodi works, but room videos and music will open in the browser. WebView2 is part of Windows 11 and can be installed from Microsoft's website.

[Tasks]
Name: "desktopicon"; Description: "{cm:DesktopIcon}"; Flags: unchecked

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\Katsodi"; Filename: "{app}\bin\Katsodi.exe"; WorkingDir: "{app}\bin"
Name: "{autodesktop}\Katsodi"; Filename: "{app}\bin\Katsodi.exe"; WorkingDir: "{app}\bin"; Tasks: desktopicon

[Run]
Filename: "{app}\bin\Katsodi.exe"; Description: "{cm:Launch}"; Flags: nowait postinstall skipifsilent

[Code]
// The provider player needs the Evergreen WebView2 runtime; without it the
// app still works and opens room sources in the browser, so only inform.
function WebView2Installed(): Boolean;
var
  Version: String;
begin
  Result :=
    (RegQueryStringValue(HKLM, 'SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}', 'pv', Version) and (Version <> '') and (Version <> '0.0.0.0')) or
    (RegQueryStringValue(HKCU, 'Software\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}', 'pv', Version) and (Version <> '') and (Version <> '0.0.0.0'));
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep = ssPostInstall) and not WebView2Installed() and not WizardSilent() then
    MsgBox(CustomMessage('NoWebView2'), mbInformation, MB_OK);
end;
