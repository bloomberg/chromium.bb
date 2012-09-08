// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Implementation of the CommandExecuteImpl class which implements the
// IExecuteCommand and related interfaces for handling ShellExecute based
// launches of the Chrome browser.

#include "win8/delegate_execute/command_execute_impl.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_handle.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "win8/delegate_execute/chrome_util.h"

// CommandExecuteImpl is resposible for activating chrome in Windows 8. The
// flow is complicated and this tries to highlight the important events.
// The current approach is to have a single instance of chrome either
// running in desktop or metro mode. If there is no current instance then
// the desktop shortcut launches desktop chrome and the metro tile or search
// charm launches metro chrome.
// If chrome is running then focus/activation is given to the existing one
// regarless of what launch point the user used.
//
// The general flow when chrome is the default browser is as follows:
//
// 1- User interacts with launch point (icon, tile, search, shellexec, etc)
// 2- Windows finds the appid for launch item and resolves it to chrome
// 3- Windows activates CommandExecuteImpl inside a surrogate process
// 4- Windows calls the following sequence of entry points:
//    CommandExecuteImpl::SetShowWindow
//    CommandExecuteImpl::SetPosition
//    CommandExecuteImpl::SetDirectory
//    CommandExecuteImpl::SetParameter
//    CommandExecuteImpl::SetNoShowUI
//    CommandExecuteImpl::SetSelection
//    CommandExecuteImpl::Initialize
//    Up to this point the code basically just gathers values passed in, like
//    the launch scheme (or url) and the activation verb.
// 5- Windows calls CommandExecuteImpl::Getvalue()
//    Here we need to return AHE_IMMERSIVE or AHE_DESKTOP. That depends on:
//    a) if run in high-integrity return AHE_DESKTOP
//    b) if chrome is running return the AHE_ mode of chrome
//    c) if the current process is inmmersive return AHE_IMMERSIVE
//    d) if the protocol is file and IExecuteCommandHost::GetUIMode() is not
//       ECHUIM_DESKTOP then return AHE_IMMERSIVE
//    e) if none of the above return AHE_DESKTOP
// 6- If we returned AHE_DESKTOP in step 5 then CommandExecuteImpl::Execute()
//    is called, here we call GetLaunchMode() which:
//    a) if chrome is running return the mode of chrome or
//    b) return IExecuteCommandHost::GetUIMode()
// 7- If GetLaunchMode() returns
//    a) ECHUIM_DESKTOP we call LaunchDestopChrome() that calls ::CreateProcess
//    b) else we call one of the IApplicationActivationManager activation
//       functions depending on the parameters passed in step 4.
//
//  Some examples will help clarify the common cases.
//
//  I - No chrome running, taskbar icon launch:
//      a) Scheme is 'file', Verb is 'open'
//      b) GetValue() returns at e) step : AHE_DESKTOP
//      c) Execute() calls LaunchDestopChrome()
//      --> desktop chrome runs
//  II- No chrome running, tile activation launch:
//      a) Scheme is 'file', Verb is 'open'
//      b) GetValue() returns at d) step : AHE_IMMERSIVE
//      c) Windows does not call us back and just activates chrome
//      --> metro chrome runs
//  III- No chrome running, url link click on metro app:
//      a) Scheme is 'http', Verb is 'open'
//      b) Getvalue() returns at e) step : AHE_DESKTOP
//      c) Execute() calls IApplicationActivationManager::ActivateForProtocol
//      --> metro chrome runs
//
CommandExecuteImpl::CommandExecuteImpl()
    : integrity_level_(base::INTEGRITY_UNKNOWN),
      launch_scheme_(INTERNET_SCHEME_DEFAULT),
      chrome_mode_(ECHUIM_SYSTEM_LAUNCHER) {
  memset(&start_info_, 0, sizeof(start_info_));
  start_info_.cb = sizeof(start_info_);
  // We need to query the user data dir of chrome so we need chrome's
  // path provider.
  chrome::RegisterPathProvider();
}

// CommandExecuteImpl
STDMETHODIMP CommandExecuteImpl::SetKeyState(DWORD key_state) {
  AtlTrace("In %hs\n", __FUNCTION__);
  return S_OK;
}

STDMETHODIMP CommandExecuteImpl::SetParameters(LPCWSTR params) {
  AtlTrace("In %hs [%S]\n", __FUNCTION__, params);
  if (params) {
    parameters_ = params;
  }
  return S_OK;
}

