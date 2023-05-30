﻿//
// Copyright 2020 - 2022 Andon "Kaldaien" Coleman
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//

#include "../resource.h"
#include "../version.h"

#include <strsafe.h>
#include <wtypes.h>
#include <dxgi1_5.h>

#include <gsl/gsl_util>

#include <sk_utility/command.h>

#include <SKIF.h>
#include <SKIF_utility.h>
#include <SKIF_imgui.h>

#include <stores/Steam/steam_library.h>
#include <injection.h>

//#include <font_awesome.h>
#include <fonts/fa_621.h>
#include <fonts/fa_621b.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_win32.h>
#include <imgui/imgui_internal.h>
#include <dxgi1_6.h>
#include <xinput.h>

#include <fsutil.h>
#include <psapi.h>

#include <codecvt>
#include <fstream>
#include <random>

#include <filesystem>
#include <concurrent_queue.h>

#include "imgui/d3d11/imgui_impl_dx11.h"
#include <d3d11.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#include <unordered_set>

#include <fonts/fa_regular_400.ttf.h>
#include <fonts/fa_brands_400.ttf.h>
#include <fonts/fa_solid_900.ttf.h>

#include <stores/Steam/app_record.h>

#include <wtypes.h>
#include <WinInet.h>

#include <gsl/gsl>
#include <comdef.h>

#include <sstream>
#include <cwctype>

#include <unordered_set>
#include <nlohmann/json.hpp>

#include <dwmapi.h>
#include <ui_tabs/about.h>
#include <ui_tabs/settings.h>

#include "TextFlow.hpp"

#include <registry.h>
#include <updater.h>

#include "drvreset.h"

#pragma comment (lib, "wininet.lib")

const int SKIF_STEAM_APPID = 1157970;
bool RecreateSwapChains    = false;
bool RepositionSKIF        = false;
bool tinyDPIFonts          = false;
bool invalidateFonts       = false;
bool failedLoadFonts       = false;
bool failedLoadFontsPrompt = false;
DWORD invalidatedFonts     = 0;
DWORD invalidatedDevice    = 0;
bool startedMinimized      = false;
bool msgDontRedraw         = false;
bool coverFadeActive       = false;
bool SKIF_Shutdown         = false;
int  startupFadeIn         = 0;
int addAdditionalFrames    = 0;

// Custom Global Key States used for moving SKIF around using WinKey + Arrows
bool KeyWinKey = false;
int  SnapKeys  = 0;     // 2 = Left, 4 = Up, 8 = Right, 16 = Down

// This is used in conjunction with _registry.bMinimizeOnGameLaunch to suppress the "Please start game" notification
bool SKIF_bSuppressServiceNotification = false;

// Graphics options set during runtime
bool SKIF_bCanFlip                 = false, // Flip Sequential               Windows 7 (2013 Platform Update), or Windows 8+
     SKIF_bCanWaitSwapchain        = false, // Waitable Swapchain            Windows 8.1+
     SKIF_bCanFlipDiscard          = false, // Flip Discard                  Windows 10+
     SKIF_bCanAllowTearing         = false, // DWM Tearing                   Windows 10+
     SKIF_bCanHDR                  = false, // High Dynamic Range            Windows 10 1709+ (Build 16299)
     SKIF_bHDREnabled              = false; // HDR Enabled

// Holds swapchain wait handles
std::vector<HANDLE> vSwapchainWaitHandles;

// GOG Galaxy stuff
std::wstring GOGGalaxy_Path        = L"";
std::wstring GOGGalaxy_Folder      = L"";
std::wstring GOGGalaxy_UserID      = L"";
bool GOGGalaxy_Installed           = false;

DWORD    RepopulateGamesWasSet     = 0;
bool     RepopulateGames           = false,
         RefreshSettingsTab        = false;
uint32_t SelectNewSKIFGame         = 0;

bool  HoverTipActive               = false;
DWORD HoverTipDuration             = 0;

// Notification icon stuff
#define SKIF_NOTIFY_ICON                    0x1330 // 4912
#define SKIF_NOTIFY_EXIT                    0x1331 // 4913
#define SKIF_NOTIFY_START                   0x1332 // 4914
#define SKIF_NOTIFY_STOP                    0x1333 // 4915
#define SKIF_NOTIFY_STARTWITHSTOP           0x1334 // 4916
#define WM_SKIF_NOTIFY_ICON      (WM_USER + 0x150) // 1360
bool SKIF_isTrayed = false;
NOTIFYICONDATA niData;
HMENU hMenu;

// Cmd line argument stuff
SKIF_Signals _Signal;

PopupState UpdatePromptPopup = PopupState_Closed;
PopupState HistoryPopup      = PopupState_Closed;
UITab SKIF_Tab_Selected      = UITab_Library,
      SKIF_Tab_ChangeTo      = UITab_None;

HMODULE hModSKIF     = nullptr;
HMODULE hModSpecialK = nullptr;

// Texture related locks to prevent driver crashes
concurrency::concurrent_queue <CComPtr <IUnknown>> SKIF_ResourcesToFree;

#define GCL_HICON           (-14)

float fAspect     = 16.0f / 9.0f;
float fBottomDist = 0.0f;

ID3D11Device*           g_pd3dDevice           = nullptr;
ID3D11DeviceContext*    g_pd3dDeviceContext    = nullptr;
ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

// Forward declarations

bool                CreateDeviceD3D                           (HWND hWnd);
void                CleanupDeviceD3D                          (void);
LRESULT WINAPI      SKIF_WndProc                              (HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI      SKIF_Notify_WndProc                       (HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void                SKIF_putStopOnInjection                   (bool in);
void                SKIF_Initialize                           (void);
void                SKIF_UI_DrawComponentVersion              (void);
void                SKIF_UI_DrawPlatformStatus                (void);
void                SKIF_UI_DrawShellyTheGhost                (void);

bool    bExitOnInjection = false; // Used to exit SKIF on a successful injection if it's used merely as a launcher
CHandle hInjectAck (0);           // Signalled when a game finishes injection

// Holds current global DPI scaling, 1.0f == 100%, 1.5f == 150%.
// Can go below 1.0f if SKIF is shown on a smaller screen with less than 1000px in height.
float SKIF_ImGui_GlobalDPIScale      = 1.0f;
// Holds last frame's DPI scaling
float SKIF_ImGui_GlobalDPIScale_Last = 1.0f;

std::string SKIF_StatusBarText = "";
std::string SKIF_StatusBarHelp = "";
HWND        SKIF_hWnd          =  0;
HWND        SKIF_ImGui_hWnd    =  0;
HWND        SKIF_Notify_hWnd   =  0;

CONDITION_VARIABLE SKIF_IsFocused    = { };

ImGuiStyle SKIF_ImGui_DefaultStyle;

HWND hWndOrigForeground;

void
SKIF_Startup_ProcessCmdLineArgs (LPWSTR lpCmdLine)
{
  _Signal.Start =
    StrStrIW (lpCmdLine, L"Start")    != NULL;

  _Signal.Temporary =
    StrStrIW (lpCmdLine, L"Temp")     != NULL;

  _Signal.Stop =
    StrStrIW (lpCmdLine, L"Stop")     != NULL;

  _Signal.Quit =
    StrStrIW (lpCmdLine, L"Quit")     != NULL;

  _Signal.Minimize =
    StrStrIW (lpCmdLine, L"Minimize") != NULL;

  _Signal.AddSKIFGame =
    StrStrIW (lpCmdLine, L"AddGame=") != NULL;

  // Both AddSKIFGame and Launcher can include .exe in the argument
  //   so only set Launcher if AddSKIFGame is false.
  if (! _Signal.AddSKIFGame)
        _Signal.Launcher =
      StrStrIW (lpCmdLine, L".exe")     != NULL;

  _Signal._RunningInstance =
    FindWindowExW (0, 0, SKIF_WindowClass, nullptr);
}

void
SKIF_Startup_AddGame (LPWSTR lpCmdLine)
{
  if (! _Signal.AddSKIFGame)
    return;

  PLOG_INFO << "Adding custom game to SKIF...";

  // O:\WindowsApps\DevolverDigital.MyFriendPedroWin10_1.0.6.0_x64__6kzv4j18v0c96\MyFriendPedro.exe

  std::wstring cmdLine        = std::wstring(lpCmdLine);
  std::wstring cmdLineArgs    = cmdLine;

  // Transform to lowercase
  std::wstring cmdLineLower   = SKIF_Util_TowLower (cmdLine);

  std::wstring splitPos1Lower = L"addgame="; // Start split
  std::wstring splitEXELower  = L".exe";     // Stop split (exe)
  std::wstring splitLNKLower  = L".lnk";     // Stop split (lnk)

  // Exclude anything before "addgame=", if any
  cmdLine = cmdLine.substr(cmdLineLower.find(splitPos1Lower) + splitPos1Lower.length());

  // First position is a space -- skip that one
  if (cmdLine.find(L" ") == 0)
    cmdLine = cmdLine.substr(1);

  // First position is a quotation mark -- we need to strip those
  if (cmdLine.find(L"\"") == 0)
    cmdLine = cmdLine.substr(1, cmdLine.find(L"\"", 1) - 1) + cmdLine.substr(cmdLine.find(L"\"", 1) + 1, std::wstring::npos);

  // Update lowercase
  cmdLineLower   = SKIF_Util_TowLower (cmdLine);

  // If .exe is part of the string
  if (cmdLineLower.find(splitEXELower) != std::wstring::npos)
  {
    // Extract proxied arguments, if any
    cmdLineArgs = cmdLine.substr(cmdLineLower.find(splitEXELower) + splitEXELower.length());

    // Exclude anything past ".exe"
    cmdLine = cmdLine.substr(0, cmdLineLower.find(splitEXELower) + splitEXELower.length());
  }

  // If .lnk is part of the string
  else if (cmdLineLower.find(splitLNKLower) != std::wstring::npos)
  {
    // Exclude anything past ".lnk" since we're reading the arguments from the shortcut itself
    cmdLine = cmdLine.substr(0, cmdLineLower.find(splitLNKLower) + splitLNKLower.length());
      
    WCHAR wszTarget   [MAX_PATH];
    WCHAR wszArguments[MAX_PATH];

    SKIF_Util_ResolveShortcut (SKIF_hWnd, cmdLine.c_str(), wszTarget, wszArguments, MAX_PATH);

    cmdLine     = std::wstring(wszTarget);
    cmdLineArgs = std::wstring(wszArguments);
  }

  // Clear var if no valid path was found
  else {
    cmdLine.clear();
  }

  // Only proceed if we have an actual valid path
  if (cmdLine.length() > 0)
  {
    // First position of the arguments is a space -- skip that one
    if (cmdLineArgs.find(L" ") == 0)
      cmdLineArgs = cmdLineArgs.substr(1);

    extern std::wstring SKIF_GetProductName    (const wchar_t* wszName);
    extern int          SKIF_AddCustomAppID    (std::vector<std::pair<std::string, app_record_s>>* apps,
                                                std::wstring name, std::wstring path, std::wstring args);
    extern
      std::vector <
        std::pair < std::string, app_record_s >
                  > apps;

    if (PathFileExists (cmdLine.c_str()))
    {
      std::wstring productName = SKIF_GetProductName (cmdLine.c_str());

      if (productName == L"")
        productName = std::filesystem::path (cmdLine).replace_extension().filename().wstring();

      SelectNewSKIFGame = (uint32_t)SKIF_AddCustomAppID (&apps, productName, cmdLine, cmdLineArgs);
    
      // If a running instance of SKIF already exists, terminate this one as it has served its purpose
      if (SelectNewSKIFGame > 0 && _Signal._RunningInstance != 0)
      {
        SendMessage (_Signal._RunningInstance, WM_SKIF_REFRESHGAMES, SelectNewSKIFGame, 0x0);
        PLOG_INFO << "Terminating due to one of these contions were found to be true:";
        PLOG_INFO << "SelectNewSKIFGame > 0: "  << (SelectNewSKIFGame  > 0);
        PLOG_INFO << "hwndAlreadyExists != 0: " << (_Signal._RunningInstance != 0);
        ExitProcess (0x0);
      }
    }

    else {
      PLOG_ERROR << "Non-valid path detected: " << cmdLine;
    }
  }

  else {
    PLOG_ERROR << "Non-valid string detected: " << lpCmdLine;
  }
}

void
SKIF_Startup_LaunchGamePreparation (LPWSTR lpCmdLine)
{
  if (! _Signal.Launcher)
    return;

  PLOG_INFO << "Preparing game path, launch options, and working directory...";
  
  static SKIF_CommonPathsCache& _path_cache = SKIF_CommonPathsCache::GetInstance ( );
  static SKIF_InjectionContext& _inject     = SKIF_InjectionContext::GetInstance ( );

  std::wstring cmdLine        = std::wstring(lpCmdLine);
  std::wstring delimiter      = L".exe"; // split lpCmdLine at the .exe

  // First position is a quotation mark -- we need to strip those
  if (cmdLine.find(L"\"") == 0)
    cmdLine = cmdLine.substr(1, cmdLine.find(L"\"", 1) - 1) + cmdLine.substr(cmdLine.find(L"\"", 1) + 1, std::wstring::npos);

  // Transform to lowercase
  std::wstring cmdLineLower = SKIF_Util_TowLower (cmdLine);

  // Extract the target path and any proxied command line arguments
  std::wstring path           = cmdLine.substr(0, cmdLineLower.find(delimiter) + delimiter.length());                        // path
  std::wstring proxiedCmdLine = cmdLine.substr(   cmdLineLower.find(delimiter) + delimiter.length(), cmdLineLower.length()); // proxied command line

  // Path does not seem to be absolute -- add the current working directory in front of the path
  if (path.find(L"\\") == std::wstring::npos)
    path = SK_FormatStringW (LR"(%ws\%ws)", _path_cache.skif_workdir_org, path.c_str()); //orgWorkingDirectory.wstring() + L"\\" + path;

  std::wstring workingDirectory = std::filesystem::path(path).parent_path().wstring();               // path to the parent folder                              

  PLOG_VERBOSE << "Executable:        " << path;
  PLOG_VERBOSE << "Command Line Args: " << proxiedCmdLine;
  PLOG_VERBOSE << "Working Directory: " << workingDirectory;

  bool isLocalBlacklisted  = false,
       isGlobalBlacklisted = false;

  if (PathFileExists (path.c_str()))
  {
    std::wstring blacklistFile = SK_FormatStringW (L"%s\\SpecialK.deny.%ws",
                                                    std::filesystem::path(path).parent_path().wstring().c_str(),                 // full path to parent folder
                                                    std::filesystem::path(path).filename().replace_extension().wstring().c_str() // filename without extension
    );

    // Check if the executable is blacklisted
    isLocalBlacklisted  = PathFileExistsW (blacklistFile.c_str());
    isGlobalBlacklisted = _inject._TestUserList (SK_WideCharToUTF8(path).c_str(), false);

    if (! isLocalBlacklisted &&
        ! isGlobalBlacklisted)
    {
      // Whitelist the path if it haven't been already
      _inject._WhitelistBasedOnPath (SK_WideCharToUTF8(path));

      _Signal._GamePath    = path;
      _Signal._GameArgs    = proxiedCmdLine;
      _Signal._GameWorkDir = workingDirectory;
    }
  }

  else {
    PLOG_ERROR << "Non-valid path detected: " << path;

    ExitProcess (0x0);
  }
}

void
SKIF_Startup_LaunchGameService (void)
{
  if (_Signal._GamePath.empty())
    return;

  PLOG_INFO << "Starting injection service...";

  static SKIF_InjectionContext& _inject     = SKIF_InjectionContext::GetInstance ( );

  if (_Signal._RunningInstance)
    SendMessage (_Signal._RunningInstance, WM_SKIF_LAUNCHER, 0x0, 0x0);

  else if (! _inject.bCurrentState)
  {
    bExitOnInjection = true;
    _inject._StartStopInject (false, true);
  }
}

void
SKIF_Startup_LaunchGame (void)
{
  if (_Signal._GamePath.empty())
    return;
      
  PLOG_INFO << "Launching game executable: " << _Signal._GamePath;

  SHELLEXECUTEINFOW
    sexi              = { };
    sexi.cbSize       = sizeof (SHELLEXECUTEINFOW);
    sexi.lpVerb       = L"OPEN";
    sexi.lpFile       = _Signal._GamePath.c_str();
    sexi.lpParameters = _Signal._GameArgs.c_str();
    sexi.lpDirectory  = _Signal._GameWorkDir.c_str();
    sexi.nShow        = SW_SHOW;
    sexi.fMask        = SEE_MASK_FLAG_NO_UI |
                        SEE_MASK_NOASYNC    | SEE_MASK_NOZONECHECKS;

  // Launch executable
  ShellExecuteExW (&sexi);

  // If a running instance of SKIF already exists, or the game was blacklisted, terminate this one as it has served its purpose
  if (_Signal._RunningInstance)
  {
    PLOG_INFO << "Terminating as this instance has fulfilled its purpose.";

    ExitProcess (0x0);
  }
}

void
SKIF_Startup_ProxyCommandLineArguments (void)
{
  if (! _Signal._RunningInstance)
    return;

  if (! _Signal.Start            && 
      ! _Signal.Stop             &&
      ! _Signal.Minimize         &&
      ! _Signal.Quit)
    return;

  PLOG_INFO << "Proxying command line arguments...";

  if (_Signal.Start)
  {
    // This means we proxied this cmd to ourselves, in which
    // case we want to set up bExitOnInjection as well
    //if (_Signal._RunningInstance == SKIF_hWnd)
    //  PostMessage (_Signal._RunningInstance, (_Signal.Temporary) ? WM_SKIF_TEMPSTARTEXIT : WM_SKIF_START, 0x0, 0x0);
    //else
      PostMessage (_Signal._RunningInstance, (_Signal.Temporary) ? WM_SKIF_TEMPSTART     : WM_SKIF_START, 0x0, 0x0);
  }

  if (_Signal.Stop)
    PostMessage (_Signal._RunningInstance, WM_SKIF_STOP, 0x0, 0x0);
    
  if (_Signal.Minimize)
  {
    //PostMessage (_Signal._RunningInstance, WM_SKIF_MINIMIZE, 0x0, 0x0);

    // Send WM_SKIF_MINIMIZE to all running instances (including ourselves)
    EnumWindows ( []( HWND   hWnd,
                      LPARAM lParam ) -> BOOL
    {
      wchar_t                         wszRealWindowClass [64] = { };
      if (RealGetWindowClassW (hWnd,  wszRealWindowClass, 64))
        if (StrCmpIW ((LPWSTR)lParam, wszRealWindowClass) == 0)
          PostMessage (hWnd, WM_SKIF_MINIMIZE, 0x0, 0x0);
      return TRUE;
    }, (LPARAM)SKIF_WindowClass);
  }

  if (_Signal.Quit)
  {
    // PostMessage (_Signal._RunningInstance, WM_CLOSE, 0x0, 0x0);

    // Send WM_CLOSE to all running instances (including ourselves)
    EnumWindows ( []( HWND   hWnd,
                      LPARAM lParam ) -> BOOL
    {
      wchar_t                         wszRealWindowClass [64] = { };
      if (RealGetWindowClassW (hWnd,  wszRealWindowClass, 64))
        if (StrCmpIW ((LPWSTR)lParam, wszRealWindowClass) == 0)
          PostMessage (hWnd, WM_CLOSE, 0x0, 0x0);
      return TRUE;
    }, (LPARAM)SKIF_WindowClass);
  }

  // Restore the foreground state to whatever app had it before
  if (SKIF_hWnd == 0)
  {
    if (hWndOrigForeground != 0)
    {
      if (IsIconic        (hWndOrigForeground))
        ShowWindow        (hWndOrigForeground, SW_SHOWNA);
      SetForegroundWindow (hWndOrigForeground);
    }

    PLOG_INFO << "Terminating due to this instance having done its job.";
    ExitProcess (0x0);
  }
}

void
SKIF_Startup_RaiseRunningInstance (void)
{
  if (! _Signal._RunningInstance)
    return;
  
  // We must allow the existing process to set the foreground window
  //   as this is part of the WM_SKIF_RESTORE procedure
  DWORD pidAlreadyExists = 0;
  GetWindowThreadProcessId (_Signal._RunningInstance, &pidAlreadyExists);
  if (pidAlreadyExists)
    AllowSetForegroundWindow (pidAlreadyExists);

  PLOG_INFO << "Attempting to restore the running instance: " << pidAlreadyExists;
  SendMessage (_Signal._RunningInstance, WM_SKIF_RESTORE, 0x0, 0x0);
  
  PLOG_INFO << "Terminating due to this instance having done its job.";
  ExitProcess (0x0);
}


void SKIF_putStopOnInjection (bool in)
{
  static SKIF_RegistrySettings& _registry = SKIF_RegistrySettings::GetInstance ( );
  static SKIF_InjectionContext& _inject   = SKIF_InjectionContext::GetInstance ( );

  _registry.regKVDisableStopOnInjection.putData(!in);

  // If we're disabling the feature, also disable the legacy key
  if (in)
    _registry.regKVLegacyDisableStopOnInjection.putData(!in);

  if (_inject.bCurrentState)
    _inject._ToggleInjectAck (in);
}

void SKIF_CreateUpdateNotifyMenu (void)
{
  if (hMenu != NULL)
    DestroyMenu (hMenu);

  static SKIF_InjectionContext& _inject   = SKIF_InjectionContext::GetInstance ( );

  bool svcRunning         = false,
       svcRunningAutoStop = false,
       svcStopped         = false;

  if (_inject.bCurrentState      && hInjectAck.m_h <= 0)
    svcRunning         = true;
  else if (_inject.bCurrentState && hInjectAck.m_h != 0)
    svcRunningAutoStop = true;
  else
    svcStopped         = true;

  hMenu = CreatePopupMenu ( );
  AppendMenu (hMenu, MF_STRING | ((svcRunningAutoStop) ? MF_CHECKED | MF_GRAYED : (svcRunning)         ? MF_GRAYED : 0x0), SKIF_NOTIFY_STARTWITHSTOP, L"Start Service");
  AppendMenu (hMenu, MF_STRING | ((svcRunning)         ? MF_CHECKED | MF_GRAYED : (svcRunningAutoStop) ? MF_GRAYED : 0x0), SKIF_NOTIFY_START,         L"Start Service (manual stop)");
  AppendMenu (hMenu, MF_STRING | ((svcStopped)         ? MF_CHECKED | MF_GRAYED :                                    0x0), SKIF_NOTIFY_STOP,          L"Stop Service");

//AppendMenu (hMenu, MF_STRING | ((  _inject.bCurrentState) ? MF_CHECKED | MF_GRAYED : 0x0), SKIF_NOTIFY_START, L"Start Injection");
//AppendMenu (hMenu, MF_STRING | ((! _inject.bCurrentState) ? MF_CHECKED | MF_GRAYED : 0x0), SKIF_NOTIFY_STOP,  L"Stop Injection");
  AppendMenu (hMenu, MF_SEPARATOR, 0, NULL);
  AppendMenu (hMenu, MF_STRING, SKIF_NOTIFY_EXIT,          L"Exit");
}

void SKIF_CreateNotifyIcon (void)
{
  ZeroMemory (&niData,  sizeof (NOTIFYICONDATA));
  niData.cbSize       = sizeof (NOTIFYICONDATA); // 6.0.6 or higher (Windows Vista and later)
  niData.uID          = SKIF_NOTIFY_ICON;
  niData.uFlags       = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
  niData.hIcon        = LoadIcon (hModSKIF, MAKEINTRESOURCE(IDI_SKIF));
  niData.hWnd         = SKIF_Notify_hWnd;
  niData.uVersion     = NOTIFYICON_VERSION_4;
  wcsncpy_s (niData.szTip,      128, L"Special K",   128);

  niData.uCallbackMessage = WM_SKIF_NOTIFY_ICON;

  Shell_NotifyIcon (NIM_ADD, &niData);
  //Shell_NotifyIcon (NIM_SETVERSION, &niData); // Breaks shit, lol
}

void SKIF_UpdateNotifyIcon (void)
{
  static SKIF_InjectionContext& _inject   = SKIF_InjectionContext::GetInstance ( );

  niData.uFlags        = NIF_ICON;
  if (_inject.bCurrentState)
    niData.hIcon       = LoadIcon (hModSKIF, MAKEINTRESOURCE (IDI_SKIFONNOTIFY));
  else
    niData.hIcon       = LoadIcon (hModSKIF, MAKEINTRESOURCE (IDI_SKIF));

  Shell_NotifyIcon (NIM_MODIFY, &niData);
}

void SKIF_CreateNotifyToast (std::wstring message, std::wstring title = L"")
{
  static SKIF_RegistrySettings& _registry = SKIF_RegistrySettings::GetInstance ( );

  if ( _registry.iNotifications == 1 ||                           // Always
      (_registry.iNotifications == 2 && ! SKIF_ImGui_IsFocused()) // When Unfocused
    )
  {
    niData.uFlags       = NIF_INFO;
    niData.dwInfoFlags  = NIIF_NONE | NIIF_NOSOUND | NIIF_RESPECT_QUIET_TIME;
    wcsncpy_s(niData.szInfoTitle, 64, title.c_str(), 64);
    wcsncpy_s(niData.szInfo, 256, message.c_str(), 256);

    Shell_NotifyIcon (NIM_MODIFY, &niData);
  }
}

void SKIF_UI_DrawComponentVersion (void)
{
  static SKIF_InjectionContext& _inject   = SKIF_InjectionContext::GetInstance ( );

  ImGui::BeginGroup       ( );

  ImGui::Spacing          ( );
  ImGui::SameLine         ( );
  ImGui::TextColored      (ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), (const char *)u8"\u2022 ");
  ImGui::SameLine         ( );
  ImGui::TextColored      (ImGui::GetStyleColorVec4(ImGuiCol_SKIF_TextBase),    "Special K 32-bit");

#ifdef _WIN64
  ImGui::Spacing          ( );
  ImGui::SameLine         ( );
  ImGui::TextColored      (ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), (const char *)u8"\u2022 ");
  ImGui::SameLine         ( );
  ImGui::TextColored      (ImGui::GetStyleColorVec4(ImGuiCol_SKIF_TextBase),    "Special K 64-bit");
#endif
    
  ImGui::Spacing          ( );
  ImGui::SameLine         ( );
  ImGui::TextColored      (ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), (const char *)u8"\u2022 ");
  ImGui::SameLine         ( );
  ImGui::TextColored      (ImGui::GetStyleColorVec4(ImGuiCol_SKIF_TextBase),    "Frontend (SKIF)");

  ImGui::EndGroup         ( );
  ImGui::SameLine         ( );
  ImGui::BeginGroup       ( );
    
  ImGui::TextColored      (ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled), "v");
  ImGui::SameLine         ( );
  ImGui::TextColored      (ImGui::GetStyleColorVec4(ImGuiCol_SKIF_TextBase),    _inject.SKVer32.c_str());

