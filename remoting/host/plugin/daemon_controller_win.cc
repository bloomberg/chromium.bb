// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/plugin/daemon_controller.h"

#include <objbase.h>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "remoting/base/scoped_sc_handle_win.h"
#include "remoting/host/branding.h"

// MIDL-generated declarations and definitions.
#include "elevated_controller.h"
#include "elevated_controller_i.c"

namespace remoting {

namespace {

// The COM elevation moniker for the elevated controller.
const char kElevationMoniker[] = "Elevation:Administrator!new:"
    "{430a9403-8176-4733-afdc-0b325a8fda84}";

// Name of the Daemon Controller's worker thread.
const char kDaemonControllerThreadName[] = "Daemon Controller thread";

// A base::Thread implementation that initializes COM on the new thread.
class ComThread : public base::Thread {
 public:
  explicit ComThread(const char* name);

  // Activates an elevated instance of the controller and returns the pointer
  // to the control interface in |control_out|. This class keeps the ownership
  // of the pointer so the caller should not call call AddRef() or Release().
  HRESULT ActivateElevatedController(IDaemonControl** control_out);

  bool Start();

 protected:
  virtual void Init() OVERRIDE;
  virtual void CleanUp() OVERRIDE;

  IDaemonControl* control_;

  DISALLOW_COPY_AND_ASSIGN(ComThread);
};

class DaemonControllerWin : public remoting::DaemonController {
 public:
  DaemonControllerWin();
  virtual ~DaemonControllerWin();

  virtual State GetState() OVERRIDE;
  virtual void GetConfig(const GetConfigCallback& callback) OVERRIDE;
  virtual void SetConfigAndStart(
      scoped_ptr<base::DictionaryValue> config) OVERRIDE;
  virtual void SetPin(const std::string& pin) OVERRIDE;
  virtual void Stop() OVERRIDE;

 private:
  // Opens the Chromoting service returning its handle in |service_out|.
  DWORD OpenService(ScopedScHandle* service_out);

  // The functions that actually do the work. They should be called in
  // the context of |worker_thread_|;
  void DoGetConfig(const GetConfigCallback& callback);
  void DoSetConfigAndStart(scoped_ptr<base::DictionaryValue> config);
  void DoStop();

  // Converts a Windows service status code to a Daemon state.
  static State ConvertToDaemonState(DWORD service_state);

  // The worker thread used for servicing long running operations.
  ComThread worker_thread_;

  // The lock protecting access to all data members below.
  base::Lock lock_;

  // The error occurred during the last transition.
  HRESULT last_error_;

  // The daemon state reported to the JavaScript code.
  State state_;

  // The state that should never be reported to JS unless there is an error.
  // For instance, when Start() is called, the state of the service doesn't
  // switch to "starting" immediately. This could lead to JS interpreting
  // "stopped" as a failure to start the service.
  // TODO(alexeypa): remove this variable once JS interface supports callbacks.
  State forbidden_state_;