STDMETHODIMP CommandExecuteImpl::SetPosition(POINT pt) {
  AtlTrace("In %hs\n", __FUNCTION__);
  return S_OK;
}

STDMETHODIMP CommandExecuteImpl::SetShowWindow(int show) {
  AtlTrace("In %hs  show=%d\n", __FUNCTION__, show);
  start_info_.wShowWindow = show;
  start_info_.dwFlags |= STARTF_USESHOWWINDOW;
  return S_OK;
}

STDMETHODIMP CommandExecuteImpl::SetNoShowUI(BOOL no_show_ui) {
  AtlTrace("In %hs no_show=%d\n", __FUNCTION__, no_show_ui);
  return S_OK;
}

STDMETHODIMP CommandExecuteImpl::SetDirectory(LPCWSTR directory) {
  AtlTrace("In %hs\n", __FUNCTION__);
  return S_OK;
}

STDMETHODIMP CommandExecuteImpl::GetValue(enum AHE_TYPE* pahe) {
  AtlTrace("In %hs\n", __FUNCTION__);

  if (!GetLaunchScheme(&display_name_, &launch_scheme_)) {
    AtlTrace("Failed to get scheme, E_FAIL\n");
    return E_FAIL;
  }

  if (integrity_level_ == base::HIGH_INTEGRITY) {
    // Metro mode apps don't work in high integrity mode.
    AtlTrace("High integrity, AHE_DESKTOP\n");
    *pahe = AHE_DESKTOP;
    return S_OK;
  }

  FilePath user_data_dir;
  if (!PathService::Get(chrome::DIR_USER_DATA, &user_data_dir))  {
    AtlTrace("Failed to get chrome's data dir path, E_FAIL\n");
    return E_FAIL;
  }

  HWND chrome_window = ::FindWindowEx(HWND_MESSAGE, NULL,
                                      chrome::kMessageWindowClass,
                                      user_data_dir.value().c_str());
  if (chrome_window) {
    AtlTrace("Found chrome window %p\n", chrome_window);
    // The failure cases below are deemed to happen due to the inherently racy
    // procedure of going from chrome's window to process handle during which
    // chrome might have exited. Failing here would probably just cause the
    // user to retry at which point we would do the right thing.
    DWORD chrome_pid = 0;
    ::GetWindowThreadProcessId(chrome_window, &chrome_pid);
    if (!chrome_pid) {
      AtlTrace("Failed to get chrome's PID, E_FAIL\n");
      return E_FAIL;
    }
    base::win::ScopedHandle process(
        ::OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, chrome_pid));
    if (!process.IsValid()) {
      AtlTrace("Failed to open chrome's process [%d], E_FAIL\n", chrome_pid);
      return E_FAIL;
    }
    if (IsImmersiveProcess(process.Get())) {
      AtlTrace("Chrome [%d] is inmmersive, AHE_IMMERSIVE\n", chrome_pid);
      chrome_mode_ = ECHUIM_IMMERSIVE;
      *pahe = AHE_IMMERSIVE;
    } else {
      AtlTrace("Chrome [%d] is Desktop, AHE_DESKTOP\n");
      chrome_mode_ = ECHUIM_DESKTOP;
      *pahe = AHE_DESKTOP;
    }
    return S_OK;
  }

  if (IsImmersiveProcess(GetCurrentProcess())) {
    AtlTrace("Current process is inmmersive, AHE_IMMERSIVE\n");
    *pahe = AHE_IMMERSIVE;
    return S_OK;
  }

  if ((launch_scheme_ == INTERNET_SCHEME_FILE) &&
      (GetLaunchMode() != ECHUIM_DESKTOP)) {
    AtlTrace("INTERNET_SCHEME_FILE, mode != ECHUIM_DESKTOP, AHE_IMMERSIVE\n");
    *pahe = AHE_IMMERSIVE;
    return S_OK;
  }

  AtlTrace("Fallback is AHE_DESKTOP\n");
  *pahe = AHE_DESKTOP;
  return S_OK;
}