#ifdef _WIN64
  ImGui::TextColored      (ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled), "v");
  ImGui::SameLine         ( );
  ImGui::TextColored      (ImGui::GetStyleColorVec4(ImGuiCol_SKIF_TextBase),    _inject.SKVer64.c_str());
#endif
    
  ImGui::TextColored      (ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled), "v");
  ImGui::SameLine         ( );
  ImGui::ItemSize         (ImVec2 (0.0f, ImGui::GetTextLineHeight ()));
  ImGui::SameLine         ( );
  ImGui::TextColored      (ImGui::GetStyleColorVec4(ImGuiCol_SKIF_TextBase), SKIF_VERSION_STR_A " (" __DATE__ ")");

  ImGui::EndGroup         ( );

  ImGui::BeginGroup       ( );
  ImGui::Spacing          ( );
  ImGui::SameLine         ( );
  ImGui::TextColored      (ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), (const char *)u8"\u2022 ");
  ImGui::SameLine         ( );
  ImGui::TextColored      (ImGui::GetStyleColorVec4(ImGuiCol_SKIF_TextBase), "View release notes...");
  SKIF_ImGui_SetMouseCursorHand ( );
  if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
    HistoryPopup = PopupState_Open;
  ImGui::EndGroup         ( );
  
  static SKIF_Updater& _updater = 
         SKIF_Updater::GetInstance ( );
  
  if ((_updater.GetState ( ) & UpdateFlags_Available) == UpdateFlags_Available)
  {
    SKIF_ImGui_Spacing      ( );
    
    ImGui::ItemSize         (ImVec2 (65.0f, 0.0f));

    ImGui::SameLine         ( );

    std::string btnLabel = ICON_FA_WRENCH "  Update";

    if ((_updater.GetState() & UpdateFlags_Rollback) == UpdateFlags_Rollback)
      btnLabel = ICON_FA_WRENCH "  Rollback";

    ImGui::PushStyleColor (ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_SKIF_Warning));
    if (ImGui::Button (btnLabel.c_str(), ImVec2(150.0f * SKIF_ImGui_GlobalDPIScale,
                                                 30.0f * SKIF_ImGui_GlobalDPIScale )))
      UpdatePromptPopup = PopupState_Open;
    ImGui::PopStyleColor ( );
  }
}

void SKIF_UI_DrawPlatformStatus (void)
{
  ImGui::BeginGroup       ( );
  ImGui::Spacing          ( );
  ImGui::SameLine         ( );

  static bool isSKIFAdmin = IsUserAnAdmin();
  if (isSKIFAdmin)
  {
    ImGui::TextColored     (ImGui::GetStyleColorVec4(ImGuiCol_SKIF_Yellow), ICON_FA_TRIANGLE_EXCLAMATION " ");
    ImGui::SameLine        ( );
    ImGui::TextColored     (ImGui::GetStyleColorVec4(ImGuiCol_SKIF_Yellow), "App is running as an administrator!");
    SKIF_ImGui_SetHoverTip ( "Running elevated is not recommended as it will inject Special K into system processes.\n"
                             "Please restart this app and the global injector service as a regular user.");
  }
  else {
    ImGui::TextColored     (ImGui::GetStyleColorVec4(ImGuiCol_SKIF_Success), ICON_FA_CHECK " ");
    ImGui::SameLine        ( );
    ImGui::TextColored     (ImGui::GetStyleColorVec4(ImGuiCol_SKIF_Success), "App is running with normal privileges.");
    SKIF_ImGui_SetHoverTip ( "This is the recommended option as Special K will not be injected\n"
                             "into system processes nor games running as an administrator.");
  }

  ImGui::EndGroup         ( );

  struct Platform {
    std::string     Name;
    std::wstring    ProcessName;
    DWORD           ProcessID   = 0,
                    PreviousPID = 0;
    bool            isRunning   = false,
                    isAdmin     = false;

    Platform (std::string n, std::wstring pn)
    {
      Name        =  n;
      ProcessName = pn;
    }
  };

  static DWORD dwLastRefresh = 0;
  static Platform Platforms[] = {
    {"32-bit service",  L"SKIFsvc32.exe"},
#ifdef _WIN64
    {"64-bit service",  L"SKIFsvc64.exe"},
#endif
    {"Steam",               L"steam.exe"},
    {"Origin",              L"Origin.exe"},
    {"Galaxy",              L"GalaxyClient.exe"},
    {"EA Desktop",          L"EADesktop.exe"},
    {"Epic Games Launcher", L"EpicGamesLauncher.exe"},
    {"Ubisoft Connect",     L"upc.exe"},
    {"RTSS",                L"RTSS.exe"}
  };

  // Timer has expired, refresh
  if (dwLastRefresh < SKIF_Util_timeGetTime() && (! ImGui::IsAnyMouseDown ( ) || ! SKIF_ImGui_IsFocused ( ) ))
  {
    for (auto& p : Platforms)
    {
      p.ProcessID = 0;
      p.isRunning = false;
    }

    PROCESSENTRY32W pe32 = { };

    SK_AutoHandle hProcessSnap (
      CreateToolhelp32Snapshot (TH32CS_SNAPPROCESS, 0)
    );

    if ((intptr_t)hProcessSnap.m_h > 0)
    {
      pe32.dwSize = sizeof (PROCESSENTRY32W);

      if (Process32FirstW (hProcessSnap, &pe32))
      {
        do
        {
          for (auto& p : Platforms)
          {
            if (wcsstr (pe32.szExeFile, p.ProcessName.c_str()))
            {
              p.ProcessID = pe32.th32ProcessID;
              p.isRunning = true;

              // If it is a new process, check if it is running as an admin
              if (p.ProcessID != p.PreviousPID)
              {
                p.PreviousPID = p.ProcessID;
                p.isAdmin     = SKIF_Util_IsProcessAdmin (p.ProcessID);
              }

              // Skip checking the remaining platforms for this process
              continue;
            }
          }
        } while (Process32NextW (hProcessSnap, &pe32));
      }
    }

    dwLastRefresh = SKIF_Util_timeGetTime () + 1000; // Set timer for next refresh
  }

  for ( auto& p : Platforms )
  {
    if (p.isRunning)
    {
      ImGui::BeginGroup       ( );
      ImGui::Spacing          ( );
      ImGui::SameLine         ( );

      if (p.isAdmin)
      {
        ImGui::TextColored     (ImGui::GetStyleColorVec4(ImGuiCol_SKIF_Yellow), ICON_FA_TRIANGLE_EXCLAMATION " ");
        ImGui::SameLine        ( );
        if (p.ProcessName == L"RTSS.exe")
          ImGui::TextColored     (ImGui::GetStyleColorVec4(ImGuiCol_SKIF_Yellow), (p.Name + " is running and might conflict with Special K!").c_str() );
        else
          ImGui::TextColored     (ImGui::GetStyleColorVec4(ImGuiCol_SKIF_Yellow), (p.Name + " is running as an administrator!").c_str() );

        if (isSKIFAdmin)
          SKIF_ImGui_SetHoverTip (("It is not recommended to run either " + p.Name + " or this app as an administrator.\n"
                                   "Please restart both as a normal user.").c_str());
        else if (p.ProcessName == L"RTSS.exe")
          SKIF_ImGui_SetHoverTip ( "RivaTuner Statistics Server is known to occasionally conflict with Special K.\n"
                                   "Please stop it if Special K does not function as expected. You might have\n"
                                   "to stop MSI Afterburner as well if you use that application.");
        else if (p.ProcessName == L"SKIFsvc32.exe" || p.ProcessName == L"SKIFsvc64.exe")
          SKIF_ImGui_SetHoverTip ( "Running elevated is not recommended as it will inject Special K into system processes.\n"
                                   "Please restart the frontend and the global injector service as a regular user.");
        else
          SKIF_ImGui_SetHoverTip (("Running elevated will prevent injection into these games.\n"
                                   "Please restart " + p.Name + " as a normal user.").c_str());
      }
      else {
        ImGui::TextColored     (ImGui::GetStyleColorVec4(ImGuiCol_SKIF_Success), ICON_FA_CHECK " ");
        ImGui::SameLine        ( );
        ImGui::TextColored     (ImGui::GetStyleColorVec4(ImGuiCol_SKIF_Success), (p.Name + " is running.").c_str());
      }

      ImGui::EndGroup          ( );
    }
    else if (p.ProcessName == L"SKIFsvc32.exe" || p.ProcessName == L"SKIFsvc64.exe")
    {
      ImGui::Spacing          ( );
      ImGui::SameLine         ( );
      ImGui::ItemSize         (ImVec2 (ImGui::CalcTextSize (ICON_FA_CHECK " ") .x, ImGui::GetTextLineHeight()));
      //ImGui::TextColored      (ImColor (0.68F, 0.68F, 0.68F), " " ICON_FA_MINUS " ");
      ImGui::SameLine         ( );
      ImGui::TextColored      (ImGui::GetStyleColorVec4 (ImGuiCol_TextDisabled), (p.Name + " is stopped.").c_str());
    }

#ifdef _WIN64
    if (p.ProcessName == L"SKIFsvc64.exe")
      ImGui::NewLine           ( );
#else
    if (p.ProcessName == L"SKIFsvc32.exe")
      ImGui::NewLine();
#endif
  }
}

void SKIF_UI_DrawShellyTheGhost (void)
{
  static SKIF_RegistrySettings& _registry = SKIF_RegistrySettings::GetInstance ( );
  static SKIF_InjectionContext& _inject   = SKIF_InjectionContext::GetInstance ( );

  if (                        _registry.iGhostVisibility == 1 ||
    (_inject.bCurrentState && _registry.iGhostVisibility == 2) )
  {
    // Required for subsequent GetCursorPosX() calls to get the right pos, as otherwise it resets to 0.0f
    ImGui::SameLine ( );

    // Prepare Shelly color and Y position
    const  float fGhostTimeStep = 0.01f;
    static float fGhostTime     = 0.0f;

    float fGhostYPos = 4.0f * (std::sin(6 * (fGhostTime / 2.5f)) + 0.5f);

    ImVec4 vGhostColor = ImColor::ImColor (
        0.5f * (std::sin(6 * (fGhostTime / 2.5f)) + 1),
        0.5f * (std::sin(6 * (fGhostTime / 2.5f + 1.0f / 3.0f)) + 1),
        0.5f * (std::sin(6 * (fGhostTime / 2.5f + 2.0f / 3.0f)) + 1)
      );

    if (_registry.iStyle == 2)
      vGhostColor = vGhostColor * ImVec4(0.8f, 0.8f, 0.8f, 1.0f);

    // Non-static as it needs to be updated constantly due to mixed-DPI monitor configs
    float fMaxPos = ImGui::GetContentRegionMax ( ).x - ImGui::GetCursorPosX ( ) - 115.0f * SKIF_ImGui_GlobalDPIScale;

    static float direction = -0.33f; // Each frame takes a 0.33% step in either direction
    static float fMinPos   =  0.0f;
    static float fRelPos   = 50.0f;  // Percentage based (0% -> 100%)

    // Change direction if we go below 1% or above 99% of the distance
    if (fRelPos <= 1.0f || fRelPos >= 99.0f)
      direction = -direction;

    // Take a new relative step in the new direction
    fRelPos += direction;

    // Convert relative position for an actual position
    float fActPos = (fMaxPos - fMinPos) * (fRelPos / 100.0f);

    ImGui::SameLine    (0.0f, fActPos);
  
    ImGui::SetCursorPosY (
      ImGui::GetCursorPosY ( ) + fGhostYPos
                          );

    ImGui::TextColored (vGhostColor, ICON_FA_GHOST);
    
    // Increase Shelly timestep for next frame
    fGhostTime += fGhostTimeStep;
  }
}


void SKIF_SetStyle (void)
{  
  static SKIF_RegistrySettings& _registry = SKIF_RegistrySettings::GetInstance ( );

  // Setup Dear ImGui style
  switch (_registry.iStyle)
  {
  case 3:
    ImGui::StyleColorsClassic  ( );
    break;
  case 2:
    ImGui::StyleColorsLight    ( );
    break;
  case 1:
    ImGui::StyleColorsDark     ( );
    break;
  case 0:
  default:
    SKIF_ImGui_StyleColorsDark ( );
    _registry.iStyle = 0;
  }
}


