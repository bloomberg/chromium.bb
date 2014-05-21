// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/daemon_controller_delegate_win.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_comptr.h"
#include "base/win/windows_version.h"
#include "remoting/base/scoped_sc_handle_win.h"
#include "remoting/host/branding.h"
// chromoting_lib.h contains MIDL-generated declarations.
#include "remoting/host/chromoting_lib.h"
#include "remoting/host/usage_stats_consent.h"

using base::win::ScopedBstr;
using base::win::ScopedComPtr;

namespace remoting {

namespace {

// ProgID of the daemon controller.
const wchar_t kDaemonController[] =
    L"ChromotingElevatedController.ElevatedController";

// The COM elevation moniker for the Elevated Controller.
const wchar_t kDaemonControllerElevationMoniker[] =
    L"Elevation:Administrator!new:"
    L"ChromotingElevatedController.ElevatedController";

// The maximum duration of keeping a reference to a privileged instance of
// the Daemon Controller. This effectively reduces number of UAC prompts a user
// sees.
const int kPrivilegedTimeoutSec = 5 * 60;

// The maximum duration of keeping a reference to an unprivileged instance of
// the Daemon Controller. This interval should not be too long. If upgrade
// happens while there is a live reference to a Daemon Controller instance
// the old binary still can be used. So dropping the references often makes sure
// that the old binary will go away sooner.
const int kUnprivilegedTimeoutSec = 60;

void ConfigToString(const base::DictionaryValue& config, ScopedBstr* out) {
  std::string config_str;
  base::JSONWriter::Write(&config, &config_str);
  ScopedBstr config_scoped_bstr(base::UTF8ToUTF16(config_str).c_str());
  out->Swap(config_scoped_bstr);
}

DaemonController::State ConvertToDaemonState(DWORD service_state) {
  switch (service_state) {
  case SERVICE_RUNNING:
    return DaemonController::STATE_STARTED;

  case SERVICE_CONTINUE_PENDING:
  case SERVICE_START_PENDING:
    return DaemonController::STATE_STARTING;
    break;

  case SERVICE_PAUSE_PENDING:
  case SERVICE_STOP_PENDING:
    return DaemonController::STATE_STOPPING;
    break;

  case SERVICE_PAUSED:
  case SERVICE_STOPPED:
    return DaemonController::STATE_STOPPED;
    break;

  default:
    NOTREACHED();
    return DaemonController::STATE_UNKNOWN;
  }
}

DWORD OpenService(ScopedScHandle* service_out) {
  // Open the service and query its current state.
  ScopedScHandle scmanager(
      ::OpenSCManagerW(NULL, SERVICES_ACTIVE_DATABASE,
                       SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE));
  if (!scmanager.IsValid()) {
    DWORD error = GetLastError();
    PLOG(ERROR) << "Failed to connect to the service control manager";
    return error;
  }

  ScopedScHandle service(
      ::OpenServiceW(scmanager, kWindowsServiceName, SERVICE_QUERY_STATUS));
  if (!service.IsValid()) {
    DWORD error = GetLastError();
    if (error != ERROR_SERVICE_DOES_NOT_EXIST) {
      PLOG(ERROR) << "Failed to open to the '" << kWindowsServiceName
                  << "' service";
    }
    return error;
  }

  service_out->Set(service.Take());
  return ERROR_SUCCESS;
}

DaemonController::AsyncResult HResultToAsyncResult(
    HRESULT hr) {
  if (SUCCEEDED(hr)) {
    return DaemonController::RESULT_OK;
  } else if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
    return DaemonController::RESULT_CANCELLED;
  } else {
    // TODO(sergeyu): Report other errors to the webapp once it knows
    // how to handle them.
    return DaemonController::RESULT_FAILED;
  }
}

void InvokeCompletionCallback(
    const DaemonController::CompletionCallback& done, HRESULT hr) {
  done.Run(HResultToAsyncResult(hr));
}

}  // namespace

DaemonControllerDelegateWin::DaemonControllerDelegateWin()
    : control_is_elevated_(false),
      window_handle_(NULL) {
}

DaemonControllerDelegateWin::~DaemonControllerDelegateWin() {
}

DaemonController::State DaemonControllerDelegateWin::GetState() {
  if (base::win::GetVersion() < base::win::VERSION_XP) {
    return DaemonController::STATE_NOT_IMPLEMENTED;
  }
  // TODO(alexeypa): Make the thread alertable, so we can switch to APC
  // notifications rather than polling.
  ScopedScHandle service;
  DWORD error = OpenService(&service);

  switch (error) {
    case ERROR_SUCCESS: {
      SERVICE_STATUS status;
      if (::QueryServiceStatus(service, &status)) {
        return ConvertToDaemonState(status.dwCurrentState);
      } else {
        PLOG(ERROR) << "Failed to query the state of the '"
                    << kWindowsServiceName << "' service";
        return DaemonController::STATE_UNKNOWN;
      }
      break;
    }
    case ERROR_SERVICE_DOES_NOT_EXIST:
      return DaemonController::STATE_NOT_INSTALLED;
    default:
      return DaemonController::STATE_UNKNOWN;
  }
}

