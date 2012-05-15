// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/plugin/daemon_controller.h"

#include <objbase.h>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/string16.h"
#include "base/stringize_macros.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "base/timer.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_comptr.h"
#include "remoting/base/scoped_sc_handle_win.h"
#include "remoting/host/branding.h"
#include "remoting/host/plugin/daemon_installer_win.h"

// MIDL-generated declarations and definitions.
#include "remoting/host/elevated_controller.h"

using base::win::ScopedBstr;
using base::win::ScopedComPtr;

namespace remoting {

namespace {

// ProgID of the daemon controller.
const char16 kDaemonController[] =
    TO_L_STRING("ChromotingElevatedController.ElevatedController");

// The COM elevation moniker for the Elevated Controller.
const char16 kDaemonControllerElevationMoniker[] =
    TO_L_STRING("Elevation:Administrator!new:")
    TO_L_STRING("ChromotingElevatedController.ElevatedController");

// Name of the Daemon Controller's worker thread.
const char kDaemonControllerThreadName[] = "Daemon Controller thread";

// The maximum interval between showing UAC prompts.
const int kUacTimeoutSec = 15 * 60;

// A base::Thread implementation that initializes COM on the new thread.
class ComThread : public base::Thread {
 public:
  explicit ComThread(const char* name);

  bool Start();

 protected:
  virtual void Init() OVERRIDE;
  virtual void CleanUp() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ComThread);
};

class DaemonControllerWin : public remoting::DaemonController {
 public:
  DaemonControllerWin();
  virtual ~DaemonControllerWin();

  virtual State GetState() OVERRIDE;
  virtual void GetConfig(const GetConfigCallback& callback) OVERRIDE;
  virtual void SetConfigAndStart(
      scoped_ptr<base::DictionaryValue> config,
      const CompletionCallback& done_callback) OVERRIDE;
  virtual void UpdateConfig(scoped_ptr<base::DictionaryValue> config,
                            const CompletionCallback& done_callback) OVERRIDE;
  virtual void Stop(const CompletionCallback& done_callback) OVERRIDE;
  virtual void SetWindow(void* window_handle) OVERRIDE;
  virtual void GetVersion(const GetVersionCallback& done_callback) OVERRIDE;

 private:
  // Activates an unprivileged instance of the daemon controller and caches it.
  HRESULT ActivateController();

  // Activates an elevated instance of the daemon controller and caches it.
  HRESULT ActivateElevatedController();

  // Releases the cached instance of the controller.
  void ReleaseController();

  // Procedes with the daemon configuration if the installation succeeded,
  // otherwise reports the error.
  void OnInstallationComplete(scoped_ptr<base::DictionaryValue> config,
                              const CompletionCallback& done_callback,
                              HRESULT result);

  // Opens the Chromoting service returning its handle in |service_out|.
  DWORD OpenService(ScopedScHandle* service_out);

  // Converts a config dictionary to a scoped BSTR.
  static void ConfigToString(const base::DictionaryValue& config,
                             ScopedBstr* out);

  // Converts a Windows service status code to a Daemon state.
  static State ConvertToDaemonState(DWORD service_state);

  // Converts HRESULT to the AsyncResult.
  static AsyncResult HResultToAsyncResult(HRESULT hr);

  // The functions that actually do the work. They should be called in
  // the context of |worker_thread_|;
  void DoGetConfig(const GetConfigCallback& callback);
  void DoInstallAsNeededAndStart(scoped_ptr<base::DictionaryValue> config,
                                 const CompletionCallback& done_callback);
  void DoSetConfigAndStart(scoped_ptr<base::DictionaryValue> config,
                           const CompletionCallback& done_callback);
  void DoUpdateConfig(scoped_ptr<base::DictionaryValue> config,
                      const CompletionCallback& done_callback);
  void DoStop(const CompletionCallback& done_callback);
  void DoSetWindow(void* window_handle);
  void DoGetVersion(const GetVersionCallback& callback);

  // |control_| holds a reference to an instance of the daemon controller
  // to prevent a UAC prompt on every operation.
  ScopedComPtr<IDaemonControl> control_;

  // True if |control_| holds a reference to an elevated instance of the daemon
  // controller.
  bool control_is_elevated_;

  // This timer is used to release |control_| after a timeout.
  scoped_ptr<base::OneShotTimer<DaemonControllerWin> > release_timer_;

  // Handle of the plugin window.
  HWND window_handle_;

  // The worker thread used for servicing long running operations.
  ComThread worker_thread_;

  scoped_ptr<DaemonInstallerWin> installer_;

