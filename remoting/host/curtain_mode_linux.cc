// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/curtain_mode.h"

#include <X11/extensions/XInput.h>

#include "base/callback.h"
#include "base/single_thread_task_runner.h"
#include "remoting/base/logging.h"
#include "remoting/host/client_session_control.h"

namespace remoting {

class CurtainModeLinux : public CurtainMode {
 public:
  CurtainModeLinux();

  // Overriden from CurtainMode.
  virtual bool Activate() OVERRIDE;

 private:
  // Returns true if the host is running under an Xvfb session.
  bool IsXvfbSession();

  DISALLOW_COPY_AND_ASSIGN(CurtainModeLinux);
};

CurtainModeLinux::CurtainModeLinux() {
}

bool CurtainModeLinux::Activate() {
  // We can't curtain the session in run-time in Linux.
  // Either the session is running on Xvfb (i.e. always curtained), or it is
  // attached to the physical console (i.e. impossible to curtain).
  bool activated = IsXvfbSession();
  if (!activated) {
    LOG(ERROR) << "Curtain-mode is not supported when running on non-Xvfb "
                  "X server";
  }

  return activated;
}

bool CurtainModeLinux::IsXvfbSession() {
  // Try to identify an Xvfb session. There's no way to query what X server we
  // are running under, so we check for the Xvfb input devices.
  // TODO(rmsousa): Find a similar way to determine that the *output* is secure.
  Display* display = XOpenDisplay(NULL);
  int opcode, event, error;
  if (!XQueryExtension(display, "XInputExtension", &opcode, &event, &error)) {
    // If XInput is not available, assume it is not an Xvfb session.
    LOG(ERROR) << "X Input extension not available: " << error;
    XCloseDisplay(display);
    return false;
  }
  int num_devices;
  XDeviceInfo* devices;
  bool found_xvfb_mouse = false;
  bool found_xvfb_keyboard = false;
  bool found_other_devices = false;
  devices = XListInputDevices(display, &num_devices);
  for (int i = 0; i < num_devices; i++) {
    XDeviceInfo* device_info = &devices[i];
    if (device_info->use == IsXExtensionPointer) {
      if (strcmp(device_info->name, "Xvfb mouse") == 0) {
        found_xvfb_mouse = true;
      } else if (strcmp(device_info->name, "Virtual core XTEST pointer") != 0) {
        found_other_devices = true;
        HOST_LOG << "Non Xvfb mouse found: " << device_info->name;
      }
    } else if (device_info->use == IsXExtensionKeyboard) {
      if (strcmp(device_info->name, "Xvfb keyboard") == 0) {
        found_xvfb_keyboard = true;
      } else if (strcmp(device_info->name,
                        "Virtual core XTEST keyboard") != 0) {
        found_other_devices = true;
        HOST_LOG << "Non Xvfb keyboard found: " << device_info->name;
      }
    } else if (device_info->use == IsXPointer) {
      if (strcmp(device_info->name, "Virtual core pointer") != 0) {
        found_other_devices = true;
        HOST_LOG << "Non Xvfb mouse found: " << device_info->name;
      }
    } else if (device_info->use == IsXKeyboard) {
      if (strcmp(device_info->name, "Virtual core keyboard") != 0) {
        found_other_devices = true;
        HOST_LOG << "Non Xvfb keyboard found: " << device_info->name;
      }
    } else {
      found_other_devices = true;
      HOST_LOG << "Non Xvfb device found: " << device_info->name;
    }
  }
  XFreeDeviceList(devices);
  XCloseDisplay(display);
  return found_xvfb_mouse && found_xvfb_keyboard && !found_other_devices;
}

// static
scoped_ptr<CurtainMode> CurtainMode::Create(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control) {
  return scoped_ptr<CurtainMode>(new CurtainModeLinux());
}

}  // namespace remoting