STDMETHODIMP CommandExecuteImpl::Execute() {
  AtlTrace("In %hs\n", __FUNCTION__);

  if (integrity_level_ == base::HIGH_INTEGRITY)
    return LaunchDesktopChrome();

  EC_HOST_UI_MODE mode = GetLaunchMode();
  if (mode == ECHUIM_DESKTOP)
    return LaunchDesktopChrome();

  HRESULT hr = E_FAIL;
  CComPtr<IApplicationActivationManager> activation_manager;
  hr = activation_manager.CoCreateInstance(CLSID_ApplicationActivationManager);
  if (!activation_manager) {
    AtlTrace("Failed to get the activation manager, error 0x%x\n", hr);
    return S_OK;
  }

  string16 app_id = delegate_execute::GetAppId(chrome_exe_);

  DWORD pid = 0;
  if (launch_scheme_ == INTERNET_SCHEME_FILE) {
    AtlTrace("Activating for file\n");
    hr = activation_manager->ActivateApplication(app_id.c_str(),
                                                 verb_.c_str(),
                                                 AO_NONE,
                                                 &pid);
  } else {
    AtlTrace("Activating for protocol\n");
    hr = activation_manager->ActivateForProtocol(app_id.c_str(),
                                                 item_array_,
                                                 &pid);
  }
  if (hr == E_APPLICATION_NOT_REGISTERED) {
    AtlTrace("Metro chrome is not registered, launching in desktop\n");
    return LaunchDesktopChrome();
  }
  AtlTrace("Metro Chrome launch, pid=%d, returned 0x%x\n", pid, hr);
  return S_OK;
}

STDMETHODIMP CommandExecuteImpl::Initialize(LPCWSTR name,
                                            IPropertyBag* bag) {
  AtlTrace("In %hs\n", __FUNCTION__);
  if (!FindChromeExe(&chrome_exe_))
    return E_FAIL;
  delegate_execute::UpdateChromeIfNeeded(chrome_exe_);
  if (name) {
    AtlTrace("Verb is %S\n", name);
    verb_ = name;
  }

  base::GetProcessIntegrityLevel(base::GetCurrentProcessHandle(),
                                 &integrity_level_);
  if (integrity_level_ == base::HIGH_INTEGRITY) {
    AtlTrace("Delegate execute launched in high integrity level\n");
  }
  return S_OK;
}

STDMETHODIMP CommandExecuteImpl::SetSelection(IShellItemArray* item_array) {
  AtlTrace("In %hs\n", __FUNCTION__);
  item_array_ = item_array;
  return S_OK;
}

STDMETHODIMP CommandExecuteImpl::GetSelection(REFIID riid, void** selection) {
  AtlTrace("In %hs\n", __FUNCTION__);
  return S_OK;
}

STDMETHODIMP CommandExecuteImpl::AllowForegroundTransfer(void* reserved) {
  AtlTrace("In %hs\n", __FUNCTION__);
  return S_OK;
}

// Returns false if chrome.exe cannot be found.
// static
bool CommandExecuteImpl::FindChromeExe(FilePath* chrome_exe) {
  AtlTrace("In %hs\n", __FUNCTION__);
  // Look for chrome.exe one folder above delegate_execute.exe (as expected in
  // Chrome installs). Failing that, look for it alonside delegate_execute.exe.
  FilePath dir_exe;
  if (!PathService::Get(base::DIR_EXE, &dir_exe)) {
    AtlTrace("Failed to get current exe path\n");
    return false;
  }

  *chrome_exe = dir_exe.DirName().Append(chrome::kBrowserProcessExecutableName);
  if (!file_util::PathExists(*chrome_exe)) {
    *chrome_exe = dir_exe.Append(chrome::kBrowserProcessExecutableName);
    if (!file_util::PathExists(*chrome_exe)) {
      AtlTrace("Failed to find chrome exe file\n");
      return false;
    }
  }

  AtlTrace("Got chrome exe path as %ls\n", chrome_exe->value().c_str());
  return true;
}

bool CommandExecuteImpl::GetLaunchScheme(
    string16* display_name, INTERNET_SCHEME* scheme) {
  if (!item_array_)
    return false;

  ATLASSERT(display_name);
  ATLASSERT(scheme);

  DWORD count = 0;
  item_array_->GetCount(&count);

  if (count != 1) {
    AtlTrace("Cannot handle %d elements in the IShellItemArray\n", count);
    return false;
  }

  CComPtr<IEnumShellItems> items;
  item_array_->EnumItems(&items);
  CComPtr<IShellItem> shell_item;
  HRESULT hr = items->Next(1, &shell_item, &count);
  if (hr != S_OK) {
    AtlTrace("Failed to read element from the IShellItemsArray\n");
    return false;
  }

  base::win::ScopedCoMem<wchar_t> name;
  hr = shell_item->GetDisplayName(SIGDN_URL, &name);
  if (hr != S_OK) {
    AtlTrace("Failed to get display name\n");
    return false;
  }

  AtlTrace("Display name is [%ls]\n", name);

  wchar_t scheme_name[16];
  URL_COMPONENTS components = {0};
  components.lpszScheme = scheme_name;
  components.dwSchemeLength = sizeof(scheme_name)/sizeof(scheme_name[0]);

  components.dwStructSize = sizeof(components);
  if (!InternetCrackUrlW(name, 0, 0, &components)) {
    AtlTrace("Failed to crack url %ls\n", name);
    return false;
  }

  AtlTrace("Launch scheme is [%ls] (%d)\n", scheme_name, components.nScheme);
  *display_name = name;
  *scheme = components.nScheme;
  return true;
}