  DISALLOW_COPY_AND_ASSIGN(DaemonControllerWin);
};

ComThread::ComThread(const char* name) : base::Thread(name) {
}

bool ComThread::Start() {
  // N.B. The single threaded COM apartment must be run on a UI message loop.
  base::Thread::Options thread_options(MessageLoop::TYPE_UI, 0);
  return StartWithOptions(thread_options);
}

void ComThread::Init() {
  CoInitialize(NULL);
}

void ComThread::CleanUp() {
  CoUninitialize();
}

DaemonControllerWin::DaemonControllerWin()
    : control_is_elevated_(false),
      window_handle_(NULL),
      worker_thread_(kDaemonControllerThreadName) {
  if (!worker_thread_.Start()) {
    LOG(FATAL) << "Failed to start the Daemon Controller worker thread.";
  }
}

DaemonControllerWin::~DaemonControllerWin() {
  // Clean up resources allocated on the worker thread.
  worker_thread_.message_loop_proxy()->PostTask(
      FROM_HERE,
      base::Bind(&DaemonControllerWin::ReleaseController,
                 base::Unretained(this)));
  worker_thread_.Stop();
}

remoting::DaemonController::State DaemonControllerWin::GetState() {
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
        LOG_GETLASTERROR(ERROR)
            << "Failed to query the state of the '" << kWindowsServiceName
            << "' service";
        return STATE_UNKNOWN;
      }
      break;
    }
    case ERROR_SERVICE_DOES_NOT_EXIST:
      return STATE_NOT_INSTALLED;
    default:
      return STATE_UNKNOWN;
  }
}

void DaemonControllerWin::GetConfig(const GetConfigCallback& callback) {
  worker_thread_.message_loop_proxy()->PostTask(
      FROM_HERE,
      base::Bind(&DaemonControllerWin::DoGetConfig,
                 base::Unretained(this), callback));
}

void DaemonControllerWin::SetConfigAndStart(
    scoped_ptr<base::DictionaryValue> config,
    const CompletionCallback& done_callback) {
  worker_thread_.message_loop_proxy()->PostTask(
      FROM_HERE, base::Bind(
          &DaemonControllerWin::DoInstallAsNeededAndStart,
          base::Unretained(this), base::Passed(&config), done_callback));
}

void DaemonControllerWin::UpdateConfig(
    scoped_ptr<base::DictionaryValue> config,
    const CompletionCallback& done_callback) {
  worker_thread_.message_loop_proxy()->PostTask(
      FROM_HERE, base::Bind(
          &DaemonControllerWin::DoUpdateConfig,
          base::Unretained(this), base::Passed(&config), done_callback));
}

void DaemonControllerWin::Stop(const CompletionCallback& done_callback) {
  worker_thread_.message_loop_proxy()->PostTask(
      FROM_HERE, base::Bind(
          &DaemonControllerWin::DoStop, base::Unretained(this),
          done_callback));
}

void DaemonControllerWin::SetWindow(void* window_handle) {
  worker_thread_.message_loop_proxy()->PostTask(
      FROM_HERE, base::Bind(
          &DaemonControllerWin::DoSetWindow, base::Unretained(this),
          window_handle));
}

void DaemonControllerWin::GetVersion(const GetVersionCallback& callback) {
  worker_thread_.message_loop_proxy()->PostTask(
      FROM_HERE,
      base::Bind(&DaemonControllerWin::DoGetVersion,
                 base::Unretained(this), callback));
}

HRESULT DaemonControllerWin::ActivateController() {
  DCHECK(worker_thread_.message_loop_proxy()->BelongsToCurrentThread());

  if (control_.get() == NULL) {
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
  }

  return S_OK;
}

HRESULT DaemonControllerWin::ActivateElevatedController() {
  DCHECK(worker_thread_.message_loop_proxy()->BelongsToCurrentThread());

  // Release an unprivileged instance of the daemon controller if any.
  if (!control_is_elevated_) {
    ReleaseController();
  }

  if (control_.get() == NULL) {
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

    // Note that we hold a reference to an elevated instance now.
    control_is_elevated_ = true;

    // Release |control_| upon expiration of the timeout.
    release_timer_.reset(new base::OneShotTimer<DaemonControllerWin>());
    release_timer_->Start(FROM_HERE,
                          base::TimeDelta::FromSeconds(kUacTimeoutSec),
                          this,
                          &DaemonControllerWin::ReleaseController);
  }

  return S_OK;
}

void DaemonControllerWin::ReleaseController() {
  DCHECK(worker_thread_.message_loop_proxy()->BelongsToCurrentThread());

  control_.Release();
  release_timer_.reset();
  control_is_elevated_ = false;
}

