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
#include "base/win/wrapped_window_proc.h"
#include "remoting/host/client_session_control.h"
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
  class Core : public base::RefCountedThreadSafe<Core> {
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

    // Handles WM_CREATE messages.
    LRESULT OnCreate(HWND hwnd);

    // Handles WM_INPUT messages.
    LRESULT OnInput(HRAWINPUT input_handle);

    // Window procedure invoked by the OS to process window messages.
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message,
                                       WPARAM wparam, LPARAM lparam);

    // Task runner on which public methods of this class must be called.
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

    // Task runner on which |window_| is created.
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

    // Atom representing the input window class.
    ATOM atom_;

    // Instance of the module containing the window procedure.
    HINSTANCE instance_;

    // Handle of the input window.
    HWND window_;

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
      atom_(0),
      instance_(NULL),
      window_(NULL),
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
  DCHECK(!atom_);
  DCHECK(!instance_);
  DCHECK(!window_);
}

void LocalInputMonitorWin::Core::StartOnUiThread() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  // Create a window for receiving raw input.
  string16 window_class_name = base::StringPrintf(kWindowClassFormat, this);
  WNDCLASSEX window_class;
  base::win::InitializeWindowClass(
      window_class_name.c_str(),
      &base::win::WrappedWindowProc<WindowProc>,
      0, 0, 0, NULL, NULL, NULL, NULL, NULL,
      &window_class);
  instance_ = window_class.hInstance;
  atom_ = RegisterClassEx(&window_class);
  if (atom_ == 0) {
    LOG_GETLASTERROR(ERROR)
        << "Failed to register the window class '" << window_class_name << "'";
    return;
  }

  window_ = CreateWindow(MAKEINTATOM(atom_), 0, 0, 0, 0, 0, 0, HWND_MESSAGE, 0,
                         instance_, this);
  if (window_ == NULL) {
    LOG_GETLASTERROR(ERROR) << "Failed to create the raw input window";
    return;
  }
}

void LocalInputMonitorWin::Core::StopOnUiThread() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  if (window_ != NULL) {
    DestroyWindow(window_);
    window_ = NULL;
  }

  if (atom_ != 0) {
    UnregisterClass(MAKEINTATOM(atom_), instance_);
    atom_ = 0;
    instance_ = NULL;
  }
}

LRESULT LocalInputMonitorWin::Core::OnCreate(HWND hwnd) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  // Register to receive raw mouse input.
  RAWINPUTDEVICE device = {0};
  device.dwFlags = RIDEV_INPUTSINK;
  device.usUsagePage = kGenericDesktopPage;
  device.usUsage = kMouseUsage;
  device.hwndTarget = hwnd;
  if (!RegisterRawInputDevices(&device, 1, sizeof(device))) {
    LOG_GETLASTERROR(ERROR) << "RegisterRawInputDevices() failed";
    return -1;
  }

  return 0;
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

// static
LRESULT CALLBACK LocalInputMonitorWin::Core::WindowProc(
    HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  // Store |this| to the window's user data.
  if (message == WM_CREATE) {
    CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lparam);
    SetLastError(ERROR_SUCCESS);
    LONG_PTR result = SetWindowLongPtr(
        hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
    CHECK(result != 0 || GetLastError() == ERROR_SUCCESS);
  }

  Core* self = reinterpret_cast<Core*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
  switch (message) {
    case WM_CREATE:
      return self->OnCreate(hwnd);

    case WM_INPUT:
      return self->OnInput(reinterpret_cast<HRAWINPUT>(lparam));

    default:
      return DefWindowProc(hwnd, message, wparam, lparam);
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
