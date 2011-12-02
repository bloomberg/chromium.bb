// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me_host_user_interface.h"

#include "base/bind.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/continue_window.h"
#include "remoting/host/disconnect_window.h"
#include "remoting/host/local_input_monitor.h"

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


It2MeHostUserInterface::It2MeHostUserInterface(ChromotingHost* host,
                                               ChromotingHostContext* context)
    : host_(host),
      context_(context),
      is_monitoring_local_inputs_(false),
      ui_thread_proxy_(context->ui_message_loop()) {
}

It2MeHostUserInterface::~It2MeHostUserInterface() {
}

void It2MeHostUserInterface::Init() {
  InitFrom(DisconnectWindow::Create(),
           ContinueWindow::Create(),
           LocalInputMonitor::Create());
}

void It2MeHostUserInterface::InitFrom(DisconnectWindow* disconnect_window,
                                      ContinueWindow* continue_window,
                                      LocalInputMonitor* monitor) {
  disconnect_window_.reset(disconnect_window);
  continue_window_.reset(continue_window);
  local_input_monitor_.reset(monitor);
}

void It2MeHostUserInterface::OnSignallingConnected(
    SignalStrategy* signal_strategy, const std::string& full_jid) {
}

void It2MeHostUserInterface::OnSignallingDisconnected() {
}

void It2MeHostUserInterface::OnClientAuthenticated(const std::string& jid) {
  // There should not be more than one concurrent authenticated connection.
  DCHECK(authenticated_jid_.empty());
  authenticated_jid_ = jid;

  std::string username = jid.substr(0, jid.find('/'));
  ui_thread_proxy_.PostTask(FROM_HERE, base::Bind(
    &It2MeHostUserInterface::ProcessOnClientAuthenticated,
    base::Unretained(this),
    username));
}

void It2MeHostUserInterface::OnClientDisconnected(const std::string& jid) {
  if (jid == authenticated_jid_) {
    authenticated_jid_.clear();
    ui_thread_proxy_.PostTask(FROM_HERE, base::Bind(
        &It2MeHostUserInterface::ProcessOnClientDisconnected,
        base::Unretained(this)));
  }
}

void It2MeHostUserInterface::OnAccessDenied() {
}

void It2MeHostUserInterface::OnShutdown() {
}

void It2MeHostUserInterface::Shutdown() {
  DCHECK(context_->ui_message_loop()->BelongsToCurrentThread());

  MonitorLocalInputs(false);
  ShowDisconnectWindow(false, std::string());
  ShowContinueWindow(false);
  StartContinueWindowTimer(false);

  ui_thread_proxy_.Detach();
}

void It2MeHostUserInterface::ProcessOnClientAuthenticated(
    const std::string& username) {
  DCHECK(context_->ui_message_loop()->BelongsToCurrentThread());

  MonitorLocalInputs(true);
  ShowDisconnectWindow(true, username);
  StartContinueWindowTimer(true);
}

void It2MeHostUserInterface::ProcessOnClientDisconnected() {
  DCHECK(context_->ui_message_loop()->BelongsToCurrentThread());

  MonitorLocalInputs(false);
  ShowDisconnectWindow(false, std::string());
  ShowContinueWindow(false);
  StartContinueWindowTimer(false);
}

void It2MeHostUserInterface::MonitorLocalInputs(bool enable) {
  DCHECK(context_->ui_message_loop()->BelongsToCurrentThread());

  if (enable == is_monitoring_local_inputs_)
    return;
  if (enable) {
    local_input_monitor_->Start(host_);
  } else {
    local_input_monitor_->Stop();
  }
  is_monitoring_local_inputs_ = enable;
}

void It2MeHostUserInterface::ShowDisconnectWindow(bool show,
                                                  const std::string& username) {
  DCHECK(context_->ui_message_loop()->BelongsToCurrentThread());

  if (show) {
    disconnect_window_->Show(host_, username);
  } else {
    disconnect_window_->Hide();
  }
}

void It2MeHostUserInterface::ShowContinueWindow(bool show) {
  DCHECK(context_->ui_message_loop()->BelongsToCurrentThread());

  if (show) {
    continue_window_->Show(host_, base::Bind(
        &It2MeHostUserInterface::ContinueSession, base::Unretained(this)));
  } else {
    continue_window_->Hide();
  }
}

void It2MeHostUserInterface::ContinueSession(bool continue_session) {
  DCHECK(context_->ui_message_loop()->BelongsToCurrentThread());

  if (continue_session) {
    host_->PauseSession(false);
    timer_task_.reset();
    StartContinueWindowTimer(true);
  } else {
    host_->Shutdown(base::Closure());
  }
}

void It2MeHostUserInterface::StartContinueWindowTimer(bool start) {
  DCHECK(context_->ui_message_loop()->BelongsToCurrentThread());

  if (start) {
    timer_task_.reset(new TimerTask(
        context_->ui_message_loop(),
        base::Bind(&It2MeHostUserInterface::OnContinueWindowTimer,
                   base::Unretained(this)),
        kContinueWindowShowTimeoutMs));
  } else {
    timer_task_.reset();
  }
}

void It2MeHostUserInterface::OnContinueWindowTimer() {
  DCHECK(context_->ui_message_loop()->BelongsToCurrentThread());

  host_->PauseSession(true);
  ShowContinueWindow(true);

  timer_task_.reset(new TimerTask(
      context_->ui_message_loop(),
      base::Bind(&It2MeHostUserInterface::OnShutdownHostTimer,
                 base::Unretained(this)),
      kContinueWindowHideTimeoutMs));
}

void It2MeHostUserInterface::OnShutdownHostTimer() {
  DCHECK(context_->ui_message_loop()->BelongsToCurrentThread());

  ShowContinueWindow(false);
  host_->Shutdown(base::Closure());
}

}  // namespace remoting
