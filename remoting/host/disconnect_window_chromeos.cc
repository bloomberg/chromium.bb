// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/wm_shell.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/host_window.h"

namespace remoting {

namespace {

class DisconnectWindowAura : public HostWindow {
 public:
  DisconnectWindowAura();
  ~DisconnectWindowAura() override;

  // HostWindow interface.
  void Start(const base::WeakPtr<ClientSessionControl>& client_session_control)
      override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DisconnectWindowAura);
};

DisconnectWindowAura::DisconnectWindowAura() {}

DisconnectWindowAura::~DisconnectWindowAura() {
  ash::WmShell::Get()->system_tray_notifier()->NotifyScreenShareStop();
}

void DisconnectWindowAura::Start(
    const base::WeakPtr<ClientSessionControl>& client_session_control) {
  // TODO(kelvinp): Clean up the NotifyScreenShareStart interface when we
  // completely retire Hangout Remote Desktop v1.
  base::string16 helper_name;
  ash::WmShell::Get()->system_tray_notifier()->NotifyScreenShareStart(
      base::Bind(&ClientSessionControl::DisconnectSession,
                 client_session_control, protocol::OK),
      helper_name);
}

}  // namespace

// static
std::unique_ptr<HostWindow> HostWindow::CreateDisconnectWindow() {
  return base::MakeUnique<DisconnectWindowAura>();
}

}  // namespace remoting