void SKIF_Initialize (void)
{
  static bool isInitalized = false;

  if (! isInitalized)
  {     isInitalized = true;

    static SKIF_CommonPathsCache& _path_cache = SKIF_CommonPathsCache::GetInstance ( );
    
    // Let's change the current working directory to the folder of the executable itself.
    SetCurrentDirectory (         _path_cache.specialk_install);

    // Generate 8.3 filenames
    SK_Generate8Dot3    (_path_cache.skif_workdir_org);
    SK_Generate8Dot3    (_path_cache.specialk_install);

    bool fallback = true;

    // See if we can interact with the install folder
    // This section of the code triggers a refresh of the DLL files for other running SKIF instances
    // TODO: Find a better way to determine permissions that does not rely on creating dummy files/folders?
    std::filesystem::path testDir  (_path_cache.specialk_install);
    std::filesystem::path testFile (testDir);

    testDir  /= L"SKIFTMPDIR";
    testFile /= L"SKIFTMPFILE.tmp";

    // Try to delete any existing tmp folder or file (won't throw an exception at least)
    RemoveDirectory (testDir.c_str());
    DeleteFile      (testFile.wstring().c_str());

    std::error_code ec;
    // See if we can create a folder
    if (! std::filesystem::exists (            testDir, ec) &&
          std::filesystem::create_directories (testDir, ec))
    {
      // Delete it
      RemoveDirectory (testDir.c_str());

      // See if we can create a file
      HANDLE h = CreateFile ( testFile.wstring().c_str(),
              GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
                  NULL,
                    CREATE_NEW,
                      FILE_ATTRIBUTE_NORMAL,
                        NULL );

      // If the file was created successfully
      if (h != INVALID_HANDLE_VALUE)
      {
        // We need to close the handle as well
        CloseHandle (h);

        // Delete it
        DeleteFile (testFile.wstring().c_str());

        // Use current path as we have write permissions
        wcsncpy_s ( _path_cache.specialk_userdata, MAX_PATH,
                    _path_cache.specialk_install, _TRUNCATE );

        // No need to rely on the fallback
        fallback = false;
      }
    }

    if (fallback)
    {
      // Fall back to appdata in case of issues
      std::wstring fallbackDir =
        std::wstring (_path_cache.my_documents.path) + LR"(\My Mods\SpecialK\)";

      wcsncpy_s ( _path_cache.specialk_userdata, MAX_PATH,
                  fallbackDir.c_str(), _TRUNCATE);
        
      // Create any missing directories
      if (! std::filesystem::exists (            fallbackDir, ec))
            std::filesystem::create_directories (fallbackDir, ec);
    }

    // Now we can proceed with initializing the logging
    // If SKIF is used as a launcher, use a separate log file
    std::wstring logPath =
      SK_FormatStringW ((_Signal.Launcher) ? LR"(%ws\SKIF_launcher.log)" 
                                           : LR"(%ws\SKIF.log)",
            _path_cache.specialk_userdata
      );

    // Delete the old log file
    DeleteFile (logPath.c_str());

    // Engage logging!
    plog::init (plog::debug, logPath.c_str(), 10000000, 1);

#ifdef _WIN64
    PLOG_INFO << "Special K Injection Frontend (SKIF) 64-bit v " << SKIF_VERSION_STR_A;
#else
    PLOG_INFO << "Special K Injection Frontend (SKIF) 32-bit v " << SKIF_VERSION_STR_A;
#endif

    PLOG_INFO << "Built " __TIME__ ", " __DATE__;
    PLOG_INFO << SKIF_LOG_SEPARATOR;
    PLOG_INFO << "Working directory:  ";
    PLOG_INFO << "Old:                " << _path_cache.skif_workdir_org;
    PLOG_INFO << "New:                " << std::filesystem::current_path ();
    PLOG_INFO << "SKIF executable:    " << _path_cache.skif_executable;
    PLOG_INFO << "Special K install:  " << _path_cache.specialk_install;
    PLOG_INFO << "Special K userdata: " << _path_cache.specialk_userdata;
    PLOG_INFO << SKIF_LOG_SEPARATOR;

    if (_Signal.Launcher)
      PLOG_INFO << "SKIF is being used as a launcher.";
  }
}

bool bKeepWindowAlive  = true,
     bKeepProcessAlive = true;

// Uninstall registry keys
// Current User: HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Uninstall\{F4A43527-9457-424A-90A6-17CF02ACF677}_is1
// All Users:   HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{F4A43527-9457-424A-90A6-17CF02ACF677}_is1

// Install folders
// Legacy:                              Documents\My Mods\SpecialK
// Modern (Current User; non-elevated): %LOCALAPPDATA%\Programs\Special K
// Modern (All Users;        elevated): %PROGRAMFILES%\Special K

