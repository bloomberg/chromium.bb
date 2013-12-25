// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/daemon_installer_win.h"

#include <windows.h>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/process/launch.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/win/object_watcher.h"
#include "base/win/registry.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_variant.h"
#include "base/win/windows_version.h"
#include "google_update/google_update_idl.h"
#include "remoting/base/dispatch_win.h"
#include "remoting/host/win/omaha.h"

using base::win::ScopedBstr;
using base::win::ScopedComPtr;
using base::win::ScopedVariant;

namespace {

// ProgID of the per-machine Omaha COM server.
const wchar_t kGoogleUpdate[] = L"GoogleUpdate.Update3WebMachine";

// The COM elevation moniker for the per-machine Omaha COM server.
const wchar_t kGoogleUpdateElevationMoniker[] =
    L"Elevation:Administrator!new:GoogleUpdate.Update3WebMachine";

// The registry key where the configuration of Omaha is stored.
const wchar_t kOmahaUpdateKeyName[] = L"Software\\Google\\Update";

// The name of the value where the full path to GoogleUpdate.exe is stored.
const wchar_t kOmahaPathValueName[] = L"path";

// The command line format string for GoogleUpdate.exe
const wchar_t kGoogleUpdateCommandLineFormat[] =
    L"\"%ls\" /install \"bundlename=Chromoting%%20Host&appguid=%ls&"
    L"appname=Chromoting%%20Host&needsadmin=True&lang=%ls\"";

// TODO(alexeypa): Get the desired laungage from the web app.
const wchar_t kOmahaLanguage[] = L"en";

// An empty string for optional parameters.
const wchar_t kOmahaEmpty[] = L"";

// The installation status polling interval.
const int kOmahaPollIntervalMs = 500;

}  // namespace

namespace remoting {

// This class implements on-demand installation of the Chromoting Host via
// per-machine Omaha instance.
class DaemonComInstallerWin : public DaemonInstallerWin {
 public:
  DaemonComInstallerWin(const ScopedComPtr<IDispatch>& update3,
                        const CompletionCallback& done);

  // DaemonInstallerWin implementation.
  virtual void Install() OVERRIDE;

 private:
  // Polls the installation status performing state-specific actions (such as
  // starting installation once download has finished).
  void PollInstallationStatus();

  // Omaha interfaces.
  ScopedVariant app_;
  ScopedVariant bundle_;
  ScopedComPtr<IDispatch> update3_;

  base::Timer polling_timer_;
};

// This class implements on-demand installation of the Chromoting Host by
// launching a per-user instance of Omaha and requesting elevation.
class DaemonCommandLineInstallerWin
    : public DaemonInstallerWin,
      public base::win::ObjectWatcher::Delegate {
 public:
  DaemonCommandLineInstallerWin(const CompletionCallback& done);
  ~DaemonCommandLineInstallerWin();

  // DaemonInstallerWin implementation.
  virtual void Install() OVERRIDE;

  // base::win::ObjectWatcher::Delegate implementation.
  virtual void OnObjectSignaled(HANDLE object) OVERRIDE;

 private:
  // Handle of the launched process.
  base::win::ScopedHandle process_;

  // Used to determine when the launched process terminates.
  base::win::ObjectWatcher process_watcher_;
};

DaemonComInstallerWin::DaemonComInstallerWin(
    const ScopedComPtr<IDispatch>& update3,
    const CompletionCallback& done)
    : DaemonInstallerWin(done),
      update3_(update3),
      polling_timer_(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kOmahaPollIntervalMs),
          base::Bind(&DaemonComInstallerWin::PollInstallationStatus,
                     base::Unretained(this)),
          false) {
}

