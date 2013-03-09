// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_session_controller.h"

#include "base/logging.h"
#include "remoting/host/desktop_session_proxy.h"

namespace remoting {

IpcSessionController::IpcSessionController(
    scoped_refptr<DesktopSessionProxy> desktop_session_proxy)
    : desktop_session_proxy_(desktop_session_proxy) {
}

IpcSessionController::~IpcSessionController() {
}

void IpcSessionController::OnClientResolutionChanged(
    const SkIPoint& client_dpi,
    const SkISize& client_size) {
  NOTIMPLEMENTED();
}

}  // namespace remoting