HRESULT CommandExecuteImpl::LaunchDesktopChrome() {
  AtlTrace("In %hs\n", __FUNCTION__);
  string16 display_name = display_name_;

  switch (launch_scheme_) {
    case INTERNET_SCHEME_FILE:
      // If anything other than chrome.exe is passed in the display name we
      // should honor it. For e.g. If the user clicks on a html file when
      // chrome is the default we should treat it as a parameter to be passed
      // to chrome.
      if (display_name.find(L"chrome.exe") != string16::npos)
        display_name.clear();
      break;

    default:
      break;
  }

  string16 command_line = L"\"";
  command_line += chrome_exe_.value();
  command_line += L"\"";

  if (!parameters_.empty()) {
    AtlTrace("Adding parameters %ls to command line\n", parameters_.c_str());
    command_line += L" ";
    command_line += parameters_.c_str();
  }

  if (!display_name.empty()) {
    command_line += L" -- ";
    command_line += display_name;
  }

  AtlTrace("Formatted command line is %ls\n", command_line.c_str());

  PROCESS_INFORMATION proc_info = {0};
  BOOL ret = CreateProcess(NULL, const_cast<LPWSTR>(command_line.c_str()),
                           NULL, NULL, FALSE, 0, NULL, NULL, &start_info_,
                           &proc_info);
  if (ret) {
    AtlTrace("Process id is %d\n", proc_info.dwProcessId);
    AllowSetForegroundWindow(proc_info.dwProcessId);
    CloseHandle(proc_info.hProcess);
    CloseHandle(proc_info.hThread);
  } else {
    AtlTrace("Process launch failed, error %d\n", ::GetLastError());
  }

  return S_OK;
}

EC_HOST_UI_MODE CommandExecuteImpl::GetLaunchMode() {
  // We are to return chrome's mode if chrome exists else we query our embedder
  // IServiceProvider and learn the mode from them.
  static bool launch_mode_determined = false;
  static EC_HOST_UI_MODE launch_mode = ECHUIM_DESKTOP;

  const char* modes[] = { "Desktop", "Inmmersive", "SysLauncher", "??" };

  if (launch_mode_determined)
    return launch_mode;

  if (chrome_mode_ != ECHUIM_SYSTEM_LAUNCHER) {
    launch_mode = chrome_mode_;
    AtlTrace("Launch mode is that of chrome, %s\n", modes[launch_mode]);
    launch_mode_determined = true;
    return launch_mode;
  }

  if (parameters_ == ASCIIToWide(switches::kForceImmersive)) {
    launch_mode = ECHUIM_IMMERSIVE;
    launch_mode_determined = true;
  } else if (parameters_ == ASCIIToWide(switches::kForceDesktop)) {
    launch_mode = ECHUIM_DESKTOP;
    launch_mode_determined = true;
  }

  if (launch_mode_determined) {
    parameters_.clear();
    AtlTrace("Launch mode forced to %s\n", modes[launch_mode]);
    return launch_mode;
  }

  CComPtr<IExecuteCommandHost> host;
  CComQIPtr<IServiceProvider> service_provider = m_spUnkSite;
  if (service_provider) {
    service_provider->QueryService(IID_IExecuteCommandHost, &host);
    if (host) {
      host->GetUIMode(&launch_mode);
    } else {
      AtlTrace("Failed to get IID_IExecuteCommandHost. Assuming desktop\n");
    }
  } else {
      AtlTrace("Failed to get IServiceProvider. Assuming desktop mode\n");
  }
  AtlTrace("Launch mode is %s\n", modes[launch_mode]);
  launch_mode_determined = true;
  return launch_mode;
}