void DaemonComInstallerWin::Install() {
  // Create an app bundle.
  HRESULT hr = dispatch::Invoke(update3_.get(), L"createAppBundleWeb",
                                DISPATCH_METHOD, bundle_.Receive());
  if (FAILED(hr)) {
    Done(hr);
    return;
  }
  if (bundle_.type() != VT_DISPATCH) {
    Done(DISP_E_TYPEMISMATCH);
    return;
  }

  hr = dispatch::Invoke(V_DISPATCH(&bundle_), L"initialize", DISPATCH_METHOD,
                        NULL);
  if (FAILED(hr)) {
    Done(hr);
    return;
  }

  // Add Chromoting Host to the bundle.
  ScopedVariant appid(kHostOmahaAppid);
  ScopedVariant empty(kOmahaEmpty);
  ScopedVariant language(kOmahaLanguage);
  hr = dispatch::Invoke(V_DISPATCH(&bundle_), L"createApp", DISPATCH_METHOD,
                        appid, empty, language, empty, NULL);
  if (FAILED(hr)) {
    Done(hr);
    return;
  }

  hr = dispatch::Invoke(V_DISPATCH(&bundle_), L"checkForUpdate",
                        DISPATCH_METHOD, NULL);
  if (FAILED(hr)) {
    Done(hr);
    return;
  }

  hr = dispatch::Invoke(V_DISPATCH(&bundle_), L"appWeb",
                        DISPATCH_PROPERTYGET, ScopedVariant(0), app_.Receive());
  if (FAILED(hr)) {
    Done(hr);
    return;
  }
  if (app_.type() != VT_DISPATCH) {
    Done(DISP_E_TYPEMISMATCH);
    return;
  }

  // Now poll for the installation status.
  PollInstallationStatus();
}

void DaemonComInstallerWin::PollInstallationStatus() {
  // Get the current application installation state.
  // N.B. The object underlying the ICurrentState interface has static data that
  // does not get updated as the server state changes. To get the most "current"
  // state, the currentState property needs to be queried again.
  ScopedVariant current_state;
  HRESULT hr = dispatch::Invoke(V_DISPATCH(&app_), L"currentState",
                                DISPATCH_PROPERTYGET, current_state.Receive());
  if (FAILED(hr)) {
    Done(hr);
    return;
  }
  if (current_state.type() != VT_DISPATCH) {
    Done(DISP_E_TYPEMISMATCH);
    return;
  }

  ScopedVariant state;
  hr = dispatch::Invoke(V_DISPATCH(&current_state), L"stateValue",
                        DISPATCH_PROPERTYGET, state.Receive());
  if (state.type() != VT_I4) {
    Done(DISP_E_TYPEMISMATCH);
    return;
  }

  // Perform state-specific actions.
  switch (V_I4(&state)) {
    case STATE_INIT:
    case STATE_WAITING_TO_CHECK_FOR_UPDATE:
    case STATE_CHECKING_FOR_UPDATE:
    case STATE_WAITING_TO_DOWNLOAD:
    case STATE_RETRYING_DOWNLOAD:
    case STATE_DOWNLOADING:
    case STATE_WAITING_TO_INSTALL:
    case STATE_INSTALLING:
    case STATE_PAUSED:
      break;

    case STATE_UPDATE_AVAILABLE:
      hr = dispatch::Invoke(V_DISPATCH(&bundle_), L"download",
                            DISPATCH_METHOD, NULL);
      if (FAILED(hr)) {
        Done(hr);
        return;
      }
      break;

    case STATE_DOWNLOAD_COMPLETE:
    case STATE_EXTRACTING:
    case STATE_APPLYING_DIFFERENTIAL_PATCH:
    case STATE_READY_TO_INSTALL:
      hr = dispatch::Invoke(V_DISPATCH(&bundle_), L"install",
                            DISPATCH_METHOD, NULL);
      if (FAILED(hr)) {
        Done(hr);
        return;
      }
      break;

    case STATE_INSTALL_COMPLETE:
    case STATE_NO_UPDATE:
      // Installation complete or not required. Report success.
      Done(S_OK);
      return;

    case STATE_ERROR: {
      ScopedVariant error_code;
      hr = dispatch::Invoke(V_DISPATCH(&current_state), L"errorCode",
                            DISPATCH_PROPERTYGET, error_code.Receive());
      if (FAILED(hr)) {
        Done(hr);
        return;
      }
      if (error_code.type() != VT_UI4) {
        Done(DISP_E_TYPEMISMATCH);
        return;
      }
      Done(V_UI4(&error_code));
      return;
    }

    default:
      LOG(ERROR) << "Unknown bundle state: " << V_I4(&state) << ".";
      Done(E_FAIL);
      return;
  }

  // Keep polling.
  polling_timer_.Reset();
}

DaemonCommandLineInstallerWin::DaemonCommandLineInstallerWin(
    const CompletionCallback& done) : DaemonInstallerWin(done) {
}

DaemonCommandLineInstallerWin::~DaemonCommandLineInstallerWin() {
  process_watcher_.StopWatching();
}