scoped_ptr<base::DictionaryValue> DaemonControllerDelegateWin::GetConfig() {
  // Configure and start the Daemon Controller if it is installed already.
  HRESULT hr = ActivateController();
  if (FAILED(hr))
    return scoped_ptr<base::DictionaryValue>();

  // Get the host configuration.
  ScopedBstr host_config;
  hr = control_->GetConfig(host_config.Receive());
  if (FAILED(hr))
    return scoped_ptr<base::DictionaryValue>();

  // Parse the string into a dictionary.
  base::string16 file_content(
      static_cast<BSTR>(host_config), host_config.Length());
  scoped_ptr<base::Value> config(
      base::JSONReader::Read(base::UTF16ToUTF8(file_content),
          base::JSON_ALLOW_TRAILING_COMMAS));

  if (!config || config->GetType() != base::Value::TYPE_DICTIONARY)
    return scoped_ptr<base::DictionaryValue>();

  return scoped_ptr<base::DictionaryValue>(
      static_cast<base::DictionaryValue*>(config.release()));
}

void DaemonControllerDelegateWin::InstallHost(
    const DaemonController::CompletionCallback& done) {
  DoInstallHost(base::Bind(&InvokeCompletionCallback, done));
}

void DaemonControllerDelegateWin::SetConfigAndStart(
    scoped_ptr<base::DictionaryValue> config,
    bool consent,
    const DaemonController::CompletionCallback& done) {
  DoInstallHost(
      base::Bind(&DaemonControllerDelegateWin::StartHostWithConfig,
                 base::Unretained(this), base::Passed(&config), consent, done));
}

void DaemonControllerDelegateWin::DoInstallHost(
    const DaemonInstallerWin::CompletionCallback& done) {
  // Configure and start the Daemon Controller if it is installed already.
  HRESULT hr = ActivateElevatedController();
  if (SUCCEEDED(hr)) {
    done.Run(S_OK);
    return;
  }

  // Otherwise, install it if its COM registration entry is missing.
  if (hr == CO_E_CLASSSTRING) {
    DCHECK(!installer_);

    installer_ = DaemonInstallerWin::Create(
        GetTopLevelWindow(window_handle_), done);
    installer_->Install();
    return;
  }

  LOG(ERROR) << "Failed to initiate the Chromoting Host installation "
             << "(error: 0x" << std::hex << hr << std::dec << ").";
  done.Run(hr);
}

void DaemonControllerDelegateWin::UpdateConfig(
    scoped_ptr<base::DictionaryValue> config,
    const DaemonController::CompletionCallback& done) {
  HRESULT hr = ActivateElevatedController();
  if (FAILED(hr)) {
    InvokeCompletionCallback(done, hr);
    return;
  }

  // Update the configuration.
  ScopedBstr config_str(NULL);
  ConfigToString(*config, &config_str);
  if (config_str == NULL) {
    InvokeCompletionCallback(done, E_OUTOFMEMORY);
    return;
  }

  // Make sure that the PIN confirmation dialog is focused properly.
  hr = control_->SetOwnerWindow(
      reinterpret_cast<LONG_PTR>(GetTopLevelWindow(window_handle_)));
  if (FAILED(hr)) {
    InvokeCompletionCallback(done, hr);
    return;
  }

  hr = control_->UpdateConfig(config_str);
  InvokeCompletionCallback(done, hr);
}

void DaemonControllerDelegateWin::Stop(
    const DaemonController::CompletionCallback& done) {
  HRESULT hr = ActivateElevatedController();
  if (SUCCEEDED(hr))
    hr = control_->StopDaemon();

  InvokeCompletionCallback(done, hr);
}

void DaemonControllerDelegateWin::SetWindow(void* window_handle) {
  window_handle_ = reinterpret_cast<HWND>(window_handle);
}

std::string DaemonControllerDelegateWin::GetVersion() {
  // Configure and start the Daemon Controller if it is installed already.
  HRESULT hr = ActivateController();
  if (FAILED(hr))
    return std::string();

  // Get the version string.
  ScopedBstr version;
  hr = control_->GetVersion(version.Receive());
  if (FAILED(hr))
    return std::string();

  return base::UTF16ToUTF8(
      base::string16(static_cast<BSTR>(version), version.Length()));
}

DaemonController::UsageStatsConsent
DaemonControllerDelegateWin::GetUsageStatsConsent() {
  DaemonController::UsageStatsConsent consent;
  consent.supported = true;
  consent.allowed = false;
  consent.set_by_policy = false;

  // Activate the Daemon Controller and see if it supports |IDaemonControl2|.
  HRESULT hr = ActivateController();
  if (FAILED(hr)) {
    // The host is not installed yet. Assume that the user didn't consent to
    // collecting crash dumps.
    return consent;
  }

  if (control2_.get() == NULL) {
    // The host is installed and does not support crash dump reporting.
    return consent;
  }

  // Get the recorded user's consent.
  BOOL allowed;
  BOOL set_by_policy;
  hr = control2_->GetUsageStatsConsent(&allowed, &set_by_policy);
  if (FAILED(hr)) {
    // If the user's consent is not recorded yet, assume that the user didn't
    // consent to collecting crash dumps.
    return consent;
  }

  consent.allowed = !!allowed;
  consent.set_by_policy = !!set_by_policy;
  return consent;
}

