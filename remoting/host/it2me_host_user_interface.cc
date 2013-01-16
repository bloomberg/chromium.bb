// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me_host_user_interface.h"

#include "base/bind.h"
#include "remoting/host/chromoting_host.h"
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

It2MeHostUserInterface::It2MeHostUserInterface(
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    const UiStrings& ui_strings)
    : HostUserInterface(network_task_runner, ui_task_runner, ui_strings),
      ALLOW_THIS_IN_INITIALIZER_LIST(timer_weak_factory_(this)) {
  DCHECK(ui_task_runner->BelongsToCurrentThread());
}

It2MeHostUserInterface::~It2MeHostUserInterface() {
  DCHECK(ui_task_runner()->BelongsToCurrentThread());

  ShowContinueWindow(false);
}

void It2MeHostUserInterface::Init() {
  DCHECK(ui_task_runner()->BelongsToCurrentThread());

  HostUserInterface::Init();
  continue_window_ = ContinueWindow::Create(&ui_strings());
}

void It2MeHostUserInterface::ProcessOnClientAuthenticated(
    const std::string& username) {
  DCHECK(ui_task_runner()->BelongsToCurrentThread());

  HostUserInterface::ProcessOnClientAuthenticated(username);
  StartContinueWindowTimer(true);
}

void It2MeHostUserInterface::ProcessOnClientDisconnected() {
  DCHECK(ui_task_runner()->BelongsToCurrentThread());

  HostUserInterface::ProcessOnClientDisconnected();
  ShowContinueWindow(false);
  StartContinueWindowTimer(false);
}

void It2MeHostUserInterface::ContinueSession(bool continue_session) {
  DCHECK(ui_task_runner()->BelongsToCurrentThread());

  if (continue_session) {
    get_host()->PauseSession(false);
    StartContinueWindowTimer(true);
  } else {
    DisconnectSession();
  }
}

void It2MeHostUserInterface::OnContinueWindowTimer() {
  DCHECK(ui_task_runner()->BelongsToCurrentThread());

  get_host()->PauseSession(true);
  ShowContinueWindow(true);

  // Cancel any pending timer and post one to hide the continue window.
  timer_weak_factory_.InvalidateWeakPtrs();
  ui_task_runner()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&It2MeHostUserInterface::OnShutdownHostTimer,
                 timer_weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kContinueWindowHideTimeoutMs));
}

void It2MeHostUserInterface::OnShutdownHostTimer() {
  DCHECK(ui_task_runner()->BelongsToCurrentThread());

  ShowContinueWindow(false);
  DisconnectSession();
}

void It2MeHostUserInterface::ShowContinueWindow(bool show) {
  DCHECK(ui_task_runner()->BelongsToCurrentThread());

  if (show) {
    continue_window_->Show(base::Bind(
        &It2MeHostUserInterface::ContinueSession, base::Unretained(this)));
  } else {
    continue_window_->Hide();
  }
}

void It2MeHostUserInterface::StartContinueWindowTimer(bool start) {
  DCHECK(ui_task_runner()->BelongsToCurrentThread());

  // Abandon previous timer events by invalidating their weak pointer to us.
  timer_weak_factory_.InvalidateWeakPtrs();
  if (start) {
    ui_task_runner()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&It2MeHostUserInterface::OnContinueWindowTimer,
                   timer_weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(kContinueWindowShowTimeoutMs));
  }
}

}  // namespace remoting