// Main code
int
APIENTRY
wWinMain ( _In_     HINSTANCE hInstance,
           _In_opt_ HINSTANCE hPrevInstance,
           _In_     LPWSTR    lpCmdLine,
           _In_     int       nCmdShow )
{
  UNREFERENCED_PARAMETER (hPrevInstance);
  UNREFERENCED_PARAMETER (hInstance);

  SetErrorMode (SEM_FAILCRITICALERRORS | SEM_NOALIGNMENTFAULTEXCEPT);
  
  SKIF_Util_SetThreadDescription (GetCurrentThread (), L"SKIF_MainThread");

  /*
  if (! SKIF_Util_IsWindows8Point1OrGreater ( ))
  {
    PLOG_INFO << "Unsupported version of Windows detected. Special K requires at least Windows 8.1; please update to a newer version.";
    MessageBox (NULL, L"Special K requires at least Windows 8.1\nPlease update to a newer version of Windows.", L"Unsupported Windows", MB_OK | MB_ICONERROR);
    return 0;
  }
  */

  // 2023-04-05: Shouldn't be needed as DPI-awareness is set through the embedded appmanifest file // Aemony
  //ImGui_ImplWin32_EnableDpiAwareness ();

  /* 2023-04-05: I'm pretty sure this block is unnecessary // Aemony
  GetSystemMetricsForDpi =
   (GetSystemMetricsForDpi_pfn)GetProcAddress (GetModuleHandle (L"user32.dll"),
   "GetSystemMetricsForDpi");
  */

  CoInitializeEx (nullptr, 0x0);

  if (StrStrIW (lpCmdLine, L"RestartDisplDrv") != NULL)
  {
    if (IsUserAnAdmin ( ))
    {
      if (! RestartDriver ( ))
        SKIF_Util_GetErrorAsMsgBox (L"Failed to restart display driver");
    }

    else {
      MessageBox(NULL, L"The 'RestartDisplDrv' command line argument requires"
                       L" elevated permissions to work properly.",
                       L"Admin privileges required",
               MB_ICONERROR | MB_OK);
    }

    // Don't stick around if the RestartDisplDrv command is being used.
    ExitProcess (0x0);
  }
  
  // Process cmd line arguments (1/4) -- this sets up the necessary variables
  SKIF_Startup_ProcessCmdLineArgs (lpCmdLine);

  // This constructs these singleton objects
  static SKIF_CommonPathsCache& _path_cache = SKIF_CommonPathsCache::GetInstance ( ); // Does not rely on anything
  static SKIF_RegistrySettings& _registry   = SKIF_RegistrySettings::GetInstance ( ); // Does not rely on anything

  // Initialize SKIF
  SKIF_Initialize ( ); // Relies on _path_cache and sets up logging

  plog::get()->setMaxSeverity((plog::Severity) _registry.iLogging);

  PLOG_INFO << "Max severity to log was set to " << _registry.iLogging;

  // This constructs this singleton object
  static SKIF_InjectionContext& _inject     = SKIF_InjectionContext::GetInstance ( ); // Relies on SKIF_Initialize (working dir) + _path_cache (cached paths) + logging
  
  // Process cmd line arguments (2/4)
  hWndOrigForeground = // Remember what third-party window is currently in the foreground
    GetForegroundWindow ();
  SKIF_Startup_AddGame (lpCmdLine);
  SKIF_Startup_LaunchGamePreparation (lpCmdLine);

  // If there already an instance of SKIF running we do not
  //   need to create a window to service the cmd line args
  // Process cmd line arguments (3/4)
  if (_Signal._RunningInstance)
  {
    SKIF_Startup_LaunchGameService         ( );
    SKIF_Startup_LaunchGame                ( );
    SKIF_Startup_ProxyCommandLineArguments ( );
    if (! _registry.bAllowMultipleInstances)
      SKIF_Startup_RaiseRunningInstance    ( );
  }

  // Initialize the SKIF_IsFocused variable that the gamepad thread will sleep on
  InitializeConditionVariable (&SKIF_IsFocused);

  // Load the SKIF.exe module (used to populate the icon here and there)
  hModSKIF =
    GetModuleHandleW (nullptr);

  // We don't want Steam's overlay to draw upon SKIF
  SetEnvironmentVariable (L"SteamNoOverlayUIDrawing", L"1");

  // First round
  if (_Signal.Minimize)
    nCmdShow = SW_SHOWMINNOACTIVE;

  if (nCmdShow == SW_SHOWMINNOACTIVE && _registry.bCloseToTray)
    nCmdShow = SW_HIDE;

  // Second round
  if (nCmdShow == SW_SHOWMINNOACTIVE)
    startedMinimized = true;
  else if (nCmdShow == SW_HIDE)
    startedMinimized = SKIF_isTrayed = true;

  // Check if Controlled Folder Access is enabled
  if (! _Signal.Launcher)
  {
    if (_registry.bDisableCFAWarning == false && SKIF_Util_GetControlledFolderAccess())
    {
      if (IDYES == MessageBox(NULL, L"Controlled Folder Access is enabled in Windows and may prevent Special K and even some games from working properly. "
                                    L"It is recommended to either disable the feature or add exclusions for games where Special K is used as well as SKIF (this application)."
                                    L"\n\n"
                                    L"Do you want to disable this warning for all future launches?"
                                    L"\n\n"
                                    L"Microsoft's support page with more information will open when you select any of the options below.",
                                    L"Warning about Controlled Folder Access",
                 MB_ICONWARNING | MB_YESNOCANCEL))
      {
        _registry.regKVDisableCFAWarning.putData (true);
      }

      SKIF_Util_OpenURI(L"https://support.microsoft.com/windows/allow-an-app-to-access-controlled-folders-b5b6627a-b008-2ca2-7931-7e51e912b034");
    }

    // Register SKIF in Windows to enable quick launching.
    PLOG_INFO << SKIF_LOG_SEPARATOR;
    PLOG_INFO << "Current Registry State:";
    PLOG_INFO << "Special K user data:   " << _registry.wsPath;
    PLOG_INFO << "SKIF app registration: " << _registry.wsAppRegistration;
    SKIF_Util_RegisterApp ( );
    PLOG_INFO << SKIF_LOG_SEPARATOR;
  }

  // Create application window
  WNDCLASSEX wc =
  { sizeof (WNDCLASSEX),
            CS_CLASSDC, SKIF_WndProc,
            0L,         0L,
    hModSKIF, nullptr,  nullptr,
              nullptr,  nullptr,
    SKIF_WindowClass,
              nullptr          };

  if (! ::RegisterClassEx (&wc))
  {
    return 0;
  }

  // Create invisible notify window (for the traybar icon and notification toasts)
  WNDCLASSEX wcNotify =
  { sizeof (WNDCLASSEX),
            CS_CLASSDC, SKIF_Notify_WndProc,
            0L,         0L,
        NULL, nullptr,  nullptr,
              nullptr,  nullptr,
    _T ("SK_Injection_Frontend_Notify"),
              nullptr          };

  if (! ::RegisterClassEx (&wcNotify))
  {
    return 0;
  }

  DWORD dwStyle   = ( WS_VISIBLE | WS_POPUP | WS_MINIMIZEBOX | WS_SYSMENU ),
        dwStyleEx = ( WS_EX_APPWINDOW | WS_EX_NOACTIVATE );

  if (nCmdShow != SW_SHOWMINNOACTIVE &&
      nCmdShow != SW_SHOWNOACTIVATE  &&
      nCmdShow != SW_SHOWNA          &&
      nCmdShow != SW_HIDE)
    dwStyleEx &= ~WS_EX_NOACTIVATE;

  if (SKIF_isTrayed)
    dwStyle &= ~WS_VISIBLE;

  SKIF_hWnd             =
    CreateWindowExW (                    dwStyleEx,
      wc.lpszClassName, _L("Special K"), dwStyle,
                         0, 0,
                         0, 0,
                   nullptr, nullptr,
              wc.hInstance, nullptr
    );

  SKIF_Notify_hWnd      =
    CreateWindowExW (                                 WS_EX_NOACTIVATE,
      wcNotify.lpszClassName, _T("Special K Notify"), WS_ICONIC,
                         0, 0,
                         0, 0,
                   nullptr, nullptr,
        wcNotify.hInstance, nullptr
    );

  HWND  hWnd  = SKIF_hWnd;
  HICON hIcon =
    LoadIcon (hModSKIF, MAKEINTRESOURCE (IDI_SKIF));

  SendMessage      (hWnd, WM_SETICON, ICON_BIG,        (LPARAM)hIcon);
  SendMessage      (hWnd, WM_SETICON, ICON_SMALL,      (LPARAM)hIcon);
  SendMessage      (hWnd, WM_SETICON, ICON_SMALL2,     (LPARAM)hIcon);
  SetClassLongPtrW (hWnd, GCL_HICON,         (LONG_PTR)(LPARAM)hIcon);

  // Initialize Direct3D
  if (! CreateDeviceD3D (hWnd))
  {
    CleanupDeviceD3D ();
    return 1;
  }

  SetWindowLongPtr (hWnd, GWL_EXSTYLE, dwStyleEx & ~WS_EX_NOACTIVATE);

  // The notify window has been created but not displayed.
  // Now we have a parent window to which a notification tray icon can be associated.
  SKIF_CreateNotifyIcon       ();
  SKIF_CreateUpdateNotifyMenu ();

  // Show the window
  if (! SKIF_isTrayed)
  {
    ShowWindowAsync (hWnd, nCmdShow);
    UpdateWindow    (hWnd);
  }

  // If there were not an instance of SKIF already running
  //   we need to handle any remaining tasks here.
  // Process cmd line arguments (4/4)
  if (! _Signal._RunningInstance || _registry.bAllowMultipleInstances)
  {
    if (! _Signal._GamePath.empty())
    {
      SKIF_Startup_LaunchGameService ( );
      SKIF_Startup_LaunchGame        ( );
      _registry.bSmallMode = true;
    }

    // If we are not acting as a launcher,
    //   send messages to ourselves.
    else {
      _Signal._RunningInstance = SKIF_hWnd;
      SKIF_Startup_ProxyCommandLineArguments ( );

      // If we are starting the service,
      //   let us start in small mode
      //if (_Signal.Start)
      //  _registry.bSmallMode = true;
    }
  }

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION   ();
  ImGui::CreateContext ();

  ImGuiIO& io =
    ImGui::GetIO ();

  (void)io; // WTF does this do?!

  io.IniFilename = "SKIF.ini";                                // nullptr to disable imgui.ini
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;        // Enable Gamepad Controls
//io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
  io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
//io.ConfigFlags |= ImGuiConfigFlags_NavEnableSetMousePos;
  io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleFonts;
  io.ConfigViewportsNoAutoMerge      = false;
  io.ConfigViewportsNoTaskBarIcon    =  true;
  io.ConfigViewportsNoDefaultParent  = false;
  io.ConfigDockingAlwaysTabBar       = false;
  io.ConfigDockingTransparentPayload =  true;


  if (_registry.bDisableDPIScaling)
  {
    io.ConfigFlags &= ~ImGuiConfigFlags_DpiEnableScaleFonts;     // FIXME-DPI: THIS CURRENTLY DOESN'T WORK AS EXPECTED. DON'T USE IN USER APP!
  //io.ConfigFlags |=  ImGuiConfigFlags_DpiEnableScaleViewports; // FIXME-DPI
  }

  // Setup Dear ImGui style
  SKIF_SetStyle ( );

  ImGuiStyle& style =
  ImGui::GetStyle ();

  // When viewports are enabled we tweak WindowRounding/WindowBg
  //   so platform windows can look identical to regular ones.

  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
  {
    style.WindowRounding               = 5.0F;
    style.Colors [ImGuiCol_WindowBg].w = 1.0F;
  }

  style.WindowRounding  = 4.0F;// style.ScrollbarRounding;
  style.ChildRounding   = style.WindowRounding;
  style.TabRounding     = style.WindowRounding;
  style.FrameRounding   = style.WindowRounding;
  
  if (_registry.bDisableBorders)
  {
    style.TabBorderSize   = 0.0F;
    style.FrameBorderSize = 0.0F;
  }
  else {
    style.TabBorderSize   = 1.0F * SKIF_ImGui_GlobalDPIScale;
    style.FrameBorderSize = 1.0F * SKIF_ImGui_GlobalDPIScale;
  }

  // Setup Platform/Renderer bindings
  ImGui_ImplWin32_Init (hWnd); // This ends up creating a separate window/hWnd as well
  ImGui_ImplDX11_Init  (g_pd3dDevice, g_pd3dDeviceContext);

  DWORD dwDwmPeriod = 16; // Assume 60 Hz by default
  SKIF_Util_GetMonitorHzPeriod (SKIF_hWnd, MONITOR_DEFAULTTOPRIMARY, dwDwmPeriod);
  //OutputDebugString((L"Initial refresh rate period: " + std::to_wstring (dwDwmPeriod) + L"\n").c_str());

  // Message queue/pump
  MSG msg = { };

  // Variables related to the display SKIF is visible on
  ImGuiPlatformMonitor*
          monitor          = nullptr;
  ImVec2  windowPos;
  ImRect  windowRect       = ImRect(0.0f, 0.0f, 0.0f, 0.0f);
  ImRect  monitor_extent   = ImRect(0.0f, 0.0f, 0.0f, 0.0f);
  bool    changedMode      = false;
          RepositionSKIF   = (! PathFileExistsW(L"SKIF.ini") || _registry.bOpenAtCursorPosition);

#define SKIF_FONTSIZE_DEFAULT 18.0F // 18.0F

  // Initialize ImGui fonts
  SKIF_ImGui_InitFonts (SKIF_FONTSIZE_DEFAULT, (! _Signal.Launcher) );

  // Variable related to continue/pause rendering behaviour
  bool HiddenFramesContinueRendering = true;  // We always have hidden frames that require to continue rendering on init
  bool svcTransitionFromPendingState = false; // This is used to continue rendering if we transitioned over from a pending state (which kills the refresh timer)

  // Force a one-time check before we enter the main loop
  _inject._TestServletRunlevel (true);

  // Fetch SK DLL versions
  _inject._RefreshSKDLLVersions ();

  // Initialize the updater
  static SKIF_Updater& _updater = 
         SKIF_Updater::GetInstance ( );

  // Register HDR toggle hotkey (if applicable)
  if (! _Signal.Launcher)
    SKIF_Util_RegisterHDRToggleHotKey ( );

  // Main loop
  while (! SKIF_Shutdown && IsWindow (hWnd) )
  {                         msg          = { };
    static UINT uiLastMsg = 0x0;
    coverFadeActive = false; // Assume there's no cover fade effect active

    auto _TranslateAndDispatch = [&](void) -> bool
    {
      while (! SKIF_Shutdown && PeekMessage (&msg, 0, 0U, 0U, PM_REMOVE))
      {
        if (! IsWindow (hWnd))
          return false;

        /*
        if (msg.message == WM_TIMER)
        {
          if (     msg.hwnd == NULL)
            OutputDebugString (L"Message is a thread message !\n");
          else if (msg.hwnd ==        SKIF_hWnd)
            OutputDebugString (L"Message bound for SKIF_WndProc ( ) !\n");
          else if (msg.hwnd == SKIF_Notify_hWnd)
            OutputDebugString (L"Message bound for SKIF_Notify_WndProc ( ) !\n");
          else if (msg.hwnd ==  SKIF_ImGui_hWnd)
            OutputDebugString (L"Message bound for ImGui_ImplWin32_WndProcHandler_PlatformWindow ( ) !\n");
          else {
            OutputDebugString (L"Message bound for another hWnd: ");
            OutputDebugString (std::format(L"{:x}", *reinterpret_cast<uint64_t*>(&msg.hwnd)).c_str());
            OutputDebugString (L"\n");
          }
  
          OutputDebugString((L"[_TranslateAndDispatch] Message spotted: 0x" + std::format(L"{:x}", msg.message) + L" (" + std::to_wstring(msg.message) + L")\n").c_str());
          OutputDebugString((L"[_TranslateAndDispatch]          wParam: 0x" + std::format(L"{:x}", msg.wParam)  + L" (" + std::to_wstring(msg.wParam)  + L")\n").c_str());
          OutputDebugString((L"[_TranslateAndDispatch]          lParam: 0x" + std::format(L"{:x}", msg.lParam)  + L" (" + std::to_wstring(msg.lParam)  + L")\n").c_str());
        }
        */

        // There are three different window procedures that a message can be dispatched to based on the HWND of the message
        //                                  SKIF_WndProc ( )  <=         SKIF_hWnd                         :: Handles messages meant for the "main" (aka hidden) SKIF 0x0 window that resides in the top left corner of the display
        //                           SKIF_Notify_WndProc ( )  <=  SKIF_Notify_hWnd                         :: Handles messages meant for the notification icon.
        // ImGui_ImplWin32_WndProcHandler_PlatformWindow ( )  <=   SKIF_ImGui_hWnd, Other HWNDs            :: Handles messages meant for the overarching ImGui Platform window of SKIF, as well as any
        //                                                                                                      additional swapchain windows (menus/tooltips that stretches beyond SKIF_ImGui_hWnd).
        // ImGui_ImplWin32_WndProcHandler                ( )  <=  SKIF_hWnd, SKIF_ImGui_hWnd, Other HWNDs  :: Gets called by the two main window procedures:
        //                                                                                                      - SKIF_WndProc ( )
        //                                                                                                      - ImGui_ImplWin32_WndProcHandler_PlatformWindow ( ).
        // 
        TranslateMessage (&msg);
        DispatchMessage  (&msg);

        if (msg.hwnd == 0) // Don't redraw on thread messages
          msgDontRedraw = true;

        if (msg.message == WM_MOUSEMOVE)
        {
          static LPARAM lParamPrev;
          static WPARAM wParamPrev;

          // Workaround for a bug in System Informer where it sends a fake WM_MOUSEMOVE message to the window the cursor is over
          // Ignore the message if WM_MOUSEMOVE has identical data as the previous msg
          if (msg.lParam == lParamPrev &&
              msg.wParam == wParamPrev)
            msgDontRedraw = true;
          else {
            lParamPrev = msg.lParam;
            wParamPrev = msg.wParam;
          }
        }

        uiLastMsg = msg.message;
      }

      return ! SKIF_Shutdown; // return false on exit or system shutdown
    };

    // Injection acknowledgment; shutdown injection
    //
    //  * This is backed by a periodic WM_TIMER message if injection
    //      was programmatically started and ACK has not signaled
    //
    if (                     hInjectAck.m_h != 0 &&
        WaitForSingleObject (hInjectAck.m_h,   0) == WAIT_OBJECT_0)
    {
      PLOG_INFO << "Injection was acknowledged!";
      hInjectAck.Close ();
      _inject.bAckInjSignaled = true;
      _inject._StartStopInject (true);
    }

    // If SKIF is acting as a temporary launcher, exit when the running service has been stopped
    if (bExitOnInjection && _inject.runState == SKIF_InjectionContext::RunningState::Stopped)
    {
      static DWORD dwExitDelay = SKIF_Util_timeGetTime();
      static int iDuration = -1;

      PLOG_INFO << "Application is set up to exit on Injection, beginning procedure...";

      if (iDuration == -1)
      {
        HKEY    hKey;
        DWORD32 dwData  = 0;
        DWORD   dwSize  = sizeof (DWORD32);

        if (RegOpenKeyW (HKEY_CURRENT_USER, LR"(Control Panel\Accessibility\)", &hKey) == ERROR_SUCCESS)
        {
          iDuration = (RegGetValueW(hKey, NULL, L"MessageDuration", RRF_RT_REG_DWORD, NULL, &dwData, &dwSize) == ERROR_SUCCESS) ? dwData : 5;
          RegCloseKey(hKey);
        }
        else {
          iDuration = 5;
        }
      }
      // MessageDuration * 2 seconds delay to allow Windows to send both notifications properly
      // If notifications are disabled, exit immediately
      if (dwExitDelay + iDuration * 2 * 1000 < SKIF_Util_timeGetTime() || _registry.iNotifications == 0)
      {
        bExitOnInjection = false;
        PostMessage (hWnd, WM_QUIT, 0, 0);
      }
    }

    // Set DPI related variables
    SKIF_ImGui_GlobalDPIScale_Last = SKIF_ImGui_GlobalDPIScale;
    float fontScale = 18.0F * SKIF_ImGui_GlobalDPIScale;
    if (fontScale < 15.0F)
      fontScale += 1.0F;

    // Handling sub-1000px resolutions by rebuilding the font at 11px
    if (SKIF_ImGui_GlobalDPIScale < 1.0f && (! tinyDPIFonts))
    {
      tinyDPIFonts = true;

      PLOG_VERBOSE << "DPI scale detected as being below 100%; using font scale " << fontScale << "F";
      SKIF_ImGui_InitFonts (fontScale); // 11.0F
      ImGui::GetIO ().Fonts->Build ();
      ImGui_ImplDX11_InvalidateDeviceObjects ();

      invalidatedFonts = SKIF_Util_timeGetTime();
    }

    else if (SKIF_ImGui_GlobalDPIScale >= 1.0f && tinyDPIFonts)
    {
      tinyDPIFonts = false;

      PLOG_VERBOSE << "DPI scale detected as being at or above 100%; using font scale 18.0F";

      SKIF_ImGui_InitFonts (SKIF_FONTSIZE_DEFAULT);
      ImGui::GetIO ().Fonts->Build ();
      ImGui_ImplDX11_InvalidateDeviceObjects ();

      invalidatedFonts = SKIF_Util_timeGetTime();
    }

    else if (invalidateFonts)
    {
      PLOG_VERBOSE_IF(tinyDPIFonts) << "DPI scale detected as being below 100%; using font scale " << fontScale << "F";
      SKIF_ImGui_InitFonts ((tinyDPIFonts) ? fontScale : SKIF_FONTSIZE_DEFAULT);
      ImGui::GetIO ().Fonts->Build ();
      ImGui_ImplDX11_InvalidateDeviceObjects ();

      invalidateFonts = false;
      invalidatedFonts = SKIF_Util_timeGetTime();
    }
    
    // This occurs on the next frame, as failedLoadFonts gets evaluated and set as part of ImGui_ImplDX11_NewFrame
    else if (failedLoadFonts)
    {
      SKIF_bFontChineseSimplified = false;
      SKIF_bFontChineseAll        = false;
      SKIF_bFontCyrillic          = false;
      SKIF_bFontJapanese          = false;
      SKIF_bFontKorean            = false;
      SKIF_bFontThai              = false;
      SKIF_bFontVietnamese        = false;

      PLOG_VERBOSE_IF(tinyDPIFonts) << "DPI scale detected as being below 100%; using font scale " << fontScale << "F";
      SKIF_ImGui_InitFonts ((tinyDPIFonts) ? fontScale : SKIF_FONTSIZE_DEFAULT);
      ImGui::GetIO ().Fonts->Build ();
      ImGui_ImplDX11_InvalidateDeviceObjects ();

      failedLoadFonts = false;
      failedLoadFontsPrompt = true;
    }

#pragma region New UI Frame

    if (RecreateSwapChains)
    {
      // If the device have been removed/reset/hung, we need to invalidate all resources
      if (S_OK != g_pd3dDevice->GetDeviceRemovedReason ( ))
      {
        // Invalidate resources
        ImGui_ImplDX11_InvalidateDeviceObjects ( );
        ImGui_ImplDX11_InvalidateDevice        ( );
        CleanupDeviceD3D                       ( );

        // Signal to ImGui_ImplDX11_NewFrame() that the swapchains needs recreating
        RecreateSwapChains = true;

        // Recreate
        CreateDeviceD3D                        (SKIF_hWnd);
        ImGui_ImplDX11_Init                    (g_pd3dDevice, g_pd3dDeviceContext);

        // This is used to flag that rendering should not occur until
        // any loaded textures and such also have been unloaded
        invalidatedDevice = 1;
      }
    }

    // Start the Dear ImGui frame
    ImGui_ImplDX11_NewFrame  (); // (Re)create individual swapchain windows
    ImGui_ImplWin32_NewFrame (); // Handle input
    ImGui::NewFrame          ();
    {
      // Fixes the wobble that occurs when switching between tabs,
      //  as the width/height of the window isn't dynamically calculated.
#define SKIF_wLargeMode 1038
#define SKIF_hLargeMode  944 // Does not include the status bar
#define SKIF_wSmallMode  415
#define SKIF_hSmallMode  305

      static ImVec2 SKIF_vecSmallMode,
                    SKIF_vecLargeMode,
                    SKIF_vecCurrentMode;
      ImRect rectCursorMonitor;

      // RepositionSKIF -- Step 1: Retrieve monitor of cursor, and set global DPI scale
      if (RepositionSKIF)
      {
        ImRect t;
        for (int monitor_n = 0; monitor_n < ImGui::GetPlatformIO().Monitors.Size; monitor_n++)
        {
          const ImGuiPlatformMonitor& tmpMonitor = ImGui::GetPlatformIO().Monitors[monitor_n];
          t = ImRect(tmpMonitor.MainPos, (tmpMonitor.MainPos + tmpMonitor.MainSize));

          POINT               mouse_screen_pos = { };
          if (::GetCursorPos(&mouse_screen_pos))
          {
            ImVec2 os_pos = ImVec2( (float)mouse_screen_pos.x,
                                    (float)mouse_screen_pos.y );
            if (t.Contains(os_pos))
            {
              SKIF_ImGui_GlobalDPIScale = tmpMonitor.DpiScale;

              rectCursorMonitor = t;
            }
          }
        }
      }

      SKIF_vecSmallMode   = ImVec2 ( SKIF_wSmallMode * SKIF_ImGui_GlobalDPIScale,
                                     SKIF_hSmallMode * SKIF_ImGui_GlobalDPIScale );
      SKIF_vecLargeMode   = ImVec2 ( SKIF_wLargeMode * SKIF_ImGui_GlobalDPIScale,
                                     SKIF_hLargeMode * SKIF_ImGui_GlobalDPIScale );

      // Add the status bar if it is not disabled
      if ( ! _registry.bDisableStatusBar )
      {
        SKIF_vecLargeMode.y += 31.0f * SKIF_ImGui_GlobalDPIScale;
        SKIF_vecLargeMode.y += (_registry.bDisableTooltips) ? 18.0f * SKIF_ImGui_GlobalDPIScale : 0.0f;
      }

      SKIF_vecCurrentMode  =
                    (_registry.bSmallMode) ? SKIF_vecSmallMode
                                           : SKIF_vecLargeMode;

      if (ImGui::GetFrameCount() > 2)
        ImGui::SetNextWindowSize (SKIF_vecCurrentMode);

      // RepositionSKIF -- Step 2: Repositon the window
      if (RepositionSKIF)
      {
        // Repositions the window in the center of the monitor the cursor is currently on
        ImGui::SetNextWindowPos (ImVec2(rectCursorMonitor.GetCenter().x - (SKIF_vecCurrentMode.x / 2.0f), rectCursorMonitor.GetCenter().y - (SKIF_vecCurrentMode.y / 2.0f)));
      }

      // Calculate new window boundaries and changes to fit within the workspace if it doesn't fit
      //   Delay running the code to on the third frame to allow other required parts to have already executed...
      //     Otherwise window gets positioned wrong on smaller monitors !
      if (changedMode && ImGui::GetFrameCount() > 2)
      {
        changedMode = false;

        ImVec2 topLeft      = windowPos,
               bottomRight  = windowPos + SKIF_vecCurrentMode,
               newWindowPos = windowPos;

        if (      topLeft.x < monitor_extent.Min.x )
             newWindowPos.x = monitor_extent.Min.x;
        if (      topLeft.y < monitor_extent.Min.y )
             newWindowPos.y = monitor_extent.Min.y;

        if (  bottomRight.x > monitor_extent.Max.x )
             newWindowPos.x = monitor_extent.Max.x - SKIF_vecCurrentMode.x;
        if (  bottomRight.y > monitor_extent.Max.y )
             newWindowPos.y = monitor_extent.Max.y - SKIF_vecCurrentMode.y;

        if ( newWindowPos.x != windowPos.x ||
             newWindowPos.y != windowPos.y )
          ImGui::SetNextWindowPos(newWindowPos);
      }

#pragma region Move SKIF using Windows Key + Arrow Keys

      if ((io.KeysDown[VK_LWIN] && io.KeysDownDuration[VK_LWIN] == 0.0f) ||
          (io.KeysDown[VK_RWIN] && io.KeysDownDuration[VK_RWIN] == 0.0f))
        KeyWinKey = true;
      else if (! io.KeysDown[VK_LWIN] && ! io.KeysDown[VK_RWIN])
        KeyWinKey = false;

      if (KeyWinKey && SnapKeys)
      {
        if (monitor != nullptr)
        {
          HMONITOR currentMonitor = MonitorFromWindow (SKIF_hWnd, MONITOR_DEFAULTTONULL);

          if (currentMonitor != NULL)
          {
            MONITORINFO currentMonitorInfo{}, nearestMonitorInfo{};
            currentMonitorInfo.cbSize = sizeof(currentMonitorInfo);
            nearestMonitorInfo.cbSize = sizeof(nearestMonitorInfo);

            if (GetMonitorInfo(currentMonitor, (LPMONITORINFO)&currentMonitorInfo))
            {
              // Some simple math to basically create a dummy RECT that
              //  lies alongside one of the sides of the current monitor
              RECT dummy(currentMonitorInfo.rcMonitor);
              if      (SnapKeys &  2) // Left
              {
                dummy.right  = dummy.left;
                dummy.left   = dummy.left   - static_cast<long>(SKIF_vecCurrentMode.x);
              }
              else if (SnapKeys &  4) // Up
              {
                dummy.bottom = dummy.top;
                dummy.top    = dummy.top    - static_cast<long>(SKIF_vecCurrentMode.y);
              }
              else if (SnapKeys &  8) // Right
              {
                dummy.left   = dummy.right;
                dummy.right  = dummy.right  + static_cast<long>(SKIF_vecCurrentMode.x);
              }
              else if (SnapKeys & 16) // Down
              {
                dummy.top = dummy.bottom;
                dummy.bottom = dummy.bottom + static_cast<long>(SKIF_vecCurrentMode.y);
              }

              // Let us retrieve the closest monitor based on our dummy rect
              HMONITOR nearestMonitor = MonitorFromRect(&dummy, MONITOR_DEFAULTTONEAREST);
              if (GetMonitorInfo(nearestMonitor, (LPMONITORINFO)&nearestMonitorInfo))
              {
                // Don't bother if the nearest monitor is also the current monitor
                if (nearestMonitorInfo.rcMonitor != currentMonitorInfo.rcMonitor)
                {
                  // Loop through all platform monitors
                  for (int monitor_n = 0; monitor_n < ImGui::GetPlatformIO().Monitors.Size; monitor_n++)
                  {
                    const ImGuiPlatformMonitor& tmpMonitor = ImGui::GetPlatformIO().Monitors[monitor_n];

                    // Skip if we are on the wrong monitor
                    if (tmpMonitor.MainPos.x != static_cast<float>(nearestMonitorInfo.rcMonitor.left) ||
                        tmpMonitor.MainPos.y != static_cast<float>(nearestMonitorInfo.rcMonitor.top))
                      continue;

                    // Get the screen area of the monitor
                    ImRect tmpMonRect = ImRect(tmpMonitor.MainPos, (tmpMonitor.MainPos + tmpMonitor.MainSize));

                    // Calculate the expected new size using the DPI scale of the monitor
                    ImVec2 tmpWindowSize = 
                                  (_registry.bSmallMode) ? ImVec2 ( SKIF_wSmallMode * tmpMonitor.DpiScale,
                                                               SKIF_hSmallMode * tmpMonitor.DpiScale)
                                                    : ImVec2 ( SKIF_wLargeMode * tmpMonitor.DpiScale,
                                                               SKIF_hLargeMode * tmpMonitor.DpiScale);

                    // Calculate the expected position on the new monitor
                    ImVec2 suggestedPos = ImVec2(tmpMonRect.GetCenter().x - (tmpWindowSize.x / 2.0f), tmpMonRect.GetCenter().y - (tmpWindowSize.y / 2.0f));

                    // Check if the position is within the boundaries of the
                    //  monitor and move the suggested position if it is not.
                    if (suggestedPos.x < tmpMonitor.MainPos.x)
                        suggestedPos.x = tmpMonitor.MainPos.x;
                    if (suggestedPos.y < tmpMonitor.MainPos.y)
                        suggestedPos.y = tmpMonitor.MainPos.y;

                    // Set the new window position and break out of the loop
                    ImGui::SetNextWindowPos(suggestedPos);
                    break;
                  }
                }
              }
            }
          }
        }
      }

      SnapKeys = 0;

#pragma endregion

      ImGui::Begin ( SKIF_WINDOW_TITLE_A SKIF_WINDOW_HASH,
                       nullptr,
                         ImGuiWindowFlags_NoResize          |
                         ImGuiWindowFlags_NoCollapse        |
                         ImGuiWindowFlags_NoTitleBar        |
                         ImGuiWindowFlags_NoScrollbar       | // Hide the scrollbar for the main window
                         ImGuiWindowFlags_NoScrollWithMouse   // Prevent scrolling with the mouse as well
      );

      SK_RunOnce (ImGui::GetCurrentWindow()->HiddenFramesCannotSkipItems += 2);

      HiddenFramesContinueRendering = (ImGui::GetCurrentWindowRead()->HiddenFramesCannotSkipItems > 0);
      HoverTipActive = false;

      // Update current monitors/worksize etc;
      monitor     = &ImGui::GetPlatformIO        ().Monitors [ImGui::GetCurrentWindowRead()->ViewportAllowPlatformMonitorExtend];

      // Move the invisible Win32 parent window SKIF_hWnd over to the current monitor.
      //   This solves multiple taskbars not showing SKIF's window on all monitors properly.
      if (monitor->MainPos.x != ImGui::GetMainViewport()->Pos.x ||
          monitor->MainPos.y != ImGui::GetMainViewport()->Pos.y )
      {
        MoveWindow (SKIF_hWnd, (int)monitor->MainPos.x, (int)monitor->MainPos.y, 0, 0, false);

        // Update refresh rate for the current monitor
        SKIF_Util_GetMonitorHzPeriod (SKIF_hWnd, MONITOR_DEFAULTTONEAREST, dwDwmPeriod);
        //OutputDebugString ((L"Updated refresh rate period: " + std::to_wstring (dwDwmPeriod) + L"\n").c_str());

        RecreateSwapChains = true;
      }

      float fDpiScaleFactor =
        ((io.ConfigFlags & ImGuiConfigFlags_DpiEnableScaleFonts) ? monitor->DpiScale : 1.0f);

      // RepositionSKIF -- Step 3: The Final Step -- Prevent the global DPI scale from potentially being set to outdated values
      if (RepositionSKIF)
      {
        RepositionSKIF = false;
      } else if ( monitor->WorkSize.y / fDpiScaleFactor < ((float)SKIF_hLargeMode + 40.0f) && ImGui::GetFrameCount () > 1)
      {
        SKIF_ImGui_GlobalDPIScale = (monitor->WorkSize.y / fDpiScaleFactor) / ((float)SKIF_hLargeMode / fDpiScaleFactor + 40.0f / fDpiScaleFactor);
      } else {
        SKIF_ImGui_GlobalDPIScale = (io.ConfigFlags & ImGuiConfigFlags_DpiEnableScaleFonts) ? ImGui::GetCurrentWindowRead()->Viewport->DpiScale : 1.0f;
      }

      // Rescale the style on DPI changes
      if (SKIF_ImGui_GlobalDPIScale != SKIF_ImGui_GlobalDPIScale_Last)
      {
        style.WindowPadding                         = SKIF_ImGui_DefaultStyle.WindowPadding                       * SKIF_ImGui_GlobalDPIScale;
        style.WindowRounding                        = 4.0F                                                        * SKIF_ImGui_GlobalDPIScale;
        style.WindowMinSize                         = SKIF_ImGui_DefaultStyle.WindowMinSize                       * SKIF_ImGui_GlobalDPIScale;
        style.ChildRounding                         = style.WindowRounding;
        style.PopupRounding                         = SKIF_ImGui_DefaultStyle.PopupRounding                       * SKIF_ImGui_GlobalDPIScale;
        style.FramePadding                          = SKIF_ImGui_DefaultStyle.FramePadding                        * SKIF_ImGui_GlobalDPIScale;
        style.FrameRounding                         = style.WindowRounding;
        style.ItemSpacing                           = SKIF_ImGui_DefaultStyle.ItemSpacing                         * SKIF_ImGui_GlobalDPIScale;
        style.ItemInnerSpacing                      = SKIF_ImGui_DefaultStyle.ItemInnerSpacing                    * SKIF_ImGui_GlobalDPIScale;
        style.TouchExtraPadding                     = SKIF_ImGui_DefaultStyle.TouchExtraPadding                   * SKIF_ImGui_GlobalDPIScale;
        style.IndentSpacing                         = SKIF_ImGui_DefaultStyle.IndentSpacing                       * SKIF_ImGui_GlobalDPIScale;
        style.ColumnsMinSpacing                     = SKIF_ImGui_DefaultStyle.ColumnsMinSpacing                   * SKIF_ImGui_GlobalDPIScale;
        style.ScrollbarSize                         = SKIF_ImGui_DefaultStyle.ScrollbarSize                       * SKIF_ImGui_GlobalDPIScale;
        style.ScrollbarRounding                     = SKIF_ImGui_DefaultStyle.ScrollbarRounding                   * SKIF_ImGui_GlobalDPIScale;
        style.GrabMinSize                           = SKIF_ImGui_DefaultStyle.GrabMinSize                         * SKIF_ImGui_GlobalDPIScale;
        style.GrabRounding                          = SKIF_ImGui_DefaultStyle.GrabRounding                        * SKIF_ImGui_GlobalDPIScale;
        style.TabRounding                           = style.WindowRounding;
        if (style.TabMinWidthForUnselectedCloseButton != FLT_MAX)
          style.TabMinWidthForUnselectedCloseButton = SKIF_ImGui_DefaultStyle.TabMinWidthForUnselectedCloseButton * SKIF_ImGui_GlobalDPIScale;
        style.DisplayWindowPadding                  = SKIF_ImGui_DefaultStyle.DisplayWindowPadding                * SKIF_ImGui_GlobalDPIScale;
        style.DisplaySafeAreaPadding                = SKIF_ImGui_DefaultStyle.DisplaySafeAreaPadding              * SKIF_ImGui_GlobalDPIScale;
        style.MouseCursorScale                      = SKIF_ImGui_DefaultStyle.MouseCursorScale                    * SKIF_ImGui_GlobalDPIScale;

        // These are not a part of the default style so need to assign them separately
        if (! _registry.bDisableBorders)
        {
          style.TabBorderSize                       = 1.0F                                                        * SKIF_ImGui_GlobalDPIScale;
          style.FrameBorderSize                     = 1.0F                                                        * SKIF_ImGui_GlobalDPIScale;
        }
      }

      static ImGuiTabBarFlags flagsInjection =
                ImGuiTabItemFlags_None,
                              flagsHelp =
                ImGuiTabItemFlags_None;


      // Top right window buttons
      ImVec2 topCursorPos =
        ImGui::GetCursorPos ();

      ImGui::SetCursorPos (
        ImVec2 ( SKIF_vecCurrentMode.x - 120.0f * SKIF_ImGui_GlobalDPIScale,
                                           4.0f * SKIF_ImGui_GlobalDPIScale )
      );

      ImGui::PushStyleVar (
        ImGuiStyleVar_FrameRounding, 25.0f * SKIF_ImGui_GlobalDPIScale
      );

      if ( (io.KeyCtrl && io.KeysDown['R']    && io.KeysDownDuration['R']    == 0.0f) ||
           (              io.KeysDown[VK_F5]  && io.KeysDownDuration[VK_F5]  == 0.0f)
         )
      {
        if (SKIF_Tab_Selected == UITab_Library)
          RepopulateGames   = true;

        if (SKIF_Tab_Selected == UITab_Settings)
          RefreshSettingsTab = true;
      }

      if ( (io.KeyCtrl && io.KeysDown['T']    && io.KeysDownDuration['T']    == 0.0f) ||
           (              io.KeysDown[VK_F11] && io.KeysDownDuration[VK_F11] == 0.0f) ||
            ImGui::Button ( (_registry.bSmallMode) ? ICON_FA_MAXIMIZE
                                                   : ICON_FA_MINIMIZE,
                            ImVec2 ( 40.0f * SKIF_ImGui_GlobalDPIScale,
                                      0.0f ) )
         )
      {
        _registry.bSmallMode = ! _registry.bSmallMode;
        _registry.regKVSmallMode.putData (_registry.bSmallMode);
        bExitOnInjection = false;

        PLOG_DEBUG << "Changed UI mode to " << ((_registry.bSmallMode) ? "Small Mode" : "Large Mode");

        if (_registry.bSmallMode)
        {
          // If we switch to small mode, close all popups
          ImGui::ClosePopupsOverWindow (ImGui::GetCurrentWindowRead ( ), false);
        }
        else {
          // If we switch back to large mode, re-open a few specific ones
          if (AddGamePopup == PopupState_Opened)
            AddGamePopup = PopupState_Open;

          if (UpdatePromptPopup == PopupState_Opened)
            UpdatePromptPopup = PopupState_Open;

          if (HistoryPopup == PopupState_Opened)
            HistoryPopup = PopupState_Open;

          // If SKIF was used as a launcher, initialize stuff that we did not set up while in the small mode
          if (_Signal.Launcher)
          {   _Signal.Launcher = false;

            // Register HDR toggle hotkey (if applicable)
            SKIF_Util_RegisterHDRToggleHotKey ( );

            // Kickstart the update thread
            _updater.CheckForUpdates ( );
          }
        }

        changedMode = true;

        // Hide the window for the 4 following frames as ImGui determines the sizes of items etc.
        //   This prevent flashing and elements appearing too large during those frames.
        ImGui::GetCurrentWindow()->HiddenFramesCannotSkipItems += 4;
      }

      if ( (io.KeyCtrl && io.KeysDown['1']    && io.KeysDownDuration['1']    == 0.0f)
         )
      {
        if (SKIF_Tab_Selected != UITab_Library)
            SKIF_Tab_ChangeTo  = UITab_Library;
      }

      if ( (io.KeyCtrl && io.KeysDown['2']    && io.KeysDownDuration['2']    == 0.0f)
         )
      {
        if (SKIF_Tab_Selected != UITab_Monitor)
            SKIF_Tab_ChangeTo  = UITab_Monitor;
      }

      if ( (io.KeyCtrl && io.KeysDown['3']    && io.KeysDownDuration['3']    == 0.0f)
         )
      {
        if (SKIF_Tab_Selected != UITab_Settings)
            SKIF_Tab_ChangeTo  = UITab_Settings;
      }

      if ( (io.KeyCtrl && io.KeysDown['4']    && io.KeysDownDuration['4']    == 0.0f)
         )
      {
        if (SKIF_Tab_Selected != UITab_About)
            SKIF_Tab_ChangeTo  = UITab_About;
      }

      if ( (io.KeyCtrl && io.KeysDown['A']    && io.KeysDownDuration['A']    == 0.0f)
         )
      {
        if (SKIF_Tab_Selected != UITab_Library)
            SKIF_Tab_ChangeTo  = UITab_Library;

        AddGamePopup = PopupState_Open;
      }

      if (ImGui::IsKeyPressedMap (ImGuiKey_Escape))
      {
        if (AddGamePopup      != PopupState_Closed ||
            ModifyGamePopup   != PopupState_Closed ||
            RemoveGamePopup   != PopupState_Closed ||
            UpdatePromptPopup != PopupState_Closed ||
            HistoryPopup      != PopupState_Closed  )
        {
          AddGamePopup         = PopupState_Closed;
          ModifyGamePopup      = PopupState_Closed;
          RemoveGamePopup      = PopupState_Closed;
          UpdatePromptPopup    = PopupState_Closed;
          HistoryPopup         = PopupState_Closed;

          ImGui::ClosePopupsOverWindow (ImGui::GetCurrentWindowRead ( ), false);
        }
      }

      ImGui::SameLine ();

      if ( (io.KeyCtrl && io.KeysDown['N'] && io.KeysDownDuration['N'] == 0.0f) ||
            ImGui::Button (ICON_FA_WINDOW_MINIMIZE, ImVec2 ( 30.0f * SKIF_ImGui_GlobalDPIScale,
                                                             0.0f ) ) )
      {
        ShowWindow (hWnd, SW_MINIMIZE);
      }

      ImGui::SameLine ();

      if ( (io.KeyCtrl && io.KeysDown['Q'] && io.KeysDownDuration['Q'] == 0.0f) ||
            ImGui::Button (ICON_FA_XMARK, ImVec2 ( 30.0f * SKIF_ImGui_GlobalDPIScale,
                                                          0.0f ) )
          || bKeepWindowAlive == false
         )
      {
        if (_registry.bCloseToTray && bKeepWindowAlive && ! SKIF_isTrayed)
        {
          bKeepWindowAlive = true;
          ShowWindow       (hWnd, SW_MINIMIZE);
          ShowWindow       (hWnd, SW_HIDE);
          UpdateWindow     (hWnd);
          SKIF_isTrayed    = true;
        }

        else
        {
          if (_inject.bCurrentState && ! _registry.bAllowBackgroundService )
            _inject._StartStopInject (true);

          bKeepProcessAlive = false;
        }
      }

      ImGui::PopStyleVar ();
      
      if (_registry.bCloseToTray)
        SKIF_ImGui_SetHoverTip ("This app will close to the notification area");
      else if (_inject.bCurrentState && _registry.bAllowBackgroundService)
        SKIF_ImGui_SetHoverTip ("Service continues running after this app is closed");

      ImGui::SetCursorPos (topCursorPos);

      // End of top right window buttons

      ImGui::BeginGroup ();

      // Begin Small Mode
#pragma region UI: Small Mode

      if (_registry.bSmallMode)
      {
        SKIF_Tab_Selected = UITab_SmallMode;

        auto smallMode_id =
          ImGui::GetID ("###Small_Mode_Frame");

        // A new line to allow the window titlebar buttons to be accessible
        ImGui::NewLine ();

        SKIF_ImGui_BeginChildFrame ( smallMode_id,
          ImVec2 ( 400.0f * SKIF_ImGui_GlobalDPIScale,
                    12.0f * ImGui::GetTextLineHeightWithSpacing () ),
            ImGuiWindowFlags_AlwaysUseWindowPadding | ImGuiWindowFlags_NavFlattened      |
            ImGuiWindowFlags_NoScrollbar            | ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoBackground
        );

        _inject._GlobalInjectionCtl ();

        ImGui::EndChildFrame ();

        SKIF_ImGui_ServiceMenu ();

        // Shelly the Ghost (Small Mode)

        ImGui::SetCursorPosX (2.0f);

        ImGui::SetCursorPosY (
          7.0f * SKIF_ImGui_GlobalDPIScale
        );

        ImGui::Text ("");

        SKIF_UI_DrawShellyTheGhost ( );
      } // End Small Mode

#pragma endregion

      // Begin Large Mode
#pragma region UI: Large Mode

      else
      {
        ImGui::BeginTabBar ( "###SKIF_TAB_BAR",
                               ImGuiTabBarFlags_FittingPolicyResizeDown |
                               ImGuiTabBarFlags_FittingPolicyScroll );


        if (ImGui::BeginTabItem (" " ICON_FA_GAMEPAD " Library ", nullptr, ImGuiTabItemFlags_NoTooltip | ((SKIF_Tab_ChangeTo == UITab_Library) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None)))
        {
          if (! _registry.bFirstLaunch)
          {
            // Select the About tab on first launch
            _registry.bFirstLaunch = ! _registry.bFirstLaunch;
            SKIF_Tab_ChangeTo = UITab_About;

            // Store in the registry so this only occur once.
            _registry.regKVFirstLaunch.putData(_registry.bFirstLaunch);
          }

          extern float fTint;
          if (SKIF_Tab_Selected != UITab_Library)
          {
            // Reset the dimmed cover when going back to the tab
            if (_registry.iDimCovers == 2)
              fTint = 0.75f;
          }

          SKIF_Tab_Selected = UITab_Library;
          if (SKIF_Tab_ChangeTo == UITab_Library)
            SKIF_Tab_ChangeTo = UITab_None;
            

          extern void
            SKIF_UI_Tab_DrawLibrary (void);
            SKIF_UI_Tab_DrawLibrary (     );

          ImGui::EndTabItem ();
        }


        if (ImGui::BeginTabItem (" " ICON_FA_LIST_CHECK " Monitor ", nullptr, ImGuiTabItemFlags_NoTooltip | ((SKIF_Tab_ChangeTo == UITab_Monitor) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None)))
        {
          SKIF_ImGui_BeginTabChildFrame ();

          extern void
            SKIF_UI_Tab_DrawMonitor (void);
            SKIF_UI_Tab_DrawMonitor (    );

          ImGui::EndChildFrame    ( );
          ImGui::EndTabItem       ( );
        }

        // Unload the SpecialK DLL file if the tab is not selected
        else if (hModSpecialK != 0)
        {
          FreeLibrary (hModSpecialK);
          hModSpecialK = nullptr;
        }

        if (ImGui::BeginTabItem (" " ICON_FA_GEAR " Settings ", nullptr, ImGuiTabItemFlags_NoTooltip | ((SKIF_Tab_ChangeTo == UITab_Settings) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None)))
        {
          SKIF_ImGui_BeginTabChildFrame ();

          SKIF_UI_Tab_DrawSettings( );

          ImGui::EndChildFrame    ( );
          ImGui::EndTabItem       ( );
        }
        
        if (ImGui::BeginTabItem (" " ICON_FA_COMMENT " About ", nullptr, ImGuiTabItemFlags_NoTooltip | ((SKIF_Tab_ChangeTo == UITab_About) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None)))
        {
          SKIF_ImGui_BeginTabChildFrame ();

          SKIF_Tab_Selected = UITab_About;
          if (SKIF_Tab_ChangeTo == UITab_About)
            SKIF_Tab_ChangeTo = UITab_None;

          // About Tab
          SKIF_UI_Tab_DrawAbout   ( );

          ImGui::EndChildFrame    ( );
          ImGui::EndTabItem       ( );
        }

        // Shelly the Ghost

        float title_len = (tinyDPIFonts) ? ImGui::CalcTextSize(SKIF_WINDOW_TITLE_SHORT_A).x : ImGui::CalcTextSize(SKIF_WINDOW_TITLE_A).x;
        float title_pos = SKIF_vecCurrentMode.x / 2.0f - title_len / 2.0f;

        ImGui::SetCursorPosX (title_pos);

        ImGui::SetCursorPosY (
          7.0f * SKIF_ImGui_GlobalDPIScale
        );

        ImGui::TextColored (ImVec4 (0.5f, 0.5f, 0.5f, 1.f),
                              (tinyDPIFonts) ? SKIF_WINDOW_TITLE_SHORT_A
                                             : SKIF_WINDOW_TITLE_A);
        
        SKIF_UI_DrawShellyTheGhost ( );

        ImGui::EndTabBar          ( );
      } // End Large Mode

#pragma endregion

      ImGui::EndGroup             ( );

      // Status Bar at the bottom
      if ( ! _registry.bSmallMode        &&
           ! _registry.bDisableStatusBar )
      {
        // This counteracts math performed on SKIF_vecLargeMode.y at the beginning of the frame
        float statusBarY = ImGui::GetWindowSize().y;
              statusBarY -= 31.0f * SKIF_ImGui_GlobalDPIScale;
              statusBarY -= (_registry.bDisableTooltips) ? 18.0f * SKIF_ImGui_GlobalDPIScale : 0.0f;
        ImGui::SetCursorPosY (statusBarY);

        if (! _registry.bDisableBorders)
          ImGui::Separator    (       );

        // End Separation

        // Begin Add Game
        ImVec2 tmpPos = ImGui::GetCursorPos();

        static bool btnHovered = false;
        ImGui::PushStyleColor (ImGuiCol_Button,        ImGui::GetStyleColorVec4 (ImGuiCol_WindowBg));
        ImGui::PushStyleColor (ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4 (ImGuiCol_WindowBg)); //ImColor (64,  69,  82).Value);
        ImGui::PushStyleColor (ImGuiCol_ButtonActive,  ImGui::GetStyleColorVec4 (ImGuiCol_WindowBg)); //ImColor (56, 60, 74).Value);

        if (btnHovered)
          ImGui::PushStyleColor (ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_SKIF_TextCaption)); //ImVec4(1, 1, 1, 1));
        else
          ImGui::PushStyleColor (ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_SKIF_TextBase)); //ImVec4(0.5f, 0.5f, 0.5f, 1.f));

        ImGui::PushStyleVar (ImGuiStyleVar_FrameBorderSize, 0.0f);
        if (ImGui::Button ( ICON_FA_SQUARE_PLUS " Add Game"))
        {
          AddGamePopup = PopupState_Open;
          if (SKIF_Tab_Selected != UITab_Library)
            SKIF_Tab_ChangeTo = UITab_Library;
        }
        ImGui::PopStyleVar  ( );

        btnHovered = ImGui::IsItemHovered() || ImGui::IsItemActive();

        ImGui::PopStyleColor (4);

        ImGui::SetCursorPos(tmpPos);
        // End Add Game

        // Begin Pulsating Refresh Icon
        if (_updater.IsRunning ( ))
        {
          ImGui::SetCursorPosX (
            ImGui::GetCursorPosX () +
            ImGui::GetWindowSize ().x -
              ( ImGui::CalcTextSize ( ICON_FA_ROTATE ).x ) -
            ImGui::GetCursorPosX () -
            ImGui::GetStyle   ().ItemSpacing.x * 2
          );

          ImGui::SetCursorPosY ( ImGui::GetCursorPosY () + style.FramePadding.y);

          ImGui::TextColored ( ImGui::GetStyleColorVec4(ImGuiCol_SKIF_TextCaption) *
                                ImVec4 (0.75f, 0.75f, 0.75f, 0.50f + 0.5f * (float)sin (SKIF_Util_timeGetTime() * 1 * 3.14 * 2)
                               ), ICON_FA_ROTATE );
        }

        ImGui::SetCursorPos(tmpPos);
        // End Refresh Icon

        // Begin Status Bar Text
        auto _StatusPartSize = [&](std::string& part) -> float
        {
          return
            part.empty () ?
                      0.0f : ImGui::CalcTextSize (
                                            part.c_str ()
                                                ).x;
        };

        float fStatusWidth = _StatusPartSize (SKIF_StatusBarText),
              fHelpWidth   = _StatusPartSize (SKIF_StatusBarHelp);

        ImGui::SetCursorPosX (
          ImGui::GetCursorPosX () +
          ImGui::GetWindowSize ().x -
            ( fStatusWidth +
              fHelpWidth ) -
          ImGui::GetCursorPosX () -
          ImGui::GetStyle   ().ItemSpacing.x * 2
        );

        ImGui::SetCursorPosY ( ImGui::GetCursorPosY () + style.FramePadding.y);

        ImGui::TextColored ( ImGui::GetStyleColorVec4(ImGuiCol_SKIF_TextCaption) * ImVec4 (0.75f, 0.75f, 0.75f, 1.00f),
                                "%s", SKIF_StatusBarText.c_str ()
        );

        if (! SKIF_StatusBarHelp.empty ())
        {
          ImGui::SameLine ();
          ImGui::SetCursorPosX (
            ImGui::GetCursorPosX () -
            ImGui::GetStyle      ().ItemSpacing.x
          );
          ImGui::TextDisabled ("%s", SKIF_StatusBarHelp.c_str ());
        }

        // Clear the status every frame, it's mostly used for mouse hover tooltips.
        SKIF_StatusBarText.clear ();
        SKIF_StatusBarHelp.clear ();

        // End Status Bar Text
      }


      // Font warning
      if (failedLoadFontsPrompt && ! HiddenFramesContinueRendering)
      {
        failedLoadFontsPrompt = false;

        ImGui::OpenPopup ("###FailedFontsPopup");
      }
      

      float ffailedLoadFontsWidth = 400.0f * SKIF_ImGui_GlobalDPIScale;
      ImGui::SetNextWindowSize (ImVec2 (ffailedLoadFontsWidth, 0.0f));

      if (ImGui::BeginPopupModal ("Fonts failed to load###FailedFontsPopup", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize))
      {
        ImGui::TreePush    ("");

        SKIF_ImGui_Spacing ( );

        ImGui::TextWrapped ("The selected character sets failed to load due to system limitations and have been reset.");
        ImGui::NewLine     ( );
        ImGui::TextWrapped ("Please limit the selection to only the most essential.");

        SKIF_ImGui_Spacing ( );
        SKIF_ImGui_Spacing ( );

        ImVec2 vButtonSize = ImVec2(80.0f * SKIF_ImGui_GlobalDPIScale, 0.0f);

        ImGui::SetCursorPosX (ffailedLoadFontsWidth / 2 - vButtonSize.x / 2);

        if (ImGui::Button  ("OK", vButtonSize))
          ImGui::CloseCurrentPopup ( );

        SKIF_ImGui_Spacing ( );

        ImGui::TreePop     ( );

        ImGui::EndPopup ( );
      }

      // Uses a Directory Watch signal, so this is cheap; do it once every frame.
      svcTransitionFromPendingState = _inject._TestServletRunlevel (false);

      // Another Directory Watch signal to check if DLL files should be refreshed.
      // 
      // TODO: This directory watch gets assigned to the current tab only, meaning it won't
      //       trigger an automated refresh if the user switches tabs before it is signaled.
      // 
      //       This also means the main DLL refresh watch is tied to the tab SKIF opens up
      //       on, whether that be SMALL MODE, LIBRARY, or ABOUT tab (first launch).
      static SKIF_DirectoryWatch root_folder;
      if (root_folder.isSignaled (_path_cache.specialk_install, true))
      {
        // If the Special K DLL file is currently loaded, unload it
        if (hModSpecialK != 0)
        {
          FreeLibrary (hModSpecialK);
          hModSpecialK = nullptr;
        }

        _inject._DanceOfTheDLLFiles     ();
        _inject._RefreshSKDLLVersions   ();
      }

      static std::wstring updateRoot = SK_FormatStringW (LR"(%ws\Version\)", _path_cache.specialk_userdata);
      static float  UpdateAvailableWidth = 0.0f;
      static float  calculatedWidth      = 0.0f;
      static float  NumLines             = 0;
      static size_t NumCharsOnLine       = 0;
      static std::vector<char> vecNotes;

      if (UpdatePromptPopup == PopupState_Open && ! _registry.bSmallMode && ! HiddenFramesContinueRendering && ! ImGui::IsAnyPopupOpen ( ) && ! ImGui::IsMouseDragging (ImGuiMouseButton_Left))
      {
        //UpdateAvailableWidth = ImGui::CalcTextSize ((SK_WideCharToUTF8 (newVersion.description) + " is ready to be installed.").c_str()).x + 3 * ImGui::GetStyle().ItemSpacing.x;
        UpdateAvailableWidth = 360.0f;

        if (vecNotes.empty())
        {
          calculatedWidth = 0.0f;
          NumLines        = 0.0f;
          NumCharsOnLine  = 0;

          if (! _updater.GetResults ( ).release_notes.empty())
          {
            std::string strNotes = _updater.GetResults ( ).release_notes;

            // Ensure the text wraps at every 110 character (longest line used yet, in v0.8.32)
            strNotes = TextFlow::Column(strNotes).width(110).toString();

            // Calc longest line and number of lines
            std::istringstream iss(strNotes);
            for (std::string line; std::getline(iss, line); NumLines++)
              if (line.length() > NumCharsOnLine)
                NumCharsOnLine = line.length();

            // 8.0f  per character
            // 15.0f for the scrollbar
            calculatedWidth = static_cast<float>(NumCharsOnLine) * 8.0f + 15.0f;

            // Populate the vector
            vecNotes.push_back ('\n');

            for (size_t i = 0; i < strNotes.length(); i++)
              vecNotes.push_back(strNotes[i]);

            vecNotes.push_back ('\n');

            // Ensure the vector array is double null terminated
            vecNotes.push_back ('\0');
            vecNotes.push_back ('\0');

            // Increase NumLines by 3, two from vecNotes.push_back and
            //  two from ImGui's love of having one and a half empty line below content
            NumLines += 3.5f;

            // Only allow up to 40 lines at most
            if (NumLines > 40.0f)
              NumLines = 40.0f;
          }
        }

        if ((calculatedWidth * SKIF_ImGui_GlobalDPIScale) > UpdateAvailableWidth)
          UpdateAvailableWidth = calculatedWidth;

        ImGui::OpenPopup ("###UpdatePrompt");
      }

      // Update Available prompt
      // 730px    - Full popup width
      // 715px    - Release Notes width
      //  15px    - Approx. scrollbar width
      //   7.78px - Approx. character width (700px / 90 characters)
      ImGui::SetNextWindowSize (
        ImVec2 ( UpdateAvailableWidth * SKIF_ImGui_GlobalDPIScale,
                   0.0f )
      );
      ImGui::SetNextWindowPos (ImGui::GetCurrentWindowRead()->Viewport->GetMainRect().GetCenter(), ImGuiCond_Always, ImVec2 (0.5f, 0.5f));

      bool allowWindowMove = true;

      if (ImGui::BeginPopupModal ( "Version Available###UpdatePrompt", nullptr,
                                     ImGuiWindowFlags_NoResize         |
                                     ImGuiWindowFlags_NoMove           |
                                     ImGuiWindowFlags_AlwaysAutoResize )
         )
      {
#ifdef _WIN64
        std::wstring currentVersion = SK_UTF8ToWideChar (_inject.SKVer64);
#else
        std::wstring currentVersion = SK_UTF8ToWideChar (_inject.SKVer32);
#endif
        std::string compareLabel;
        ImVec4      compareColor;
        bool        compareNewer = (SKIF_Util_CompareVersionStrings (_updater.GetResults().version, currentVersion) > 0);

        if (UpdatePromptPopup == PopupState_Open)
        {
          // Set the popup as opened after it has appeared (fixes popup not opening from other tabs)
          ImGuiWindow* window = ImGui::FindWindowByName ("###UpdatePrompt");
          if (window != nullptr && ! window->Appearing)
            UpdatePromptPopup = PopupState_Opened;
        }

        if (compareNewer)
        {
          compareLabel = "This version is newer than currently installed.";
          compareColor = ImGui::GetStyleColorVec4 (ImGuiCol_SKIF_Success);
        }
        else
        {
          compareLabel = "This version is older than currently installed!";
          compareColor = ImGui::GetStyleColorVec4 (ImGuiCol_SKIF_Warning);
        }

        SKIF_ImGui_Spacing ();

        std::string updateTxt = "is ready to be installed.";

        if ((_updater.GetState ( ) & UpdateFlags_Downloaded) != UpdateFlags_Downloaded)
          updateTxt = "is available for download.";

        float fX = (ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(((_updater.GetResults().description) + updateTxt).c_str()).x + (((compareNewer) ? 2 : 1) * ImGui::GetStyle().ItemSpacing.x)) / 2;

        ImGui::SetCursorPosX(fX);

        ImGui::TextColored (ImGui::GetStyleColorVec4 (ImGuiCol_SKIF_Success), (_updater.GetResults().description).c_str());
        ImGui::SameLine ( );
        ImGui::Text (updateTxt.c_str());

        SKIF_ImGui_Spacing ();

        if ((_updater.GetState ( ) & UpdateFlags_Failed) == UpdateFlags_Failed)
        {
          ImGui::PushStyleColor (ImGuiCol_Text, ImGui::GetStyleColorVec4 (ImGuiCol_SKIF_Warning));
          ImGui::TextWrapped ("A failed download was detected! Click Download below to try again.");
          ImGui::Text        ("In case of continued errors, consult SKIF.log for more details.");
          ImGui::PopStyleColor  ( );

          SKIF_ImGui_Spacing ();
        }

        ImGui::Text     ("Target Folder:");
        ImGui::PushStyleColor (ImGuiCol_Text, ImGui::GetStyleColorVec4 (ImGuiCol_SKIF_Info));
        ImGui::TextWrapped    (SK_WideCharToUTF8 (_path_cache.specialk_install).c_str());
        ImGui::PopStyleColor  ( );

        SKIF_ImGui_Spacing ();

        if (! vecNotes.empty())
        {
          if (! _updater.GetResults().description_installed.empty())
          {
            ImGui::Text           ("Changes from");
            ImGui::SameLine       ( );
            ImGui::TextColored    (ImGui::GetStyleColorVec4 (ImGuiCol_SKIF_TextBase), (_updater.GetResults().description_installed + ":").c_str());
          }
          else {
            ImGui::Text           ("Changes:");
          }
          ImGui::PushStyleColor (ImGuiCol_Text, ImGui::GetStyleColorVec4 (ImGuiCol_SKIF_TextBase));
          ImGui::PushFont       (fontConsolas);
          ImGui::InputTextEx    ( "###UpdatePromptChanges", "The update does not contain any release notes...",
                                  vecNotes.data(), static_cast<int>(vecNotes.size()),
                                    ImVec2 ( (UpdateAvailableWidth - 15.0f) * SKIF_ImGui_GlobalDPIScale,
                                        (fontConsolas->FontSize * NumLines) * SKIF_ImGui_GlobalDPIScale ),
                                      ImGuiInputTextFlags_Multiline | ImGuiInputTextFlags_ReadOnly );

          if (ImGui::IsItemActive    ( ))
            allowWindowMove = false;

          ImGui::PopFont        ( );
          ImGui::PopStyleColor  ( );

          SKIF_ImGui_Spacing ();
        }

        fX = (ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(compareLabel.c_str()).x + (((compareNewer) ? 2 : 1) * ImGui::GetStyle().ItemSpacing.x)) / 2;

        ImGui::SetCursorPosX(fX);
          
        ImGui::TextColored (compareColor, compareLabel.c_str());

        SKIF_ImGui_Spacing ();

        fX = (ImGui::GetContentRegionAvail().x - (((compareNewer) ? 3 : 2) * 100 * SKIF_ImGui_GlobalDPIScale) - (((compareNewer) ? 2 : 1) * ImGui::GetStyle().ItemSpacing.x)) / 2;

        ImGui::SetCursorPosX(fX);

        std::string btnLabel = "Install";

        if ((_updater.GetState ( ) & UpdateFlags_Downloaded) != UpdateFlags_Downloaded)
          btnLabel = "Download";

        if (ImGui::Button (btnLabel.c_str(), ImVec2 ( 100 * SKIF_ImGui_GlobalDPIScale,
                                                       25 * SKIF_ImGui_GlobalDPIScale )))
        {
          if (btnLabel == "Download")
          {
            _registry.wsIgnoreUpdate = L"";
            _registry.regKVIgnoreUpdate.putData (_registry.wsIgnoreUpdate);

            // Trigger a new check for updates (which should download the installer)
            _updater.CheckForUpdates ( );
          }
          else {
            if (_inject.bCurrentState)
              _inject._StartStopInject(true);

            std::wstring args = SK_FormatStringW (LR"(/VerySilent /NoRestart /Shortcuts=false /DIR="%ws")", _path_cache.specialk_install);

            SKIF_Util_OpenURI (updateRoot + _updater.GetResults().filename, SW_SHOWNORMAL, L"OPEN", args.c_str());

            //bExitOnInjection = true; // Used to close SKIF once the service had been stopped

            //Sleep(50);
            //bKeepProcessAlive = false;

            vecNotes.clear();
          }

          UpdatePromptPopup = PopupState_Closed;
          ImGui::CloseCurrentPopup ();
        }

        if (ImGui::IsItemActive    ( ))
          allowWindowMove = false;

        ImGui::SameLine ();
        ImGui::Spacing  ();
        ImGui::SameLine ();

        if (compareNewer)
        {
          if (ImGui::Button ("Ignore", ImVec2 ( 100 * SKIF_ImGui_GlobalDPIScale,
                                                 25 * SKIF_ImGui_GlobalDPIScale )))
          {
            _registry.regKVIgnoreUpdate.putData(SK_UTF8ToWideChar(_updater.GetResults().description));

            vecNotes.clear();
            UpdatePromptPopup = PopupState_Closed;
            ImGui::CloseCurrentPopup ();
          }

          SKIF_ImGui_SetHoverTip ("SKIF will not prompt about this version again.");

          ImGui::SameLine ();
          ImGui::Spacing  ();
          ImGui::SameLine ();
        }

        if (ImGui::IsItemActive    ( ))
          allowWindowMove = false;

        if (ImGui::Button ("Cancel", ImVec2 ( 100 * SKIF_ImGui_GlobalDPIScale,
                                               25 * SKIF_ImGui_GlobalDPIScale )))
        {
          vecNotes.clear();
          UpdatePromptPopup = PopupState_Closed;
          ImGui::CloseCurrentPopup ();
        }

        if (ImGui::IsItemActive    ( ))
          allowWindowMove = false;

        ImGui::EndPopup ();
      }

      // Special handling to allow the main window to be moved when the update popup is present
      // This is not allowed for any other popups
      if (UpdatePromptPopup == PopupState_Opened && allowWindowMove && ImGui::IsMouseDragging (ImGuiMouseButton_Left))
      {
        ImGui::StartMouseMovingWindow (ImGui::GetCurrentWindowRead());
        UpdatePromptPopup = PopupState_Open;
      }
      
      static float  HistoryPopupWidth          = 0.0f;
      static float  calcHistoryPopupWidth      = 0.0f;
      static float  HistoryPopupNumLines       = 0;
      static size_t HistoryPopupNumCharsOnLine = 0;
      static std::vector<char> vecHistory;
      static std::string HistoryPopupTitle;

      if (HistoryPopup == PopupState_Open && ! HiddenFramesContinueRendering && ! ImGui::IsAnyPopupOpen ( ))
      {
        //HistoryPopupWidth = ImGui::CalcTextSize ((SK_WideCharToUTF8 (newVersion.description) + " is ready to be installed.").c_str()).x + 3 * ImGui::GetStyle().ItemSpacing.x;
        HistoryPopupWidth = 360.0f;

        if (vecHistory.empty())
        {
          calcHistoryPopupWidth = 0.0f;
          HistoryPopupNumLines        = 0.0f;
          HistoryPopupNumCharsOnLine  = 0;

          if (! _updater.GetHistory ( ).empty ( ))
          {
            std::string strHistory = _updater.GetHistory ( );

            // Ensure the text wraps at every 110 character (longest line used yet, in v0.8.32)
            strHistory = TextFlow::Column(strHistory).width(110).toString();

            // Calc longest line and number of lines
            std::istringstream iss(strHistory);
            for (std::string line; std::getline(iss, line); HistoryPopupNumLines++)
              if (line.length() > HistoryPopupNumCharsOnLine)
                HistoryPopupNumCharsOnLine = line.length();

            // 8.0f  per character
            // 15.0f for the scrollbar
            calcHistoryPopupWidth = static_cast<float>(HistoryPopupNumCharsOnLine) * 8.0f + 15.0f;

            // Populate the vector
            vecHistory.push_back ('\n');

            for (size_t i = 0; i < strHistory.length(); i++)
              vecHistory.push_back(strHistory[i]);

            vecHistory.push_back ('\n');

            // Ensure the vector array is double null terminated
            vecHistory.push_back ('\0');
            vecHistory.push_back ('\0');

            // Increase NumLines by 3, two from vecHistory.push_back and
            //  two from ImGui's love of having one and a half empty line below content
            HistoryPopupNumLines += 3.5f;

            // Only allow up to 20 lines at most
            if (HistoryPopupNumLines > 40.0f)
              HistoryPopupNumLines = 40.0f;
          }
        }

        if ((calcHistoryPopupWidth * SKIF_ImGui_GlobalDPIScale) > HistoryPopupWidth)
          HistoryPopupWidth = calcHistoryPopupWidth;

        HistoryPopupTitle = "Changelog (" + _updater.GetChannel()->first + ")###History";

        ImGui::OpenPopup ("###History");
      
        ImGui::SetNextWindowSize (
          ImVec2 ( HistoryPopupWidth * SKIF_ImGui_GlobalDPIScale,
                      0.0f )
        );
        ImGui::SetNextWindowPos (ImGui::GetCurrentWindowRead()->Viewport->GetMainRect().GetCenter(), ImGuiCond_Always, ImVec2 (0.5f, 0.5f));
      }

      if (ImGui::BeginPopupModal (HistoryPopupTitle.c_str(), nullptr,
                                  ImGuiWindowFlags_NoResize |
                                  ImGuiWindowFlags_NoMove |
                                  ImGuiWindowFlags_AlwaysAutoResize )
         )
      {
        if (HistoryPopup == PopupState_Open)
        {
          // Set the popup as opened after it has appeared (fixes popup not opening from other tabs)
          ImGuiWindow* window = ImGui::FindWindowByName ("###History");
          if (window != nullptr && ! window->Appearing)
            HistoryPopup = PopupState_Opened;
        }

        /*
        SKIF_ImGui_Spacing ();

        float fX = (ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(updateTxt.c_str()).x + ImGui::GetStyle().ItemSpacing.x) / 2;

        ImGui::SetCursorPosX(fX);

        ImGui::TextColored (ImGui::GetStyleColorVec4 (ImGuiCol_SKIF_Success), updateTxt.c_str());
        */

        ImGui::Text        ("You are currently using");
        ImGui::SameLine    ( );
        ImGui::TextColored (ImGui::GetStyleColorVec4 (ImGuiCol_SKIF_Info), ("Special K v " + _inject.SKVer64).c_str());

        SKIF_ImGui_Spacing ();

        if (! vecHistory.empty())
        {
          ImGui::PushStyleColor (ImGuiCol_Text, ImGui::GetStyleColorVec4 (ImGuiCol_SKIF_TextBase));
          ImGui::PushFont       (fontConsolas);
          ImGui::InputTextEx    ( "###HistoryChanges", "No historical changes detected...",
                                  vecHistory.data(), static_cast<int>(vecHistory.size()),
                                    ImVec2 ( (HistoryPopupWidth - 15.0f) * SKIF_ImGui_GlobalDPIScale,
                         (fontConsolas->FontSize * HistoryPopupNumLines) * SKIF_ImGui_GlobalDPIScale ),
                                      ImGuiInputTextFlags_Multiline | ImGuiInputTextFlags_ReadOnly );

          ImGui::PopFont        ( );
          ImGui::PopStyleColor  ( );

          SKIF_ImGui_Spacing ();
        }

        SKIF_ImGui_Spacing ();

        float fX = (ImGui::GetContentRegionAvail().x - 100 * SKIF_ImGui_GlobalDPIScale) / 2;

        ImGui::SetCursorPosX(fX);

        if (ImGui::Button ("Close", ImVec2 ( 100 * SKIF_ImGui_GlobalDPIScale,
                                              25 * SKIF_ImGui_GlobalDPIScale )))
        {
          vecHistory.clear ( );
          HistoryPopup = PopupState_Closed;
          ImGui::CloseCurrentPopup ();
        }

        ImGui::EndPopup ();
      }

      // Ensure the taskbar overlay icon always shows the correct state
      if (_inject.bTaskbarOverlayIcon != _inject.bCurrentState)
        _inject._SetTaskbarOverlay      (_inject.bCurrentState);

      monitor_extent =
        ImGui::GetWindowAllowedExtentRect (
          ImGui::GetCurrentWindowRead   ()
        );
      windowPos      = ImGui::GetWindowPos ();
      windowRect.Min = ImGui::GetWindowPos ();
      windowRect.Max = ImGui::GetWindowPos () + ImGui::GetWindowSize ();

      if (! HoverTipActive)
        HoverTipDuration = 0;

      // This allows us to ensure the window gets set within the workspace on the second frame after launch
      SK_RunOnce (
        changedMode = true
      );

      // This allows us to compact the working set on launch
      SK_RunOnce (
        invalidatedFonts = SKIF_Util_timeGetTime ( )
      );

      if (invalidatedFonts > 0 &&
          invalidatedFonts + 500 < SKIF_Util_timeGetTime())
      {
        SKIF_Util_CompactWorkingSet ();
        invalidatedFonts = 0;
      }

      ImGui::End();
    }