HRESULT DaemonControllerDelegateWin::ActivateController() {
  if (!control_) {
    CLSID class_id;
    HRESULT hr = CLSIDFromProgID(kDaemonController, &class_id);
    if (FAILED(hr)) {
      return hr;
    }

    hr = CoCreateInstance(class_id, NULL, CLSCTX_LOCAL_SERVER,
                          IID_IDaemonControl, control_.ReceiveVoid());
    if (FAILED(hr)) {
      return hr;
    }

    // Ignore the error. IID_IDaemonControl2 is optional.
    control_.QueryInterface(IID_IDaemonControl2, control2_.ReceiveVoid());

    // Release |control_| upon expiration of the timeout.
    release_timer_.reset(new base::OneShotTimer<DaemonControllerDelegateWin>());
    release_timer_->Start(FROM_HERE,
                          base::TimeDelta::FromSeconds(kUnprivilegedTimeoutSec),
                          this,
                          &DaemonControllerDelegateWin::ReleaseController);
  }

  return S_OK;
}

HRESULT DaemonControllerDelegateWin::ActivateElevatedController() {
  // The COM elevation is supported on Vista and above.
  if (base::win::GetVersion() < base::win::VERSION_VISTA)
    return ActivateController();

  // Release an unprivileged instance of the daemon controller if any.
  if (!control_is_elevated_)
    ReleaseController();

  if (!control_) {
    BIND_OPTS3 bind_options;
    memset(&bind_options, 0, sizeof(bind_options));
    bind_options.cbStruct = sizeof(bind_options);
    bind_options.hwnd = GetTopLevelWindow(window_handle_);
    bind_options.dwClassContext  = CLSCTX_LOCAL_SERVER;

    HRESULT hr = ::CoGetObject(
        kDaemonControllerElevationMoniker,
        &bind_options,
        IID_IDaemonControl,
        control_.ReceiveVoid());
    if (FAILED(hr)) {
      return hr;
    }

    // Ignore the error. IID_IDaemonControl2 is optional.
    control_.QueryInterface(IID_IDaemonControl2, control2_.ReceiveVoid());

    // Note that we hold a reference to an elevated instance now.
    control_is_elevated_ = true;

    // Release |control_| upon expiration of the timeout.
    release_timer_.reset(new base::OneShotTimer<DaemonControllerDelegateWin>());
    release_timer_->Start(FROM_HERE,
                          base::TimeDelta::FromSeconds(kPrivilegedTimeoutSec),
                          this,
                          &DaemonControllerDelegateWin::ReleaseController);
  }

  return S_OK;
}

void DaemonControllerDelegateWin::ReleaseController() {
  control_.Release();
  control2_.Release();
  release_timer_.reset();
  control_is_elevated_ = false;
}

void DaemonControllerDelegateWin::StartHostWithConfig(
    scoped_ptr<base::DictionaryValue> config,
    bool consent,
    const DaemonController::CompletionCallback& done,
    HRESULT hr) {
  installer_.reset();

  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to install the Chromoting Host "
               << "(error: 0x" << std::hex << hr << std::dec << ").";
    InvokeCompletionCallback(done, hr);
    return;
  }

  hr = ActivateElevatedController();
  if (FAILED(hr)) {
    InvokeCompletionCallback(done, hr);
    return;
  }

  // Record the user's consent.
  if (control2_) {
    hr = control2_->SetUsageStatsConsent(consent);
    if (FAILED(hr)) {
      InvokeCompletionCallback(done, hr);
      return;
    }
  }

  // Set the configuration.
  ScopedBstr config_str(NULL);
  ConfigToString(*config, &config_str);
  if (config_str == NULL) {
    InvokeCompletionCallback(done, E_OUTOFMEMORY);
    return;
  }

  hr = control_->SetOwnerWindow(
      reinterpret_cast<LONG_PTR>(GetTopLevelWindow(window_handle_)));
  if (FAILED(hr)) {
    InvokeCompletionCallback(done, hr);
    return;
  }

  hr = control_->SetConfig(config_str);
  if (FAILED(hr)) {
    InvokeCompletionCallback(done, hr);
    return;
  }

  // Start daemon.
  hr = control_->StartDaemon();
  InvokeCompletionCallback(done, hr);
}

scoped_refptr<DaemonController> DaemonController::Create() {
  scoped_ptr<DaemonController::Delegate> delegate(
      new DaemonControllerDelegateWin());
  return new DaemonController(delegate.Pass());
}

}  // namespace remoting
