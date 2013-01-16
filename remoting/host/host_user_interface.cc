// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_user_interface.h"

#include "base/bind.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/disconnect_window.h"
#include "remoting/host/local_input_monitor.h"

namespace remoting {

HostUserInterface::HostUserInterface(
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    const UiStrings& ui_strings)
    : host_(NULL),
      network_task_runner_(network_task_runner),
      ui_task_runner_(ui_task_runner),
      is_monitoring_local_inputs_(false),
      ui_strings_(ui_strings),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      weak_ptr_(weak_factory_.GetWeakPtr()) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
}

HostUserInterface::~HostUserInterface() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  MonitorLocalInputs(false);
  disconnect_window_->Hide();
}

void HostUserInterface::Init() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  disconnect_window_ = DisconnectWindow::Create(&ui_strings());
  local_input_monitor_ = LocalInputMonitor::Create();
}

void HostUserInterface::Start(ChromotingHost* host,
                              const base::Closure& disconnect_callback) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  DCHECK(host_ == NULL);

  host_ = host;
  disconnect_callback_ = disconnect_callback;
  host_->AddStatusObserver(this);
}

void HostUserInterface::OnClientAuthenticated(const std::string& jid) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  authenticated_jid_ = jid;

  std::string username = jid.substr(0, jid.find('/'));
  ui_task_runner_->PostTask(FROM_HERE, base::Bind(
      &HostUserInterface::ProcessOnClientAuthenticated,
      weak_ptr_, username));
}

void HostUserInterface::OnClientDisconnected(const std::string& jid) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  if (jid == authenticated_jid_) {
    ui_task_runner_->PostTask(FROM_HERE, base::Bind(
        &HostUserInterface::ProcessOnClientDisconnected,
        weak_ptr_));
  }
}

void HostUserInterface::OnAccessDenied(const std::string& jid) {
}

void HostUserInterface::OnShutdown() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  // Host status observers must be removed on the network thread, so
  // it must happen here instead of in the destructor.
  host_->RemoveStatusObserver(this);
  host_ = NULL;
}

void HostUserInterface::OnDisconnectCallback() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  MonitorLocalInputs(false);
  disconnect_window_->Hide();
  DisconnectSession();
}

base::SingleThreadTaskRunner* HostUserInterface::network_task_runner() const {
  return network_task_runner_;
}

base::SingleThreadTaskRunner* HostUserInterface::ui_task_runner() const {
  return ui_task_runner_;
}

void HostUserInterface::DisconnectSession() const {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  disconnect_callback_.Run();
}

void HostUserInterface::ProcessOnClientAuthenticated(
    const std::string& username) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  if (!disconnect_window_->Show(
          base::Bind(&HostUserInterface::OnDisconnectCallback, weak_ptr_),
          username)) {
    LOG(ERROR) << "Failed to show the disconnect window.";
    DisconnectSession();
    return;
  }

  MonitorLocalInputs(true);
}

void HostUserInterface::ProcessOnClientDisconnected() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  MonitorLocalInputs(false);
  disconnect_window_->Hide();
}

void HostUserInterface::MonitorLocalInputs(bool enable) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  if (enable != is_monitoring_local_inputs_) {
    if (enable) {
      local_input_monitor_->Start(host_, disconnect_callback_);
    } else {
      local_input_monitor_->Stop();
    }
    is_monitoring_local_inputs_ = enable;
  }
}

}  // namespace remoting