#pragma endregion

    SK_RunOnce (_inject._InitializeJumpList ( ));

    // If there is any popups opened when SKIF is unfocused and not hovered, close them.
    if (! SKIF_ImGui_IsFocused ( ) && ! ImGui::IsAnyItemHovered ( ) && ImGui::IsAnyPopupOpen ( ))
    {
      // But don't close those of interest
      if (     AddGamePopup != PopupState_Open   &&
               AddGamePopup != PopupState_Opened &&
               ConfirmPopup != PopupState_Open   &&
               ConfirmPopup != PopupState_Opened &&
            ModifyGamePopup != PopupState_Open   &&
            ModifyGamePopup != PopupState_Opened &&
          UpdatePromptPopup != PopupState_Open   &&
          UpdatePromptPopup != PopupState_Opened &&
               HistoryPopup != PopupState_Open   &&
               HistoryPopup != PopupState_Opened)
        ImGui::ClosePopupsOverWindow (ImGui::GetCurrentWindowRead ( ), false);
    }

    // Actual rendering is conditional, this just processes input and ends the ImGui frame.
    ImGui::Render (); // also calls ImGui::EndFrame ();

    // Conditional rendering
    bool bRefresh = (SKIF_isTrayed || IsIconic (hWnd)) ? false : true;

    if (invalidatedDevice > 0 && SKIF_Tab_Selected == UITab_Library)
      bRefresh = false;

    if ( bRefresh )
    {
      if (! startedMinimized && ! SKIF_isTrayed)
      {
        ImGui_ImplDX11_RenderDrawData (ImGui::GetDrawData ());

        // Update, Render and Present the main and any additional Platform Windows
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
          ImGui::UpdatePlatformWindows        ();
          ImGui::RenderPlatformWindowsDefault (); // Eventually calls ImGui_ImplDX11_SwapBuffers ( ) which Presents ( )
        }
      }
    }

    if ( startedMinimized && SKIF_ImGui_IsFocused ( ) )
    {
      startedMinimized = false;
      if ( _registry.bOpenAtCursorPosition )
        RepositionSKIF = true;
    }

    // Release any leftover resources from last frame
    CComPtr <IUnknown> pResource = nullptr;
    while (! SKIF_ResourcesToFree.empty ())
    {
      if (SKIF_ResourcesToFree.try_pop (pResource))
      {
        PLOG_VERBOSE << "SKIF_ResourcesToFree: Releasing " << pResource.p;
        pResource.p->Release();
      }
      
      if (invalidatedDevice == 2)
        invalidatedDevice = 0;
    }

    // If process should stop, post WM_QUIT
    if ((! bKeepProcessAlive) && hWnd != 0)
      PostMessage (hWnd, WM_QUIT, 0x0, 0x0);

    // GamepadInputPump child thread
    static auto thread =
      _beginthreadex ( nullptr, 0x0, [](void *) -> unsigned
      {
        CRITICAL_SECTION            GamepadInputPump = { };
        InitializeCriticalSection (&GamepadInputPump);
        EnterCriticalSection      (&GamepadInputPump);
        
        SKIF_Util_SetThreadDescription (GetCurrentThread (), L"SKIF_GamepadInputPump");

        DWORD packetLast = 0,
              packetNew  = 0;

        while (IsWindow (SKIF_hWnd))
        {
          // Only act on new gamepad input if we are actually focused.
          // This prevents SKIF from acting on gameapd input when unfocused,
          // but otherwise refreshes due to some status change.
          if (SKIF_ImGui_IsFocused ( ))
          {
            //OutputDebugString(L"Is focused\n");

            // TODO: Redo implementation -- using ImGui_ImplWin32_UpdateGamepads () here is not thread-safe,
            // This is basically called twice -- once from the main thread and once here, which introduces
            // instability (and is doubtless why ImGui upgrade branch crashes). We should instead rely on
            // two separate calls: one here which doesn't update ImGui's variables, and one in the main
            // thread which do.
            extern DWORD ImGui_ImplWin32_UpdateGamepads (void);
            packetNew  = ImGui_ImplWin32_UpdateGamepads ( );

            if (packetNew  > 0  &&
                packetNew != packetLast)
            {
              packetLast = packetNew;
              //SendMessageTimeout (SKIF_hWnd, WM_NULL, 0x0, 0x0, 0x0, 100, nullptr);
              PostMessage (SKIF_hWnd, WM_SKIF_GAMEPAD, 0x0, 0x0);
            }
          }

          // If we are unfocused, sleep until we're woken up by WM_SETFOCUS
          else
          {
            //OutputDebugString(L"Entering sleep\n");
            SK_RunOnce (SKIF_Util_CompactWorkingSet ());

            SleepConditionVariableCS (
              &SKIF_IsFocused, &GamepadInputPump,
                INFINITE
            );
          }

          // XInput tends to have ~3-7 ms of latency between updates
          //   best-case, try to delay the next poll until there's
          //     new data.
          Sleep (5);
        }

        LeaveCriticalSection  (&GamepadInputPump);
        DeleteCriticalSection (&GamepadInputPump);

        _endthreadex (0x0);

        return 0;
      }, nullptr, 0x0, nullptr
    );

    // Handle dynamic pausing
    bool pause = false;
    static int
      renderAdditionalFrames = 0;

    bool input = SKIF_ImGui_IsAnyInputDown ( ) || uiLastMsg == WM_SKIF_GAMEPAD ||
                   (uiLastMsg >= WM_MOUSEFIRST && uiLastMsg <= WM_MOUSELAST)   ||
                   (uiLastMsg >= WM_KEYFIRST   && uiLastMsg <= WM_KEYLAST  );
    
    // We want SKIF to continue rendering in some specific scenarios
    ImGuiWindow* wnd = ImGui::FindWindowByName ("###KeyboardHint");
    if (wnd != nullptr && wnd->Active)
      renderAdditionalFrames = ImGui::GetFrameCount ( ) + 3; // If the keyboard hint/search is active
    else if (uiLastMsg == WM_SETCURSOR  || uiLastMsg == WM_TIMER   ||
             uiLastMsg == WM_SETFOCUS   || uiLastMsg == WM_KILLFOCUS)
      renderAdditionalFrames = ImGui::GetFrameCount ( ) + 3; // If we received some event changes
    else if (input)
      renderAdditionalFrames = ImGui::GetFrameCount ( ) + 3; // If we received any gamepad input or an input is held down
    else if (svcTransitionFromPendingState)
      renderAdditionalFrames = ImGui::GetFrameCount ( ) + 3; // If we transitioned away from a pending service state
    else if (1.0f > ImGui::GetCurrentContext()->DimBgRatio && ImGui::GetCurrentContext()->DimBgRatio > 0.0f)
      renderAdditionalFrames = ImGui::GetFrameCount ( ) + 3; // If the background is currently currently undergoing a fade effect
    else if (SKIF_Tab_Selected == UITab_Library && (startupFadeIn == 1 || coverFadeActive))
      renderAdditionalFrames = ImGui::GetFrameCount ( ) + 3; // If the cover is currently undergoing a fade effect
    else if (addAdditionalFrames > 0)
      renderAdditionalFrames = ImGui::GetFrameCount ( ) + addAdditionalFrames; // Used when the cover is currently loading in, or the update check just completed
    /*
    else if (  AddGamePopup == PopupState_Open ||
               ConfirmPopup == PopupState_Open ||
            ModifyGamePopup == PopupState_Open ||
          UpdatePromptPopup == PopupState_Open ||
               HistoryPopup == PopupState_Open )
      renderAdditionalFrames = ImGui::GetFrameCount ( ) + 3; // If a popup is transitioning to an opened state
    */
    else if (ImGui::GetFrameCount ( ) > renderAdditionalFrames)
      renderAdditionalFrames = 0;

    addAdditionalFrames = 0;

    //OutputDebugString((L"Framerate: " + std::to_wstring(ImGui::GetIO().Framerate) + L"\n").c_str());

    // Clear gamepad/nav input for the next frame as we're done with it
    memset (ImGui::GetIO ( ).NavInputs, 0, sizeof(ImGui::GetIO ( ).NavInputs));

    //if (uiLastMsg == WM_SKIF_GAMEPAD)
    //  OutputDebugString(L"[doWhile] Message spotted: WM_SKIF_GAMEPAD\n");
    //else if (uiLastMsg == WM_SKIF_COVER)
    //  OutputDebugString(L"[doWhile] Message spotted: WM_SKIF_COVER\n");
    //else if (uiLastMsg != 0x0)
    //  OutputDebugString((L"[doWhile] Message spotted: " + std::to_wstring(uiLastMsg) + L"\n").c_str());
    
    // Pause if we don't need to render any additional frames
    if (renderAdditionalFrames == 0)
      pause = true;
    // Don't pause if there's hidden frames that needs rendering
    if (HiddenFramesContinueRendering)
      pause = false;

    bool frameRateUnlocked = static_cast<DWORD>(ImGui::GetIO().Framerate) > (1000 / (dwDwmPeriod));
    //OutputDebugString((L"Frame rate unlocked: " + std::to_wstring(frameRateUnlocked) + L"\n").c_str());

    do
    {
      // Pause rendering
      if (pause)
      {
        //OutputDebugString((L"[" + SKIF_Util_timeGetTimeAsWStr() + L"][#" + std::to_wstring(ImGui::GetFrameCount()) + L"][PAUSE] Rendering paused!\n").c_str());

        // SetProcessInformation (Windows 8+)
        using SetProcessInformation_pfn =
          BOOL (WINAPI *)(HANDLE, PROCESS_INFORMATION_CLASS, LPVOID, DWORD);

        static SetProcessInformation_pfn
          SKIF_SetProcessInformation =
              (SetProcessInformation_pfn)GetProcAddress (LoadLibraryEx (L"kernel32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32),
              "SetProcessInformation");

        static PROCESS_POWER_THROTTLING_STATE PowerThrottling = {};
        PowerThrottling.Version     = PROCESS_POWER_THROTTLING_CURRENT_VERSION;

        // Enable Efficiency Mode in Windows (requires idle priority + EcoQoS)
        SetPriorityClass (GetCurrentProcess(), IDLE_PRIORITY_CLASS);
        if (SKIF_SetProcessInformation != nullptr)
        {
          PowerThrottling.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
          PowerThrottling.StateMask   = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
          SKIF_SetProcessInformation (GetCurrentProcess(), ProcessPowerThrottling, &PowerThrottling, sizeof(PowerThrottling));
        }

        //OutputDebugString ((L"vWatchHandles[SKIF_Tab_Selected].second.size(): " + std::to_wstring(vWatchHandles[SKIF_Tab_Selected].second.size()) + L"\n").c_str());
        
        // Sleep until a message is in the queue or a change notification occurs
        static bool bWaitTimeoutMsgInputFallback = false;
        if (WAIT_FAILED == MsgWaitForMultipleObjects (static_cast<DWORD>(vWatchHandles[SKIF_Tab_Selected].second.size()), vWatchHandles[SKIF_Tab_Selected].second.data(), false, bWaitTimeoutMsgInputFallback ? dwDwmPeriod : INFINITE, QS_ALLINPUT))
        {
          SK_RunOnce (
          {
            PLOG_ERROR << "Waiting on a new message or change notification failed with error message: " << SKIF_Util_GetErrorAsWStr ( );
            PLOG_ERROR << "Timeout has permanently been set to the monitors refresh rate period (" << dwDwmPeriod << ") !";
            bWaitTimeoutMsgInputFallback = true;
          });
        }

        // Wake up and disable idle priority + ECO QoS (let the system take over)
        SetPriorityClass (GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
        if (SKIF_SetProcessInformation != nullptr)
        {
          PowerThrottling.ControlMask = 0;
          PowerThrottling.StateMask   = 0;
          SKIF_SetProcessInformation (GetCurrentProcess (), ProcessPowerThrottling, &PowerThrottling, sizeof (PowerThrottling));
        }

        // Always render 3 additional frames after we wake up
        renderAdditionalFrames = ImGui::GetFrameCount() + 3;
        
        //OutputDebugString((L"[" + SKIF_Util_timeGetTimeAsWStr() + L"][#" + std::to_wstring(ImGui::GetFrameCount()) + L"][AWAKE] Woken up again!\n").c_str());
      }
      
      // The below is required as a fallback if V-Sync OFF is forced on SKIF and e.g. analog stick drift is causing constant input.
      else if (frameRateUnlocked && input) // Throttle to monitors refresh rate unless a new event is triggered, or user input is posted, but only if the frame rate is detected as being unlocked
        MsgWaitForMultipleObjects (static_cast<DWORD>(vWatchHandles[SKIF_Tab_Selected].second.size()), vWatchHandles[SKIF_Tab_Selected].second.data(), false, dwDwmPeriod, QS_ALLINPUT);

      
      if (bRefresh)
      {
        //auto timePre = SKIF_Util_timeGetTime();

        if (! msgDontRedraw && ! vSwapchainWaitHandles.empty())
        {
          static bool bWaitTimeoutSwapChainsFallback = false;

          DWORD res = WaitForMultipleObjectsEx (static_cast<DWORD>(vSwapchainWaitHandles.size()), vSwapchainWaitHandles.data(), true, bWaitTimeoutSwapChainsFallback ? dwDwmPeriod : 1000, true);

          //OutputDebugString((L"[" + SKIF_Util_timeGetTimeAsWStr() + L"][#" + std::to_wstring(ImGui::GetFrameCount()) + L"] Maybe we'll be waiting? (handles: " + std::to_wstring(vSwapchainWaitHandles.size()) + L")\n").c_str());
          if (res == WAIT_TIMEOUT)
          {
            // This is only expected to occur when an issue arises
            // e.g. the display driver resets and invalidates the
            // swapchain in the middle of a frame.
            PLOG_ERROR << "Timed out while waiting on the swapchain wait objects!";
          }
          else if (res == WAIT_FAILED)
          {
            SK_RunOnce (
            {
              PLOG_ERROR << "Waiting on the swapchain wait objects failed with error message: " << SKIF_Util_GetErrorAsWStr ( );
              PLOG_ERROR << "Timeout has permanently been set to the monitors refresh rate period (" << dwDwmPeriod << ") !";
              bWaitTimeoutSwapChainsFallback = true;
            });
          }
        }
        // Only reason we use a timeout here is in case a swapchain gets destroyed on the same frame we try waiting on its handle

        //auto timePost = SKIF_Util_timeGetTime();
        //auto timeDiff = timePost - timePre;
        //PLOG_VERBOSE << "Waited: " << timeDiff << " ms (handles : " << vSwapchainWaitHandles.size() << ")";
        //OutputDebugString((L"Waited: " + std::to_wstring(timeDiff) + L" ms (handles: " + std::to_wstring(vSwapchainWaitHandles.size()) + L")\n").c_str());
      }
      
      // Reset stuff that's set as part of pumping the message queue
      msgDontRedraw = false;
      uiLastMsg     = 0x0;

      // Pump the message queue, and break if we receive a false (WM_QUIT or WM_QUERYENDSESSION)
      if (! _TranslateAndDispatch ( ))
        break;
      
      // If we added more frames, ensure we exit the loop
      if (addAdditionalFrames > 0)
        msgDontRedraw = false;

      // Break if SKIF is no longer a window
      if (! IsWindow (hWnd))
        break;

    } while (! SKIF_Shutdown && msgDontRedraw); // For messages we don't want to redraw on, we set msgDontRedraw to true.
  }
  
  if (! _registry.bLastSelectedWritten)
  {
    _registry.regKVLastSelectedGame.putData  (_registry.iLastSelectedGame);
    _registry.regKVLastSelectedStore.putData (_registry.wsLastSelectedStore);
    _registry.bLastSelectedWritten = true;
    PLOG_INFO << "Wrote the last selected game to registry: " << _registry.iLastSelectedGame << " (" << _registry.wsLastSelectedStore << ")";
  }

  SKIF_Util_UnregisterHDRToggleHotKey ( );

  PLOG_INFO << "Killing timers...";
  KillTimer (SKIF_hWnd, _inject.IDT_REFRESH_INJECTACK);
  KillTimer (SKIF_hWnd, _inject.IDT_REFRESH_PENDING);
  KillTimer (SKIF_hWnd, IDT_REFRESH_TOOLTIP);
  KillTimer (SKIF_hWnd, IDT_REFRESH_UPDATER);
  KillTimer (SKIF_hWnd, IDT_REFRESH_GAMES);

  PLOG_INFO << "Shutting down ImGui...";
  ImGui_ImplDX11_Shutdown   (    );
  ImGui_ImplWin32_Shutdown  (    );

  CleanupDeviceD3D          (    );

  PLOG_INFO << "Destroying notification icon...";
  Shell_NotifyIcon          (NIM_DELETE, &niData);
  DeleteObject              (niData.hIcon);
  niData.hIcon               = 0;
  DestroyWindow             (SKIF_Notify_hWnd);

  PLOG_INFO << "Destroying main window...";
  //if (hDC != 0)
  //  ReleaseDC               (hWnd, hDC);
  DestroyWindow             (hWnd);

  PLOG_INFO << "Destroying ImGui context...";
  ImGui::DestroyContext     (    );

  SKIF_Notify_hWnd = 0;
  SKIF_hWnd = 0;
       hWnd = 0;

  PLOG_INFO << "Terminating process...";
  return 0;
}


