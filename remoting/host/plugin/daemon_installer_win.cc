// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/plugin/daemon_installer_win.h"

#include <windows.h>

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/string16.h"
#include "base/stringize_macros.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/timer.h"
#include "base/win/object_watcher.h"
#include "base/win/registry.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_handle.h"

namespace omaha {
#include "google_update/google_update_idl.h"
}  // namespace omaha

#include "remoting/host/constants.h"

using base::win::ScopedBstr;
using base::win::ScopedComPtr;

namespace {

// The COM elevation moniker for Omaha.
const char16 kOmahaElevationMoniker[] =
    TO_L_STRING("Elevation:Administrator!new:GoogleUpdate.Update3WebMachine");

// The registry key where the configuration of Omaha is stored.
const char16 kOmahaUpdateKeyName[] = TO_L_STRING("Software\\Google\\Update");

// The name of the value where the full path to GoogleUpdate.exe is stored.
const char16 kOmahaPathValueName[] = TO_L_STRING("path");

// The command line format string for GoogleUpdate.exe
const char16 kGoogleUpdateCommandLineFormat[] =
    TO_L_STRING("\"%ls\" /install \"bundlename=Chromoting%%20Host&appguid=%ls&")
    TO_L_STRING("appname=Chromoting%%20Host&needsadmin=True&lang=%ls\"");

// TODO(alexeypa): Get the desired laungage from the web app.
const char16 kOmahaLanguage[] = TO_L_STRING("en");

// An empty string for optional parameters.
const char16 kOmahaEmpty[] = TO_L_STRING("");

// The installation status polling interval.
const int kOmahaPollIntervalMs = 500;

}  // namespace

namespace remoting {

// This class implements on-demand installation of the Chromoting Host via
// per-machine Omaha instance.
class DaemonComInstallerWin : public DaemonInstallerWin {
 public:
  DaemonComInstallerWin(const ScopedComPtr<omaha::IGoogleUpdate3Web>& update3,
                        const CompletionCallback& done);

  // DaemonInstallerWin implementation.
  virtual void Install() OVERRIDE;

 private:
  // Polls the installation status performing state-specific actions (such as
  // starting installation once download has finished).
  void PollInstallationStatus();

  // Omaha interfaces.
  ScopedComPtr<omaha::IAppWeb> app_;
  ScopedComPtr<omaha::IAppBundleWeb> bundle_;
  ScopedComPtr<omaha::IGoogleUpdate3Web> update3_;

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
    const ScopedComPtr<omaha::IGoogleUpdate3Web>& update3,
    const CompletionCallback& done)
    : DaemonInstallerWin(done),
      update3_(update3),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          polling_timer_(
              FROM_HERE,
              base::TimeDelta::FromMilliseconds(kOmahaPollIntervalMs),
              base::Bind(&DaemonComInstallerWin::PollInstallationStatus,
                         base::Unretained(this)),
              false)) {
}