void DaemonControllerWin::OnInstallationComplete(
    scoped_ptr<base::DictionaryValue> config,
    const CompletionCallback& done_callback,
    HRESULT result) {
  DCHECK(worker_thread_.message_loop_proxy()->BelongsToCurrentThread());

  if (SUCCEEDED(result)) {
    DoSetConfigAndStart(config.Pass(), done_callback);
  } else {
    LOG(ERROR) << "Failed to install the Chromoting Host "
               << "(error: 0x" << std::hex << result << std::dec << ").";
    done_callback.Run(HResultToAsyncResult(result));
  }

  DCHECK(installer_.get() != NULL);
  installer_.reset();
}

DWORD DaemonControllerWin::OpenService(ScopedScHandle* service_out) {
  // Open the service and query its current state.
  ScopedScHandle scmanager(
      ::OpenSCManagerW(NULL, SERVICES_ACTIVE_DATABASE,
                       SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE));
  if (!scmanager.IsValid()) {
    DWORD error = GetLastError();
    LOG_GETLASTERROR(ERROR)
        << "Failed to connect to the service control manager";
    return error;
  }

  ScopedScHandle service(
      ::OpenServiceW(scmanager, kWindowsServiceName, SERVICE_QUERY_STATUS));
  if (!service.IsValid()) {
    DWORD error = GetLastError();
    if (error != ERROR_SERVICE_DOES_NOT_EXIST) {
      LOG_GETLASTERROR(ERROR)
          << "Failed to open to the '" << kWindowsServiceName << "' service";
    }
    return error;
  }

  service_out->Set(service.Take());
  return ERROR_SUCCESS;
}

// static
void DaemonControllerWin::ConfigToString(const base::DictionaryValue& config,
                                         ScopedBstr* out) {
  std::string config_str;
  base::JSONWriter::Write(&config, &config_str);
  ScopedBstr config_scoped_bstr(UTF8ToUTF16(config_str).c_str());
  out->Swap(config_scoped_bstr);
}

// static
remoting::DaemonController::State DaemonControllerWin::ConvertToDaemonState(
    DWORD service_state) {
  switch (service_state) {
  case SERVICE_RUNNING:
    return STATE_STARTED;

  case SERVICE_CONTINUE_PENDING:
  case SERVICE_START_PENDING:
    return STATE_STARTING;
    break;

  case SERVICE_PAUSE_PENDING:
  case SERVICE_STOP_PENDING:
    return STATE_STOPPING;
    break;

  case SERVICE_PAUSED:
  case SERVICE_STOPPED:
    return STATE_STOPPED;
    break;

  default:
    NOTREACHED();
    return STATE_UNKNOWN;
  }
}

// static
DaemonController::AsyncResult DaemonControllerWin::HResultToAsyncResult(
    HRESULT hr) {
  // TODO(sergeyu): Report other errors to the webapp once it knows
  // how to handle them.
  return FAILED(hr) ? RESULT_FAILED : RESULT_OK;
}

void DaemonControllerWin::DoGetConfig(const GetConfigCallback& callback) {
  DCHECK(worker_thread_.message_loop_proxy()->BelongsToCurrentThread());

  scoped_ptr<base::DictionaryValue> dictionary_null(NULL);

  // Configure and start the Daemon Controller if it is installed already.
  HRESULT hr = ActivateController();
  if (FAILED(hr)) {
    callback.Run(dictionary_null.Pass());
    return;
  }

  // Get the host configuration.
  ScopedBstr host_config;
  hr = control_->GetConfig(host_config.Receive());
  if (FAILED(hr)) {
    callback.Run(dictionary_null.Pass());
    return;
  }

  // Parse the string into a dictionary.
  string16 file_content(static_cast<BSTR>(host_config), host_config.Length());
  scoped_ptr<base::Value> config(
      base::JSONReader::Read(UTF16ToUTF8(file_content),
          base::JSON_ALLOW_TRAILING_COMMAS));

  base::DictionaryValue* dictionary;
  if (config.get() == NULL || !config->GetAsDictionary(&dictionary)) {
    callback.Run(dictionary_null.Pass());
    return;
  }
  // Release |config|, because dictionary points to the same object.
  config.release();

  callback.Run(scoped_ptr<base::DictionaryValue>(dictionary));
}