  DISALLOW_COPY_AND_ASSIGN(DaemonControllerWin);
};

ComThread::ComThread(const char* name) : base::Thread(name), control_(NULL) {
}

void ComThread::Init() {
  CoInitialize(NULL);
}

void ComThread::CleanUp() {
  if (control_ != NULL)
    control_->Release();
  CoUninitialize();
}

HRESULT ComThread::ActivateElevatedController(
    IDaemonControl** control_out) {
  // Chache the instance of Elevated Controller to prevent a UAC prompt on every
  // operation.
  if (control_ == NULL) {
    BIND_OPTS3 bind_options;
    memset(&bind_options, 0, sizeof(bind_options));
    bind_options.cbStruct = sizeof(bind_options);
    bind_options.hwnd = NULL;
    bind_options.dwClassContext  = CLSCTX_LOCAL_SERVER;

    HRESULT hr = ::CoGetObject(ASCIIToUTF16(kElevationMoniker).c_str(),
                               &bind_options,
                               IID_IDaemonControl,
                               reinterpret_cast<void**>(&control_));
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to create the elevated controller (error: 0x"
          << std::hex << hr << std::dec << ").";
      return hr;
    }
  }

  *control_out = control_;
  return S_OK;
}

bool ComThread::Start() {
  // N.B. The single threaded COM apartment must be run on a UI message loop.
  base::Thread::Options thread_options(MessageLoop::TYPE_UI, 0);
  return StartWithOptions(thread_options);
}

DaemonControllerWin::DaemonControllerWin()
    : last_error_(S_OK),
      state_(STATE_UNKNOWN),
      forbidden_state_(STATE_UNKNOWN),
      worker_thread_(kDaemonControllerThreadName) {
  if (!worker_thread_.Start()) {
    // N.B. Start() does not report the error code returned by the system.
    last_error_ = E_FAIL;
  }
}

DaemonControllerWin::~DaemonControllerWin() {
  worker_thread_.Stop();
}

remoting::DaemonController::State DaemonControllerWin::GetState() {
  // TODO(alexeypa): Make the thread alertable, so we can switch to APC
  // notifications rather than polling.
  ScopedScHandle service;
  DWORD error = OpenService(&service);

  if (error == ERROR_SUCCESS) {
    SERVICE_STATUS status;
    if (::QueryServiceStatus(service, &status)) {
      State new_state = ConvertToDaemonState(status.dwCurrentState);

      base::AutoLock lock(lock_);
      // TODO(alexeypa): Remove |forbidden_state_| hack once JS interface
      // supports callbacks.
      if (forbidden_state_ != new_state || FAILED(last_error_)) {
        state_ = new_state;
      }

      // TODO(alexeypa): Remove this hack once JS nicely reports errors.
      if (FAILED(last_error_)) {
        state_ = STATE_START_FAILED;
      }

      return state_;
    } else {
      error = GetLastError();
      LOG_GETLASTERROR(ERROR)
          << "Failed to query the state of the '" << kWindowsServiceName
          << "' service";
    }
  }

  base::AutoLock lock(lock_);
  if (error == ERROR_SERVICE_DOES_NOT_EXIST) {
    state_ = STATE_NOT_IMPLEMENTED;
  } else {
    last_error_ = HRESULT_FROM_WIN32(error);
    state_ = STATE_UNKNOWN;
  }

  return state_;
}

void DaemonControllerWin::GetConfig(const GetConfigCallback& callback) {
  worker_thread_.message_loop_proxy()->PostTask(
      FROM_HERE,
      base::Bind(&DaemonControllerWin::DoGetConfig,
                 base::Unretained(this), callback));
}

void DaemonControllerWin::SetConfigAndStart(
    scoped_ptr<base::DictionaryValue> config) {
  base::AutoLock lock(lock_);

  // TODO(alexeypa): Implement on-demand installation.
  if (state_ == STATE_STOPPED) {
    last_error_ = S_OK;
    forbidden_state_ = STATE_STOPPED;
    state_ = STATE_STARTING;
    worker_thread_.message_loop_proxy()->PostTask(
        FROM_HERE,
        base::Bind(&DaemonControllerWin::DoSetConfigAndStart,
                   base::Unretained(this), base::Passed(&config)));
  }
}

void DaemonControllerWin::SetPin(const std::string& pin) {
  NOTIMPLEMENTED();
}

void DaemonControllerWin::Stop() {
  base::AutoLock lock(lock_);

  if (state_ == STATE_STARTING ||
      state_ == STATE_STARTED ||
      state_ == STATE_STOPPING) {
    last_error_ = S_OK;
    forbidden_state_ = STATE_STARTED;
    state_ = STATE_STOPPING;
    worker_thread_.message_loop_proxy()->PostTask(
        FROM_HERE,
        base::Bind(&DaemonControllerWin::DoStop, base::Unretained(this)));
  }
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
      ::OpenServiceW(scmanager, UTF8ToUTF16(kWindowsServiceName).c_str(),
                     SERVICE_QUERY_STATUS));
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

void DaemonControllerWin::DoGetConfig(const GetConfigCallback& callback) {
  IDaemonControl* control = NULL;
  HRESULT hr = worker_thread_.ActivateElevatedController(&control);
  if (FAILED(hr)) {
    callback.Run(scoped_ptr<base::DictionaryValue>());
    return;
  }

  // Get the host configuration.
  BSTR host_config = NULL;
  hr = control->GetConfig(&host_config);
  if (FAILED(hr)) {
    callback.Run(scoped_ptr<base::DictionaryValue>());
    return;
  }

  string16 file_content(static_cast<char16*>(host_config),
                        ::SysStringLen(host_config));
  SysFreeString(host_config);

  // Parse the string into a dictionary.
  scoped_ptr<base::Value> config(
      base::JSONReader::Read(UTF16ToUTF8(file_content), true));

  base::DictionaryValue* dictionary;
  if (config.get() == NULL || !config->GetAsDictionary(&dictionary)) {
    callback.Run(scoped_ptr<base::DictionaryValue>());
    return;
  }

  config.release();
  callback.Run(scoped_ptr<base::DictionaryValue>(dictionary));
}

void DaemonControllerWin::DoSetConfigAndStart(
    scoped_ptr<base::DictionaryValue> config) {
  IDaemonControl* control = NULL;
  HRESULT hr = worker_thread_.ActivateElevatedController(&control);
  if (FAILED(hr)) {
    base::AutoLock lock(lock_);
    last_error_ = hr;
    return;
  }

  // Store the configuration.
  std::string file_content;
  base::JSONWriter::Write(config.get(), &file_content);

  BSTR host_config = ::SysAllocString(UTF8ToUTF16(file_content).c_str());
  if (host_config == NULL) {
    base::AutoLock lock(lock_);
    last_error_ = E_OUTOFMEMORY;
    return;
  }

  hr = control->SetConfig(host_config);
  ::SysFreeString(host_config);
  if (FAILED(hr)) {
    base::AutoLock lock(lock_);
    last_error_ = hr;
    return;
  }

  // Start daemon.
  hr = control->StartDaemon();
  if (FAILED(hr)) {
    base::AutoLock lock(lock_);
    last_error_ = hr;
  }
}

void DaemonControllerWin::DoStop() {
  IDaemonControl* control = NULL;
  HRESULT hr = worker_thread_.ActivateElevatedController(&control);
  if (FAILED(hr)) {
    base::AutoLock lock(lock_);
    last_error_ = hr;
    return;
  }

  hr = control->StopDaemon();
  if (FAILED(hr)) {
    base::AutoLock lock(lock_);
    last_error_ = hr;
  }
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

}  // namespace

DaemonController* remoting::DaemonController::Create() {
  return new DaemonControllerWin();
}

}  // namespace remoting
