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
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      weak_ptr_(weak_factory_.GetWeakPtr()) {
  DCHECK(ui_task_runner()->BelongsToCurrentThread());
}

HostUserInterface::~HostUserInterface() {
  DCHECK(ui_task_runner()->BelongsToCurrentThread());

  MonitorLocalInputs(false);
  ShowDisconnectWindow(false, std::string());
}

void HostUserInterface::Start(ChromotingHost* host,
                              const base::Closure& disconnect_callback) {
  DCHECK(network_task_runner()->BelongsToCurrentThread());
  DCHECK(host_ == NULL);
  DCHECK(disconnect_callback_.is_null());

  host_ = host;
  disconnect_callback_ = disconnect_callback;
  disconnect_window_ = DisconnectWindow::Create();
  local_input_monitor_ = LocalInputMonitor::Create();
  host_->AddStatusObserver(this);
}

void HostUserInterface::OnClientAuthenticated(const std::string& jid) {
  DCHECK(network_task_runner()->BelongsToCurrentThread());

  authenticated_jid_ = jid;

  std::string username = jid.substr(0, jid.find('/'));
  ui_task_runner()->PostTask(FROM_HERE, base::Bind(
      &HostUserInterface::ProcessOnClientAuthenticated,
      weak_ptr_, username));
}

void HostUserInterface::OnClientDisconnected(const std::string& jid) {
  DCHECK(network_task_runner()->BelongsToCurrentThread());

  if (jid == authenticated_jid_) {
    ui_task_runner()->PostTask(FROM_HERE, base::Bind(
        &HostUserInterface::ProcessOnClientDisconnected,
        weak_ptr_));
  }
}

void HostUserInterface::OnAccessDenied(const std::string& jid) {
}

void HostUserInterface::OnShutdown() {
  DCHECK(network_task_runner()->BelongsToCurrentThread());

  // Host status observers must be removed on the network thread, so
  // it must happen here instead of in the destructor.
  host_->RemoveStatusObserver(this);
  host_ = NULL;
}

void HostUserInterface::OnDisconnectCallback() {
  DCHECK(ui_task_runner()->BelongsToCurrentThread());

  MonitorLocalInputs(false);
  ShowDisconnectWindow(false, std::string());
  DisconnectSession();
}

base::SingleThreadTaskRunner* HostUserInterface::network_task_runner() const {
  return context_->network_task_runner();
}
base::SingleThreadTaskRunner* HostUserInterface::ui_task_runner() const {
  return context_->ui_task_runner();
}

void HostUserInterface::DisconnectSession() const {
  DCHECK(ui_task_runner()->BelongsToCurrentThread());
  DCHECK(!disconnect_callback_.is_null());

  disconnect_callback_.Run();
}

void HostUserInterface::ProcessOnClientAuthenticated(
    const std::string& username) {
  DCHECK(ui_task_runner()->BelongsToCurrentThread());

  MonitorLocalInputs(true);
  ShowDisconnectWindow(true, username);
}

void HostUserInterface::ProcessOnClientDisconnected() {
  DCHECK(ui_task_runner()->BelongsToCurrentThread());

  MonitorLocalInputs(false);
  ShowDisconnectWindow(false, std::string());
}

void HostUserInterface::StartForTest(
    ChromotingHost* host,
    const base::Closure& disconnect_callback,
    scoped_ptr<DisconnectWindow> disconnect_window,
    scoped_ptr<LocalInputMonitor> local_input_monitor) {
  DCHECK(network_task_runner()->BelongsToCurrentThread());
  DCHECK(host_ == NULL);
  DCHECK(disconnect_callback_.is_null());

  host_ = host;
  disconnect_callback_ = disconnect_callback;
  disconnect_window_ = disconnect_window.Pass();
  local_input_monitor_ = local_input_monitor.Pass();
}

void HostUserInterface::MonitorLocalInputs(bool enable) {
  DCHECK(ui_task_runner()->BelongsToCurrentThread());

  if (enable != is_monitoring_local_inputs_) {
    if (enable) {
      local_input_monitor_->Start(host_, disconnect_callback_);
    } else {
      local_input_monitor_->Stop();
    }
    is_monitoring_local_inputs_ = enable;
  }
}

void HostUserInterface::ShowDisconnectWindow(bool show,
                                             const std::string& username) {
  DCHECK(ui_task_runner()->BelongsToCurrentThread());

  if (show) {
    disconnect_window_->Show(
        host_,
        base::Bind(&HostUserInterface::OnDisconnectCallback, weak_ptr_),
        username);
  } else {
    disconnect_window_->Hide();
  }
}

}  // namespace remoting
