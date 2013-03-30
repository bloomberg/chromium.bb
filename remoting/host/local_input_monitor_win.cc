// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/local_input_monitor.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/stringprintf.h"
#include "base/threading/non_thread_safe.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/win/message_window.h"
#include "third_party/skia/include/core/SkPoint.h"

namespace remoting {

namespace {

const wchar_t kWindowClassFormat[] = L"Chromoting_LocalInputMonitorWin_%p";

// From the HID Usage Tables specification.
const USHORT kGenericDesktopPage = 1;
const USHORT kMouseUsage = 2;

class LocalInputMonitorWin : public base::NonThreadSafe,
                             public LocalInputMonitor {
 public:
  LocalInputMonitorWin(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      base::WeakPtr<ClientSessionControl> client_session_control);
  ~LocalInputMonitorWin();

 private:
  // The actual implementation resides in LocalInputMonitorWin::Core class.
  class Core : public base::RefCountedThreadSafe<Core>,
               public win::MessageWindow::Delegate {
   public:
    Core(scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
         scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
         base::WeakPtr<ClientSessionControl> client_session_control);

    void Start();
    void Stop();

   private:
    friend class base::RefCountedThreadSafe<Core>;
    virtual ~Core();

    void StartOnUiThread();
    void StopOnUiThread();

    // Handles WM_INPUT messages.
    LRESULT OnInput(HRAWINPUT input_handle);

    // win::MessageWindow::Delegate interface.
    virtual bool HandleMessage(HWND hwnd,
                               UINT message,
                               WPARAM wparam,
                               LPARAM lparam,
                               LRESULT* result) OVERRIDE;

    // Task runner on which public methods of this class must be called.
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

    // Task runner on which |window_| is created.
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

    // Used to receive raw input.
    scoped_ptr<win::MessageWindow> window_;

    // Points to the object receiving mouse event notifications.
    base::WeakPtr<ClientSessionControl> client_session_control_;

    DISALLOW_COPY_AND_ASSIGN(Core);
  };

  scoped_refptr<Core> core_;

  DISALLOW_COPY_AND_ASSIGN(LocalInputMonitorWin);
};

LocalInputMonitorWin::LocalInputMonitorWin(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control)
    : core_(new Core(caller_task_runner,
                     ui_task_runner,
                     client_session_control)) {
  core_->Start();
}

LocalInputMonitorWin::~LocalInputMonitorWin() {
  core_->Stop();
}

LocalInputMonitorWin::Core::Core(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control)
    : caller_task_runner_(caller_task_runner),
      ui_task_runner_(ui_task_runner),
      client_session_control_(client_session_control) {
  DCHECK(client_session_control_);
}

void LocalInputMonitorWin::Core::Start() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  ui_task_runner_->PostTask(FROM_HERE,
                            base::Bind(&Core::StartOnUiThread, this));
}

void LocalInputMonitorWin::Core::Stop() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  ui_task_runner_->PostTask(FROM_HERE, base::Bind(&Core::StopOnUiThread, this));
}

LocalInputMonitorWin::Core::~Core() {
  DCHECK(!window_);
}

void LocalInputMonitorWin::Core::StartOnUiThread() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  window_.reset(new win::MessageWindow());
  if (!window_->Create(this)) {
    LOG_GETLASTERROR(ERROR) << "Failed to create the raw input window";
    window_.reset();

    // If the local input cannot be monitored, the remote user can take over
    // the session. Disconnect the session now to prevent this.
    caller_task_runner_->PostTask(
        FROM_HERE, base::Bind(&ClientSessionControl::DisconnectSession,
                              client_session_control_));
  }
}

void LocalInputMonitorWin::Core::StopOnUiThread() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  // Stop receiving  raw mouse input.
  if (window_) {
    RAWINPUTDEVICE device = {0};
    device.dwFlags = RIDEV_REMOVE;
    device.usUsagePage = kGenericDesktopPage;
    device.usUsage = kMouseUsage;
    device.hwndTarget = window_->hwnd();

    // The error is harmless, ignore it.
    RegisterRawInputDevices(&device, 1, sizeof(device));
  }

  window_.reset();
}

LRESULT LocalInputMonitorWin::Core::OnInput(HRAWINPUT input_handle) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  // Get the size of the input record.
  UINT size = 0;
  UINT result = GetRawInputData(input_handle,
                                RID_INPUT,
                                NULL,
                                &size,
                                sizeof(RAWINPUTHEADER));
  if (result == -1) {
    LOG_GETLASTERROR(ERROR) << "GetRawInputData() failed";
    return 0;
  }

  // Retrieve the input record itself.
  scoped_array<uint8> buffer(new uint8[size]);
  RAWINPUT* input = reinterpret_cast<RAWINPUT*>(buffer.get());
  result = GetRawInputData(input_handle,
                           RID_INPUT,
                           buffer.get(),
                           &size,
                           sizeof(RAWINPUTHEADER));
  if (result == -1) {
    LOG_GETLASTERROR(ERROR) << "GetRawInputData() failed";
    return 0;
  }

  // Notify the observer about mouse events generated locally. Remote (injected)
  // mouse events do not specify a device handle (based on observed behavior).
  if (input->header.dwType == RIM_TYPEMOUSE &&
      input->header.hDevice != NULL) {
    POINT position;
    if (!GetCursorPos(&position)) {
      position.x = 0;
      position.y = 0;
    }

    caller_task_runner_->PostTask(
        FROM_HERE, base::Bind(&ClientSessionControl::OnLocalMouseMoved,
                              client_session_control_,
                              SkIPoint::Make(position.x, position.y)));
  }

  return DefRawInputProc(&input, 1, sizeof(RAWINPUTHEADER));
}

bool LocalInputMonitorWin::Core::HandleMessage(
    HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, LRESULT* result) {
  switch (message) {
    case WM_CREATE: {
      // Register to receive raw mouse input.
      RAWINPUTDEVICE device = {0};
      device.dwFlags = RIDEV_INPUTSINK;
      device.usUsagePage = kGenericDesktopPage;
      device.usUsage = kMouseUsage;
      device.hwndTarget = hwnd;
      if (RegisterRawInputDevices(&device, 1, sizeof(device))) {
        *result = 0;
      } else {
        LOG_GETLASTERROR(ERROR) << "RegisterRawInputDevices() failed";
        *result = -1;
      }
      return true;
    }

    case WM_INPUT:
      *result = OnInput(reinterpret_cast<HRAWINPUT>(lparam));
      return true;

    default:
      return false;
  }
}

}  // namespace

scoped_ptr<LocalInputMonitor> LocalInputMonitor::Create(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control) {
  return scoped_ptr<LocalInputMonitor>(
      new LocalInputMonitorWin(caller_task_runner,
                               ui_task_runner,
                               client_session_control));
}

}  // namespace remoting