void DaemonCommandLineInstallerWin::Install() {
  // Get the full path to GoogleUpdate.exe from the registry.
  base::win::RegKey update_key;
  LONG result = update_key.Open(HKEY_CURRENT_USER,
                                kOmahaUpdateKeyName,
                                KEY_READ);
  if (result != ERROR_SUCCESS) {
    Done(HRESULT_FROM_WIN32(result));
    return;
  }

  // presubmit: allow wstring
  std::wstring google_update;
  result = update_key.ReadValue(kOmahaPathValueName, &google_update);
  if (result != ERROR_SUCCESS) {
    Done(HRESULT_FROM_WIN32(result));
    return;
  }

  // Launch the updater process and wait for its termination.
  base::string16 command_line = base::WideToUTF16(
      base::StringPrintf(kGoogleUpdateCommandLineFormat,
                         google_update.c_str(),
                         kHostOmahaAppid,
                         kOmahaLanguage));

  base::LaunchOptions options;
  if (!base::LaunchProcess(command_line, options, &process_)) {
    result = GetLastError();
    Done(HRESULT_FROM_WIN32(result));
    return;
  }

  if (!process_watcher_.StartWatching(process_.Get(), this)) {
    result = GetLastError();
    Done(HRESULT_FROM_WIN32(result));
    return;
  }
}

void DaemonCommandLineInstallerWin::OnObjectSignaled(HANDLE object) {
  // Check if the updater process returned success.
  DWORD exit_code;
  if (GetExitCodeProcess(process_.Get(), &exit_code) && exit_code == 0) {
    Done(S_OK);
  } else {
    Done(E_FAIL);
  }
}

DaemonInstallerWin::DaemonInstallerWin(const CompletionCallback& done)
    : done_(done) {
}

DaemonInstallerWin::~DaemonInstallerWin() {
}

void DaemonInstallerWin::Done(HRESULT result) {
  CompletionCallback done = done_;
  done_.Reset();
  done.Run(result);
}

// static
scoped_ptr<DaemonInstallerWin> DaemonInstallerWin::Create(
    HWND window_handle,
    CompletionCallback done) {
  HRESULT result = E_FAIL;
  ScopedComPtr<IDispatch> update3;

  // Check if the machine instance of Omaha is available. The COM elevation is
  // supported on Vista+, so on XP/W2K3 we assume that we are running under
  // a privileged user and get ACCESS_DENIED later if we are not.
  if (base::win::GetVersion() < base::win::VERSION_VISTA) {
    CLSID class_id;
    result = CLSIDFromProgID(kGoogleUpdate, &class_id);
    if (SUCCEEDED(result)) {
      result = CoCreateInstance(class_id,
                                NULL,
                                CLSCTX_LOCAL_SERVER,
                                IID_IDispatch,
                                update3.ReceiveVoid());
    }
  } else {
    BIND_OPTS3 bind_options;
    memset(&bind_options, 0, sizeof(bind_options));
    bind_options.cbStruct = sizeof(bind_options);
    bind_options.hwnd = GetTopLevelWindow(window_handle);
    bind_options.dwClassContext = CLSCTX_LOCAL_SERVER;
    result = CoGetObject(kGoogleUpdateElevationMoniker,
                         &bind_options,
                         IID_IDispatch,
                         update3.ReceiveVoid());
  }
  if (SUCCEEDED(result)) {
    // The machine instance of Omaha is available and we successfully passed
    // the UAC prompt.
    return scoped_ptr<DaemonInstallerWin>(
        new DaemonComInstallerWin(update3, done));
  } else if (result == CO_E_CLASSSTRING) {
    // The machine instance of Omaha is not available so we will have to run
    // GoogleUpdate.exe manually passing "needsadmin=True". This will cause
    // Omaha to install the machine instance first and then install Chromoting
    // Host.
    return scoped_ptr<DaemonInstallerWin>(
        new DaemonCommandLineInstallerWin(done));
  } else {
    // The user declined the UAC prompt or some other error occured.
    done.Run(result);
    return scoped_ptr<DaemonInstallerWin>();
  }
}

HWND GetTopLevelWindow(HWND window) {
  if (window == NULL) {
    return NULL;
  }

  for (;;) {
    LONG style = GetWindowLong(window, GWL_STYLE);
    if ((style & WS_OVERLAPPEDWINDOW) == WS_OVERLAPPEDWINDOW ||
        (style & WS_POPUP) == WS_POPUP) {
      return window;
    }

    HWND parent = GetAncestor(window, GA_PARENT);
    if (parent == NULL) {
      return window;
    }

    window = parent;
  }
}

}  // namespace remoting