// Helper functions

// D3D9 test stuff
//#define SKIF_D3D9_TEST
#ifdef SKIF_D3D9_TEST
#pragma comment (lib, "d3d9.lib")
#define D3D_DEBUG_INFO
#include <d3d9.h>
#endif

bool CreateDeviceD3D (HWND hWnd)
{
  static SKIF_RegistrySettings& _registry = SKIF_RegistrySettings::GetInstance ( );

#ifdef SKIF_D3D9_TEST
  /* Test D3D9 debugging */
  IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
  if (d3d == nullptr)
  {
    OutputDebugString(L"Direct3DCreate9() failed!\n");
  } else {
    OutputDebugString(L"Direct3DCreate9() succeeded!?\n");
  }

  D3DPRESENT_PARAMETERS pp = {};
  pp.BackBufferWidth = 800;
  pp.BackBufferHeight = 600;
  pp.BackBufferFormat = D3DFMT_X8R8G8B8;
  pp.BackBufferCount = 1;
  pp.MultiSampleType = D3DMULTISAMPLE_NONE;
  pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
  pp.hDeviceWindow = NULL;
  pp.Windowed = TRUE;
  pp.EnableAutoDepthStencil = TRUE;
  pp.AutoDepthStencilFormat = D3DFMT_D16;

  IDirect3DDevice9* device = nullptr;
  // Intentionally passing an invalid parameter to CreateDevice to cause an exception to be thrown by the D3D9 debug layer
  //HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &device);
  HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, (D3DDEVTYPE)100, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &device);
  if (FAILED(hr))
  {
    OutputDebugString(L"d3d->CreateDevice() failed!\n");
  } else {
    OutputDebugString(L"d3d->CreateDevice() succeeded!?\n");
  }