void DaemonComInstallerWin::Install() {
  // Create an app bundle.
  ScopedComPtr<IDispatch> dispatch;
  HRESULT hr = update3_->createAppBundleWeb(dispatch.Receive());
  if (FAILED(hr)) {
    Done(hr);
    return;
  }

  hr = dispatch.QueryInterface(omaha::IID_IAppBundleWeb, bundle_.ReceiveVoid());
  if (FAILED(hr)) {
    Done(hr);
    return;
  }

  hr = bundle_->initialize();
  if (FAILED(hr)) {
    Done(hr);
    return;
  }

  // Add Chromoting Host to the bundle.
  ScopedBstr appid(kHostOmahaAppid);
  ScopedBstr empty(kOmahaEmpty);
  ScopedBstr language(kOmahaLanguage);
  hr = bundle_->createApp(appid, empty, language, empty);
  if (FAILED(hr)) {
    Done(hr);
    return;
  }

  hr = bundle_->checkForUpdate();
  if (FAILED(hr)) {
    Done(hr);
    return;
  }

  dispatch.Release();
  hr = bundle_->get_appWeb(0, dispatch.Receive());
  if (FAILED(hr)) {
    Done(hr);
    return;
  }

  hr = dispatch.QueryInterface(omaha::IID_IAppWeb,
                               app_.ReceiveVoid());
  if (FAILED(hr)) {
    Done(hr);
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
  ScopedComPtr<IDispatch> dispatch;
  HRESULT hr = app_->get_currentState(dispatch.Receive());
  if (FAILED(hr)) {
    Done(hr);
    return;
  }

  ScopedComPtr<omaha::ICurrentState> current_state;
  hr = dispatch.QueryInterface(omaha::IID_ICurrentState,
                               current_state.ReceiveVoid());
  if (FAILED(hr)) {
    Done(hr);
    return;
  }

  LONG state;
  hr = current_state->get_stateValue(&state);
  if (FAILED(hr)) {
    Done(hr);
    return;
  }

  // Perform state-specific actions.
  switch (state) {
    case omaha::STATE_INIT:
    case omaha::STATE_WAITING_TO_CHECK_FOR_UPDATE:
    case omaha::STATE_CHECKING_FOR_UPDATE:
    case omaha::STATE_WAITING_TO_DOWNLOAD:
    case omaha::STATE_RETRYING_DOWNLOAD:
    case omaha::STATE_DOWNLOADING:
    case omaha::STATE_WAITING_TO_INSTALL:
    case omaha::STATE_INSTALLING:
    case omaha::STATE_PAUSED:
      break;

    case omaha::STATE_UPDATE_AVAILABLE:
      hr = bundle_->download();
      if (FAILED(hr)) {
        Done(hr);
        return;
      }
      break;

    case omaha::STATE_DOWNLOAD_COMPLETE:
    case omaha::STATE_EXTRACTING:
    case omaha::STATE_APPLYING_DIFFERENTIAL_PATCH:
    case omaha::STATE_READY_TO_INSTALL:
      hr = bundle_->install();
      if (FAILED(hr)) {
        Done(hr);
        return;
      }
      break;

    case omaha::STATE_INSTALL_COMPLETE:
    case omaha::STATE_NO_UPDATE:
      // Installation complete or not required. Report success.
      Done(S_OK);
      return;

    case omaha::STATE_ERROR: {
      HRESULT error_code;
      hr = current_state->get_errorCode(&error_code);
      if (FAILED(hr)) {
        error_code = hr;
      }
      Done(error_code);
      return;
    }

    default:
      LOG(ERROR) << "Unknown bundle state: " << state << ".";
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

  string16 google_update;
  result = update_key.ReadValue(kOmahaPathValueName,
                                &google_update);
  if (result != ERROR_SUCCESS) {
    Done(HRESULT_FROM_WIN32(result));
    return;
  }

  // Launch the updater process and wait for its termination.
  string16 command_line =
      StringPrintf(kGoogleUpdateCommandLineFormat,
                   google_update.c_str(),
                   kHostOmahaAppid,
                   kOmahaLanguage);

  base::LaunchOptions options;
  if (!base::LaunchProcess(command_line, options, process_.Receive())) {
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
  done_.Run(result);
}

// static
scoped_ptr<DaemonInstallerWin> DaemonInstallerWin::Create(
    HWND window_handle,
    CompletionCallback done) {
  // Check if the machine instance of Omaha is available.
  BIND_OPTS3 bind_options;
  memset(&bind_options, 0, sizeof(bind_options));
  bind_options.cbStruct = sizeof(bind_options);
  bind_options.hwnd = GetTopLevelWindow(window_handle);
  bind_options.dwClassContext = CLSCTX_LOCAL_SERVER;

  ScopedComPtr<omaha::IGoogleUpdate3Web> update3;
  HRESULT result = ::CoGetObject(
      kOmahaElevationMoniker,
      &bind_options,
      omaha::IID_IGoogleUpdate3Web,
      update3.ReceiveVoid());
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
