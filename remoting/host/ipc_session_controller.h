// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IPC_SESSION_CONTROLLER_H_
#define REMOTING_HOST_IPC_SESSION_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "remoting/host/session_controller.h"

namespace remoting {

class DesktopSessionProxy;
class ScreenResolution;

class IpcSessionController : public SessionController {
 public:
  explicit IpcSessionController(
      scoped_refptr<DesktopSessionProxy> desktop_session_proxy);
  virtual ~IpcSessionController();

  // SessionController interface.
  virtual void SetScreenResolution(const ScreenResolution& resolution) OVERRIDE;

 private:
  // Wraps the IPC channel to the desktop session agent.
  scoped_refptr<DesktopSessionProxy> desktop_session_proxy_;

  DISALLOW_COPY_AND_ASSIGN(IpcSessionController);
};

}  // namespace remoting

#endif  // REMOTING_HOST_IPC_SESSION_CONTROLLER_H_