void DaemonControllerWin::DoInstallAsNeededAndStart(
    scoped_ptr<base::DictionaryValue> config,
    const CompletionCallback& done_callback) {
  DCHECK(worker_thread_.message_loop_proxy()->BelongsToCurrentThread());

  // Configure and start the Daemon Controller if it is installed already.
  HRESULT hr = ActivateElevatedController();
  if (SUCCEEDED(hr)) {
    DoSetConfigAndStart(config.Pass(), done_callback);
    return;
  }

  // Otherwise, install it if its COM registration entry is missing.
  if (hr == CO_E_CLASSSTRING) {
    scoped_ptr<DaemonInstallerWin> installer = DaemonInstallerWin::Create(
        GetTopLevelWindow(window_handle_),
        base::Bind(&DaemonControllerWin::OnInstallationComplete,
                   base::Unretained(this),
                   base::Passed(&config),
                   done_callback));
    if (installer.get()) {
      DCHECK(!installer_.get());
      installer_ = installer.Pass();
      installer_->Install();
    }
  } else {
    LOG(ERROR) << "Failed to initiate the Chromoting Host installation "
               << "(error: 0x" << std::hex << hr << std::dec << ").";
    done_callback.Run(HResultToAsyncResult(hr));
  }
}

void DaemonControllerWin::DoSetConfigAndStart(
    scoped_ptr<base::DictionaryValue> config,
    const CompletionCallback& done_callback) {
  DCHECK(worker_thread_.message_loop_proxy()->BelongsToCurrentThread());

  HRESULT hr = ActivateElevatedController();
  if (FAILED(hr)) {
    done_callback.Run(HResultToAsyncResult(hr));
    return;
  }

  // Set the configuration.
  ScopedBstr config_str(NULL);
  ConfigToString(*config, &config_str);
  if (config_str == NULL) {
    done_callback.Run(HResultToAsyncResult(E_OUTOFMEMORY));
    return;
  }

  hr = control_->SetOwnerWindow(
      reinterpret_cast<LONG_PTR>(GetTopLevelWindow(window_handle_)));
  if (FAILED(hr)) {
    done_callback.Run(HResultToAsyncResult(hr));
    return;
  }

  hr = control_->SetConfig(config_str);
  if (FAILED(hr)) {
    done_callback.Run(HResultToAsyncResult(hr));
    return;
  }

  // Start daemon.
  hr = control_->StartDaemon();
  done_callback.Run(HResultToAsyncResult(hr));
}

void DaemonControllerWin::DoUpdateConfig(
    scoped_ptr<base::DictionaryValue> config,
    const CompletionCallback& done_callback) {
  DCHECK(worker_thread_.message_loop_proxy()->BelongsToCurrentThread());

  HRESULT hr = ActivateElevatedController();
  if (FAILED(hr)) {
    done_callback.Run(HResultToAsyncResult(hr));
    return;
  }

  // Update the configuration.
  ScopedBstr config_str(NULL);
  ConfigToString(*config, &config_str);
  if (config_str == NULL) {
    done_callback.Run(HResultToAsyncResult(E_OUTOFMEMORY));
    return;
  }

  // Make sure that the PIN confirmation dialog is focused properly.
  hr = control_->SetOwnerWindow(
      reinterpret_cast<LONG_PTR>(GetTopLevelWindow(window_handle_)));
  if (FAILED(hr)) {
    done_callback.Run(HResultToAsyncResult(hr));
    return;
  }

  hr = control_->UpdateConfig(config_str);
  done_callback.Run(HResultToAsyncResult(hr));
}

void DaemonControllerWin::DoStop(const CompletionCallback& done_callback) {
  DCHECK(worker_thread_.message_loop_proxy()->BelongsToCurrentThread());

  HRESULT hr = ActivateElevatedController();
  if (FAILED(hr)) {
    done_callback.Run(HResultToAsyncResult(hr));
    return;
  }

  hr = control_->StopDaemon();
  done_callback.Run(HResultToAsyncResult(hr));
}

void DaemonControllerWin::DoSetWindow(void* window_handle) {
  window_handle_ = reinterpret_cast<HWND>(window_handle);
}

void DaemonControllerWin::DoGetVersion(const GetVersionCallback& callback) {
  DCHECK(worker_thread_.message_loop_proxy()->BelongsToCurrentThread());

  std::string version_null;

  // Configure and start the Daemon Controller if it is installed already.
  HRESULT hr = ActivateController();
  if (FAILED(hr)) {
    callback.Run(version_null);
    return;
  }

  // Get the version string.
  ScopedBstr version;
  hr = control_->GetVersion(version.Receive());
  if (FAILED(hr)) {
    callback.Run(version_null);
    return;
  }

  callback.Run(UTF16ToUTF8(
      string16(static_cast<BSTR>(version), version.Length())));
}

}  // namespace

scoped_ptr<DaemonController> remoting::DaemonController::Create() {
  return scoped_ptr<DaemonController>(new DaemonControllerWin());
}

}  // namespace remoting
