// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me_host_user_interface.h"

#include "base/bind.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/continue_window.h"
#include "remoting/host/disconnect_window.h"
#include "remoting/host/local_input_monitor.h"

namespace {

// Milliseconds before the continue window is shown.
static const int kContinueWindowShowTimeoutMs = 10 * 60 * 1000;

// Milliseconds before the continue window is automatically dismissed and
// the connection is closed.
static const int kContinueWindowHideTimeoutMs = 60 * 1000;

}  // namespace

namespace remoting {

class It2MeHostUserInterface::TimerTask {
 public:
  TimerTask(base::MessageLoopProxy* message_loop,
            const base::Closure& task,
            int delay_ms)
      : thread_proxy_(message_loop) {
    thread_proxy_.PostDelayedTask(FROM_HERE, task, delay_ms);
  }

 private:
  ScopedThreadProxy thread_proxy_;
};


It2MeHostUserInterface::It2MeHostUserInterface(ChromotingHostContext* context)
    : HostUserInterface(context) {
}

It2MeHostUserInterface::~It2MeHostUserInterface() {
  DCHECK(ui_message_loop()->BelongsToCurrentThread());

  ShowContinueWindow(false);
  StartContinueWindowTimer(false);
}

void It2MeHostUserInterface::Start(ChromotingHost* host,
                                   const base::Closure& disconnect_callback) {
  DCHECK(network_message_loop()->BelongsToCurrentThread());

  HostUserInterface::Start(host, disconnect_callback);
  continue_window_ = ContinueWindow::Create();
}

void It2MeHostUserInterface::OnClientAuthenticated(const std::string& jid) {
  if (!get_authenticated_jid().empty()) {
    // If we already authenticated another client then one of the
    // connections may be an attacker, so both are suspect and we have
    // to reject the second connection and shutdown the host.
    get_host()->RejectAuthenticatingClient();
    network_message_loop()->PostTask(FROM_HERE, base::Bind(
        &ChromotingHost::Shutdown, get_host(), base::Closure()));
  } else {
    HostUserInterface::OnClientAuthenticated(jid);
  }
}

void It2MeHostUserInterface::ProcessOnClientAuthenticated(
    const std::string& username) {
  DCHECK(ui_message_loop()->BelongsToCurrentThread());

  HostUserInterface::ProcessOnClientAuthenticated(username);
  StartContinueWindowTimer(true);
}

void It2MeHostUserInterface::ProcessOnClientDisconnected() {
  DCHECK(ui_message_loop()->BelongsToCurrentThread());

  HostUserInterface::ProcessOnClientDisconnected();
  ShowContinueWindow(false);
  StartContinueWindowTimer(false);
}

void It2MeHostUserInterface::StartForTest(
    ChromotingHost* host,
    const base::Closure& disconnect_callback,
    scoped_ptr<DisconnectWindow> disconnect_window,
    scoped_ptr<ContinueWindow> continue_window,
    scoped_ptr<LocalInputMonitor> local_input_monitor) {
  HostUserInterface::StartForTest(host, disconnect_callback,
                                  disconnect_window.Pass(),
                                  local_input_monitor.Pass());
  continue_window_ = continue_window.Pass();
}

void It2MeHostUserInterface::ContinueSession(bool continue_session) {
  DCHECK(ui_message_loop()->BelongsToCurrentThread());

  if (continue_session) {
    get_host()->PauseSession(false);
    timer_task_.reset();
    StartContinueWindowTimer(true);
  } else {
    DisconnectSession();
  }
}

void It2MeHostUserInterface::OnContinueWindowTimer() {
  DCHECK(ui_message_loop()->BelongsToCurrentThread());

  get_host()->PauseSession(true);
  ShowContinueWindow(true);

  timer_task_.reset(new TimerTask(
      ui_message_loop(),
      base::Bind(&It2MeHostUserInterface::OnShutdownHostTimer,
                 base::Unretained(this)),
      kContinueWindowHideTimeoutMs));
}

void It2MeHostUserInterface::OnShutdownHostTimer() {
  DCHECK(ui_message_loop()->BelongsToCurrentThread());

  ShowContinueWindow(false);
  DisconnectSession();
}

void It2MeHostUserInterface::ShowContinueWindow(bool show) {
  DCHECK(ui_message_loop()->BelongsToCurrentThread());

  if (show) {
    continue_window_->Show(get_host(), base::Bind(
        &It2MeHostUserInterface::ContinueSession, base::Unretained(this)));
  } else {
    continue_window_->Hide();
  }
}

void It2MeHostUserInterface::StartContinueWindowTimer(bool start) {
  DCHECK(ui_message_loop()->BelongsToCurrentThread());

  if (start) {
    timer_task_.reset(new TimerTask(
        ui_message_loop(),
        base::Bind(&It2MeHostUserInterface::OnContinueWindowTimer,
                   base::Unretained(this)),
        kContinueWindowShowTimeoutMs));
  } else {
    timer_task_.reset();
  }
}

}  // namespace remoting