#endif
  
  CComPtr <IDXGIFactory2> pFactory2;

  if (S_OK != CreateDXGIFactory1 (__uuidof (IDXGIFactory2), (void **)&pFactory2.p))
    return false;

  // Windows 7 (with the Platform Update) and newer
  SKIF_bCanFlip                 =         true; // Should never be set to false here

  if (SKIF_bCanFlip)
  {
    // Windows 8.1+
    SKIF_bCanWaitSwapchain      =
      SKIF_Util_IsWindows8Point1OrGreater ();

    // Windows 10+
    SKIF_bCanFlipDiscard        =
      SKIF_Util_IsWindows10OrGreater      ();

    // Windows 10 1709+ (Build 16299)
    SKIF_bCanHDR                =
      SKIF_Util_IsWindows10v1709OrGreater (    ) &&
      SKIF_Util_IsHDRActive               (true);

    CComQIPtr <IDXGIFactory5>
                   pFactory5 (pFactory2.p);

    // Windows 10+
    if (pFactory5 != nullptr)
    {
      BOOL supportsTearing = FALSE;
      pFactory5->CheckFeatureSupport (
                            DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                                          &supportsTearing,
                                  sizeof  (supportsTearing)
                                                );
      SKIF_bCanAllowTearing = (supportsTearing != FALSE);
    }
  }

  // Overrides
  //SKIF_bCanAllowTearing       = false; // Allow Tearing
  //SKIF_bCanFlipDiscard        = false; // Flip Discard
  //SKIF_bCanFlip               = false; // Flip Sequential (if this is false, BitBlt Discard will be used instead)
  //SKIF_bCanWaitSwapchain      = false; // Waitable Swapchain

  // D3D11Device
  D3D_FEATURE_LEVEL featureLevel;
  const D3D_FEATURE_LEVEL
                    featureLevelArray [4] = {
    D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
    D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0
  };

  UINT createDeviceFlags = 0;
  // This MUST be disabled before public release! Otherwise systems without the Windows SDK installed will crash on launch.
  //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG; // Enable debug layer of D3D11

  if (S_OK != D3D11CreateDevice ( nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                              createDeviceFlags, featureLevelArray,
                                                         sizeof (featureLevelArray) / sizeof featureLevel,
                                                D3D11_SDK_VERSION,
                                                       &g_pd3dDevice,
                                                                &featureLevel,
                                                       &g_pd3dDeviceContext))
  {
    //OutputDebugString(L"D3D11CreateDevice failed!\n");
    PLOG_ERROR << "D3D11CreateDevice failed!";
    return false;
  }

  // We need to try creating a dummy swapchain before we actually start creating
  //   viewport windows. This is to ensure a compatible format is used from the
  //   get go, as e.g. using WS_EX_NOREDIRECTIONBITMAP on a BitBlt window will
  //   cause it to be hidden entirely.

  if (pFactory2 != nullptr)
  {
    CComQIPtr <IDXGISwapChain1>
                   pSwapChain1;

    DXGI_FORMAT dxgi_format;

    // HDR formats
    if (SKIF_bCanHDR && _registry.iHDRMode > 0)
    {
      if      (_registry.iHDRMode == 2)
        dxgi_format = DXGI_FORMAT_R16G16B16A16_FLOAT; // scRGB (16 bpc)
      else
        dxgi_format = DXGI_FORMAT_R10G10B10A2_UNORM;  // HDR10 (10 bpc)
    }

    // SDR formats
    else {
      if      (_registry.iSDRMode == 2)
        dxgi_format = DXGI_FORMAT_R16G16B16A16_FLOAT; // 16 bpc
      else if (_registry.iSDRMode == 1 && SKIF_Util_IsWindowsVersionOrGreater (10, 0, 16299))
        dxgi_format = DXGI_FORMAT_R10G10B10A2_UNORM;  // 10 bpc (apparently only supported for flip on Win10 1709+
      else
        dxgi_format = DXGI_FORMAT_R8G8B8A8_UNORM;     // 8 bpc;
    }

    // Create a dummy swapchain for the dummy viewport
    DXGI_SWAP_CHAIN_DESC1
      swap_desc                  = { };
    swap_desc.Width              = 8;
    swap_desc.Height             = 8;
    swap_desc.Format             = dxgi_format;
    swap_desc.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_desc.Flags              = 0x0;
    swap_desc.SampleDesc.Count   = 1;
    swap_desc.SampleDesc.Quality = 0;

    // Assume flip by default
    swap_desc.BufferCount  = 3; // Must be 2-16 for flip model

    if (SKIF_bCanWaitSwapchain)
      swap_desc.Flags     |= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

    if (SKIF_bCanAllowTearing)
      swap_desc.Flags     |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    for (auto  _swapEffect : {DXGI_SWAP_EFFECT_FLIP_DISCARD, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, DXGI_SWAP_EFFECT_DISCARD})
    {
      swap_desc.SwapEffect = _swapEffect;

      // In case flip failed, fall back to using BitBlt
      if (_swapEffect == DXGI_SWAP_EFFECT_DISCARD)
      {
        swap_desc.Format       = DXGI_FORMAT_R8G8B8A8_UNORM;
        swap_desc.BufferCount  = 1;
        swap_desc.Flags        = 0x0;
        SKIF_bCanFlipDiscard   = false;
        SKIF_bCanFlip          = false;
        SKIF_bCanHDR           = false;
        SKIF_bCanWaitSwapchain = false;
        SKIF_bCanAllowTearing  = false;
      }

      if (SUCCEEDED (pFactory2->CreateSwapChainForHwnd ( g_pd3dDevice, hWnd, &swap_desc, NULL, NULL,
                                &pSwapChain1 ))) break;
    }

    pSwapChain1.Release();
    pFactory2.Release();

    return true;
  }

  //CreateRenderTarget ();

  return false;
}

