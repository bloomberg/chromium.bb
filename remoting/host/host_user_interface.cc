// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_user_interface.h"

#include "base/bind.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/disconnect_window.h"
#include "remoting/host/local_input_monitor.h"

namespace remoting {

HostUserInterface::HostUserInterface(ChromotingHostContext* context)
    : host_(NULL),
      context_(context),
      is_monitoring_local_inputs_(false),
      ui_thread_proxy_(context->ui_message_loop()) {
}

HostUserInterface::~HostUserInterface() {
  DCHECK(ui_message_loop()->BelongsToCurrentThread());

  MonitorLocalInputs(false);
  ShowDisconnectWindow(false, std::string());

  ui_thread_proxy_.Detach();
}

void HostUserInterface::Start(ChromotingHost* host,
                              const base::Closure& disconnect_callback) {
  DCHECK(network_message_loop()->BelongsToCurrentThread());
  DCHECK(host_ == NULL);
  DCHECK(disconnect_callback_.is_null());

  host_ = host;
  disconnect_callback_ = disconnect_callback;
  disconnect_window_ = DisconnectWindow::Create();
  local_input_monitor_ = LocalInputMonitor::Create();
  host_->AddStatusObserver(this);
}

void HostUserInterface::OnClientAuthenticated(const std::string& jid) {
  authenticated_jid_ = jid;

  std::string username = jid.substr(0, jid.find('/'));
  ui_thread_proxy_.PostTask(FROM_HERE, base::Bind(
      &HostUserInterface::ProcessOnClientAuthenticated,
      base::Unretained(this), username));
}

void HostUserInterface::OnClientDisconnected(const std::string& jid) {
  if (jid == authenticated_jid_) {
    ui_thread_proxy_.PostTask(FROM_HERE, base::Bind(
        &HostUserInterface::ProcessOnClientDisconnected,
        base::Unretained(this)));
  }
}

void HostUserInterface::OnAccessDenied(const std::string& jid) {
}

void HostUserInterface::OnShutdown() {
  // Host status observers must be removed on the network thread, so
  // it must happen here instead of in the destructor.
  host_->RemoveStatusObserver(this);
  host_ = NULL;
  disconnect_callback_ = base::Closure();
}

void HostUserInterface::OnDisconnectCallback() {
  DCHECK(ui_message_loop()->BelongsToCurrentThread());
  DCHECK(!disconnect_callback_.is_null());

  MonitorLocalInputs(false);
  ShowDisconnectWindow(false, std::string());
  disconnect_callback_.Run();
}

base::MessageLoopProxy* HostUserInterface::network_message_loop() const {
  return context_->network_message_loop();
}
base::MessageLoopProxy* HostUserInterface::ui_message_loop() const {
  return context_->ui_message_loop();
}

void HostUserInterface::DisconnectSession() const {
  return disconnect_callback_.Run();
}

void HostUserInterface::ProcessOnClientAuthenticated(
    const std::string& username) {
  DCHECK(ui_message_loop()->BelongsToCurrentThread());

  MonitorLocalInputs(true);
  ShowDisconnectWindow(true, username);
}

void HostUserInterface::ProcessOnClientDisconnected() {
  DCHECK(ui_message_loop()->BelongsToCurrentThread());

  MonitorLocalInputs(false);
  ShowDisconnectWindow(false, std::string());
}

void HostUserInterface::StartForTest(
    ChromotingHost* host,
    const base::Closure& disconnect_callback,
    scoped_ptr<DisconnectWindow> disconnect_window,
    scoped_ptr<LocalInputMonitor> local_input_monitor) {
  DCHECK(network_message_loop()->BelongsToCurrentThread());
  DCHECK(host_ == NULL);
  DCHECK(disconnect_callback_.is_null());

  host_ = host;
  disconnect_callback_ = disconnect_callback;
  disconnect_window_ = disconnect_window.Pass();
  local_input_monitor_ = local_input_monitor.Pass();
}

void HostUserInterface::MonitorLocalInputs(bool enable) {
  DCHECK(ui_message_loop()->BelongsToCurrentThread());

  if (enable != is_monitoring_local_inputs_) {
    if (enable) {
      local_input_monitor_->Start(host_);
    } else {
      local_input_monitor_->Stop();
    }
    is_monitoring_local_inputs_ = enable;
  }
}

void HostUserInterface::ShowDisconnectWindow(bool show,
                                             const std::string& username) {
  DCHECK(ui_message_loop()->BelongsToCurrentThread());

  if (show) {
    disconnect_window_->Show(
        host_,
        base::Bind(&HostUserInterface::OnDisconnectCallback,
                   base::Unretained(this)),
        username);
  } else {
    disconnect_window_->Hide();
  }
}

}  // namespace remoting