void CleanupDeviceD3D (void)
{
  //CleanupRenderTarget ();

  //IUnknown_AtomicRelease ((void **)&g_pSwapChain);
  IUnknown_AtomicRelease ((void **)&g_pd3dDeviceContext);
  IUnknown_AtomicRelease ((void **)&g_pd3dDevice);
}

// Prevent race conditions between asset loading and device init
//
void SKIF_WaitForDeviceInitD3D (void)
{
  while ( g_pd3dDevice        == nullptr    ||
          g_pd3dDeviceContext == nullptr /* ||
          g_pSwapChain        == nullptr  */ )
  {
    Sleep (10UL);
  }
}

CComPtr <ID3D11Device>
SKIF_D3D11_GetDevice (bool bWait)
{
  if (bWait)
    SKIF_WaitForDeviceInitD3D ();

  return
    g_pd3dDevice;
}

/*
void CreateRenderTarget (void)
{
  ID3D11Texture2D*                           pBackBuffer = nullptr;
  g_pSwapChain->GetBuffer (0, IID_PPV_ARGS (&pBackBuffer));

  if (pBackBuffer != nullptr)
  {
    g_pd3dDevice->CreateRenderTargetView   ( pBackBuffer, nullptr, &g_mainRenderTargetView);
                                             pBackBuffer->Release ();
  }
}

void CleanupRenderTarget (void)
{
  IUnknown_AtomicRelease ((void **)&g_mainRenderTargetView);
}
*/

// Win32 message handler
extern LRESULT
ImGui_ImplWin32_WndProcHandler (HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT
WINAPI
SKIF_WndProc (HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  // This is the message procedure for the (invisible) 0x0 SKIF window
  
  //OutputDebugString((L"[SKIF_WndProc] Message spotted: 0x" + std::format(L"{:x}", msg)    + L" (" + std::to_wstring(msg)    + L")" + (msg == WM_SETFOCUS ? L" == WM_SETFOCUS" : msg == WM_KILLFOCUS ? L" == WM_KILLFOCUS" : L"") + L"\n").c_str());
  //OutputDebugString((L"[SKIF_WndProc]          wParam: 0x" + std::format(L"{:x}", wParam) + L" (" + std::to_wstring(wParam) + L")" + ((HWND)wParam == NULL ? L" == NULL" : (HWND)wParam == SKIF_hWnd ? L" == SKIF_hWnd" : (HWND)wParam == SKIF_ImGui_hWnd ? L" == SKIF_ImGui_hWnd" : (HWND)wParam == SKIF_Notify_hWnd ? L" == SKIF_Notify_hWnd" : L"") + L"\n").c_str());
  //OutputDebugString((L"[SKIF_WndProc]          wParam: 0x" + std::format(L"{:x}", wParam) + L" (" + std::to_wstring(wParam) + L")\n").c_str());
  //OutputDebugString((L"[SKIF_WndProc]          lParam: 0x" + std::format(L"{:x}", lParam) + L" (" + std::to_wstring(lParam) + L")\n").c_str());

  /*
  if (msg != WM_NULL        && 
      msg != WM_NCHITTEST   &&
      msg != WM_MOUSEFIRST  &&
      msg != WM_MOUSEMOVE
    )
  {
    OutputDebugString((L"[WndProc] Message spotted: " + std::to_wstring(msg) + L" + wParam: " + std::to_wstring(wParam) + L"\n").c_str());
  }

  if (ImGui::GetCurrentContext() != NULL)
  {
    OutputDebugString((L"[WndProc][" + SKIF_Util_timeGetTimeAsWStr() + L"][#" + std::to_wstring(ImGui::GetFrameCount()) + L"] Message spotted : " + std::to_wstring(msg) + L" w wParam : " + std::to_wstring(wParam) + L"\n").c_str());
  }
  */

  if (ImGui_ImplWin32_WndProcHandler (hWnd, msg, wParam, lParam))
    return true;
  
  UpdateFlags uFlags = UpdateFlags_Unknown;

  static SKIF_RegistrySettings& _registry = SKIF_RegistrySettings::GetInstance ( );
  static SKIF_InjectionContext& _inject   = SKIF_InjectionContext::GetInstance ( );

  switch (msg)
  {
    case WM_HOTKEY:
      if (wParam == SKIF_HotKey_HDR)
        SKIF_Util_EnableHDROutput ( );
    break;

    case WM_QUIT:
      SKIF_Shutdown = true;
      break;

    case WM_ENDSESSION: 
      // Session is shutting down -- perform any last minute changes!
      if (wParam == 1)
      {
        PLOG_INFO << "Received system shutdown signal!";

        if (! _registry.bLastSelectedWritten)
        {
          _registry.regKVLastSelectedGame.putData  (_registry.iLastSelectedGame);
          _registry.regKVLastSelectedStore.putData (_registry.wsLastSelectedStore);
          _registry.bLastSelectedWritten = true;
          PLOG_INFO << "Wrote the last selected game to registry: " << _registry.iLastSelectedGame << " (" << _registry.wsLastSelectedStore << ")";
        }

        SKIF_Shutdown = true;
      }
      return 0;
      break;

    case WM_QUERYENDSESSION: // System wants to shut down and is asking if we can allow it
      //SKIF_Shutdown = true;
      PLOG_INFO << "System in querying if we can shut down!";
      return true;
      break;

    case WM_GETICON: // Work around bug in Task Manager sending this message every time it refreshes its process list
      msgDontRedraw = true;
      break;

    case WM_DISPLAYCHANGE:
      if (SKIF_Tab_Selected == UITab_Settings)
        RefreshSettingsTab = true; // Only set this if the Settings tab is actually selected
      break;

    case WM_SKIF_MINIMIZE:
      if (_registry.bCloseToTray && ! SKIF_isTrayed)
      {
        ShowWindow       (hWnd, SW_MINIMIZE);
        ShowWindow       (hWnd, SW_HIDE);
        UpdateWindow     (hWnd);
        SKIF_isTrayed    = true;
      }

      else if (! _registry.bCloseToTray) {
        ShowWindowAsync (hWnd, SW_MINIMIZE);
      }
      break;

    case WM_SKIF_START:
      if (_inject.runState != SKIF_InjectionContext::RunningState::Started)
        _inject._StartStopInject (false);
      else if (_inject.runState == SKIF_InjectionContext::RunningState::Started)
        _inject._ToggleInjectAck (false);
      break;

    case WM_SKIF_TEMPSTART:
    case WM_SKIF_TEMPSTARTEXIT:
      if (_inject.runState != SKIF_InjectionContext::RunningState::Started)
        _inject._StartStopInject (false, true);
      else if (_inject.runState == SKIF_InjectionContext::RunningState::Started)
        _inject._ToggleInjectAck (true);

      if (msg == WM_SKIF_TEMPSTARTEXIT)
        bExitOnInjection = true;
      break;

    case WM_SKIF_STOP:
      _inject._StartStopInject   (true);
      break;

    case WM_SKIF_REFRESHGAMES: // TODO: Contemplate this design, and its position in the new design with situational pausing. Concerns WM_SKIF_REFRESHGAMES / IDT_REFRESH_GAMES.
      RepopulateGamesWasSet = SKIF_Util_timeGetTime();
      RepopulateGames = true;
      SelectNewSKIFGame = (uint32_t)wParam;

      SetTimer (SKIF_hWnd,
          IDT_REFRESH_GAMES,
          50,
          (TIMERPROC) NULL
      );
      break;

    case WM_SKIF_LAUNCHER:
      if (_inject.runState != SKIF_InjectionContext::RunningState::Started)
        _inject._StartStopInject (false, true);

      // Reload the whitelist as it might have been changed
      _inject._LoadList          (true);
      break;

    case WM_SKIF_POWERMODE:
      if (_registry.bSmallMode)
        msgDontRedraw = true;
      break;

    case WM_SKIF_COVER:
      addAdditionalFrames += 3;
      break;

    case WM_SKIF_UPDATER:
      SKIF_Updater::GetInstance ( ).RefreshResults ( ); // Swap in the new results

      uFlags = (UpdateFlags)wParam;

      if (uFlags != UpdateFlags_Unknown)
      {
        // Only show the update prompt if we have a file downloaded and we either
        // forced it (switched channel) or it is not ignored nor an older version
        if ( (uFlags & UpdateFlags_Downloaded) == UpdateFlags_Downloaded &&
            ((uFlags & UpdateFlags_Forced)     == UpdateFlags_Forced     ||
            ((uFlags & UpdateFlags_Ignored)    != UpdateFlags_Ignored    &&
             (uFlags & UpdateFlags_Rollback)   != UpdateFlags_Rollback   )))
        {
          UpdatePromptPopup = PopupState_Open;
          addAdditionalFrames += 3;
        }
      }
      break;

    case WM_SKIF_RESTORE:
      _inject.bTaskbarOverlayIcon = false;

      if (! SKIF_isTrayed && ! IsIconic (hWnd))
        RepositionSKIF            = true;

      if (SKIF_isTrayed)
      {   SKIF_isTrayed           = false;
        ShowWindowAsync   (hWnd, SW_SHOW);
      }

      ShowWindowAsync     (hWnd, SW_RESTORE);

      if (! UpdateWindow        (hWnd))
        PLOG_DEBUG << "UpdateWindow ( ) failed!";

      if (! SetForegroundWindow (hWnd))
        PLOG_DEBUG << "SetForegroundWindow ( ) failed!";

      if (! SetActiveWindow     (hWnd))
        PLOG_DEBUG << "SetActiveWindow ( ) failed: "  << SKIF_Util_GetErrorAsWStr ( );

      if (! BringWindowToTop    (hWnd))
        PLOG_DEBUG << "BringWindowToTop ( ) failed: " << SKIF_Util_GetErrorAsWStr ( );
      break;

    case WM_TIMER:
      msgDontRedraw = false;
      switch (wParam)
      {
        case IDT_REFRESH_UPDATER:
          return 0;
        case IDT_REFRESH_TOOLTIP:
          // Do not redraw if SKIF is not being hovered by the mouse or a hover tip is not longer "active" any longer
          if (! SKIF_ImGui_IsMouseHovered ( ) || ! HoverTipActive)
            msgDontRedraw = true;
          
          KillTimer (SKIF_hWnd, IDT_REFRESH_TOOLTIP);
          return 0;
        case IDT_REFRESH_GAMES: // TODO: Contemplate this design, and its position in the new design with situational pausing. Concerns WM_SKIF_REFRESHGAMES / IDT_REFRESH_GAMES.
          if (RepopulateGamesWasSet != 0 && RepopulateGamesWasSet + 1000 < SKIF_Util_timeGetTime())
          {
            RepopulateGamesWasSet = 0;
            KillTimer (SKIF_hWnd, IDT_REFRESH_GAMES);
          }
          return 0;
        case cIDT_REFRESH_INJECTACK:
        case cIDT_REFRESH_PENDING:
          // These are just dummy events to get SKIF to refresh for a couple of frames more periodically
          return 0;
      }
      break;

    case WM_SIZE:
      // Might as well trigger a recreation on WM_SIZE when not minimized
      // It is possible this catches device reset/hung scenarios
      if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED)
        RecreateSwapChains = true;
      return 0;

   /* 2023-04-29: Disabled as this was never used and only referenced the unused empty "parent swapchain" that was never presented
    * HDR toggles are handled through ImGui_ImplDX11_NewFrame() and pFactory1->IsCurrent () */
#if 0
        UINT swap_flags = 0x0;

        if (SKIF_bCanFlip)
        {
          if (SKIF_bCanWaitSwapchain) // Note: IDXGISwapChain::ResizeBuffers can't be used to add or remove this flag. 
            swap_flags |= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

          if (SKIF_bCanAllowTearing)     //  Note: IDXGISwapChain::ResizeBuffers can't be used to add or remove this flag.
            swap_flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
        }

        CleanupRenderTarget ();
        g_pSwapChain->ResizeBuffers (
          0, (UINT)LOWORD (lParam),
             (UINT)HIWORD (lParam),
            DXGI_FORMAT_UNKNOWN,
            swap_flags
        );
        CreateRenderTarget ();
      }
      return 0;
#endif

    case WM_SYSCOMMAND:
      if ((wParam & 0xfff0) == SC_KEYMENU)
      {
        // Disable ALT application menu
        if ( lParam == 0x00 ||
             lParam == 0x20 )
        {
          return 0;
        }
      }

      else if ((wParam & 0xfff0) == SC_MOVE)
      {
        // Disables the native move modal loop of Windows and
        // use the RepositionSKIF approach to move the window
        // to the center of the display the cursor is on.
        PostMessage (hWnd, WM_SKIF_RESTORE, 0x0, 0x0);
        return 0;
      }
      break;

    case WM_CLOSE:
      // Already handled in ImGui_ImplWin32_WndProcHandler
      break;

    case WM_DESTROY:
      ::PostQuitMessage (0);
      return 0;
  }

  return
    ::DefWindowProc (hWnd, msg, wParam, lParam);
}

LRESULT
WINAPI
SKIF_Notify_WndProc (HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  // This is the message procedure for the notification icon
  
  //OutputDebugString((L"[SKIF_Notify_WndProc] Message spotted: 0x" + std::format(L"{:x}", msg)    + L" (" + std::to_wstring(msg)    + L")" + (msg == WM_SETFOCUS ? L" == WM_SETFOCUS" : msg == WM_KILLFOCUS ? L" == WM_KILLFOCUS" : L"") + L"\n").c_str());
  //OutputDebugString((L"[SKIF_Notify_WndProc]          wParam: 0x" + std::format(L"{:x}", wParam) + L" (" + std::to_wstring(wParam) + L")" + ((HWND)wParam == NULL ? L" == NULL" : (HWND)wParam == SKIF_hWnd ? L" == SKIF_hWnd" : (HWND)wParam == SKIF_ImGui_hWnd ? L" == SKIF_ImGui_hWnd" : (HWND)wParam == SKIF_Notify_hWnd ? L" == SKIF_Notify_hWnd" : L"") + L"\n").c_str());
  //OutputDebugString((L"[SKIF_Notify_WndProc] Message spotted: 0x" + std::format(L"{:x}", msg)    + L" (" + std::to_wstring(msg)    + L")\n").c_str());
  //OutputDebugString((L"[SKIF_Notify_WndProc]          wParam: 0x" + std::format(L"{:x}", wParam) + L" (" + std::to_wstring(wParam) + L")\n").c_str());
  //OutputDebugString((L"[SKIF_Notify_WndProc]          wParam: 0x" + std::format(L"{:x}", wParam) + L" (" + std::to_wstring(wParam) + L") " + ((HWND)wParam == SKIF_hWnd ? L"== SKIF_hWnd" : ((HWND)wParam == SKIF_ImGui_hWnd ? L"== SKIF_ImGui_hWnd" : (HWND)wParam == SKIF_Notify_hWnd ? L"== SKIF_Notify_hWnd" : L"")) + L"\n").c_str());
  //OutputDebugString((L"[SKIF_Notify_WndProc]          lParam: 0x" + std::format(L"{:x}", lParam) + L" (" + std::to_wstring(lParam) + L")\n").c_str());

  switch (msg)
  {
    case WM_SKIF_NOTIFY_ICON:
      msgDontRedraw = true; // Don't redraw the main window when we're interacting with the notification icon
      switch (lParam)
      {
        case WM_LBUTTONDBLCLK:
        case WM_LBUTTONUP:
          PostMessage (SKIF_hWnd, WM_SKIF_RESTORE, 0x0, 0x0);
          return 0;
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
          // Get current mouse position.
          POINT curPoint;
          GetCursorPos (&curPoint);

          // To display a context menu for a notification icon, the current window must be the foreground window
          // before the application calls TrackPopupMenu or TrackPopupMenuEx. Otherwise, the menu will not disappear
          // when the user clicks outside of the menu or the window that created the menu (if it is visible).
          SetForegroundWindow (hWnd);

          // TrackPopupMenu blocks the app until TrackPopupMenu returns
          TrackPopupMenu (
            hMenu,
            TPM_RIGHTBUTTON,
            curPoint.x,
            curPoint.y,
            0,
            hWnd,
            NULL
          );

          // However, when the current window is the foreground window, the second time this menu is displayed,
          // it appears and then immediately disappears. To correct this, you must force a task switch to the
          // application that called TrackPopupMenu. This is done by posting a benign message to the window or
          // thread, as shown in the following code sample:
          PostMessage (hWnd, WM_NULL, 0, 0);
          return 0;
        case NIN_BALLOONHIDE:
        case NIN_BALLOONSHOW:
        case NIN_BALLOONTIMEOUT:
        case NIN_BALLOONUSERCLICK:
        case NIN_POPUPCLOSE:
        case NIN_POPUPOPEN:
          break;
      }
      break;

    case WM_COMMAND:
      switch (LOWORD(wParam))
      {
        case SKIF_NOTIFY_START:
          //PostMessage (SKIF_hWnd, (_registry.bStopOnInjection) ? WM_SKIF_TEMPSTART : WM_SKIF_START, 0, 0);
          PostMessage (SKIF_hWnd, WM_SKIF_START, 0, 0);
          break;
        case SKIF_NOTIFY_STARTWITHSTOP:
          PostMessage (SKIF_hWnd, WM_SKIF_TEMPSTART, 0, 0);
          break;
        case SKIF_NOTIFY_STOP:
          PostMessage (SKIF_hWnd, WM_SKIF_STOP, 0, 0);
          break;
        case SKIF_NOTIFY_EXIT:
          PostMessage (SKIF_hWnd, WM_CLOSE, 0, 0);
          break;
      }
      break;
  }
  return
    ::DefWindowProc (hWnd, msg, wParam, lParam);
}