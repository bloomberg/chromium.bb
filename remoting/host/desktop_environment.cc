// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_environment.h"

#include "remoting/host/capturer.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/continue_window.h"
#include "remoting/host/curtain.h"
#include "remoting/host/disconnect_window.h"
#include "remoting/host/event_executor.h"
#include "remoting/host/local_input_monitor.h"

static const int kContinueWindowTimeoutSecs = 10 * 60;

namespace remoting {

// static
DesktopEnvironment* DesktopEnvironment::Create(ChromotingHostContext* context) {
  Capturer* capturer = Capturer::Create();
  EventExecutor* event_executor =
      EventExecutor::Create(context->desktop_message_loop(), capturer);
  Curtain* curtain = Curtain::Create();
  DisconnectWindow* disconnect_window = DisconnectWindow::Create();
  ContinueWindow* continue_window = ContinueWindow::Create();
  LocalInputMonitor* local_input_monitor = LocalInputMonitor::Create();

  return new DesktopEnvironment(context, capturer, event_executor, curtain,
                                disconnect_window, continue_window,
                                local_input_monitor);
}

DesktopEnvironment::DesktopEnvironment(ChromotingHostContext* context,
                                       Capturer* capturer,
                                       EventExecutor* event_executor,
                                       Curtain* curtain,
                                       DisconnectWindow* disconnect_window,
                                       ContinueWindow* continue_window,
                                       LocalInputMonitor* local_input_monitor)
    : host_(NULL),
      context_(context),
      capturer_(capturer),
      event_executor_(event_executor),
      curtain_(curtain),
      disconnect_window_(disconnect_window),
      continue_window_(continue_window),
      local_input_monitor_(local_input_monitor),
      is_monitoring_local_inputs_(false) {
}

DesktopEnvironment::~DesktopEnvironment() {
}

void DesktopEnvironment::OnConnect(const std::string& username) {
  MonitorLocalInputs(true);
  ShowDisconnectWindow(true, username);
  StartContinueWindowTimer(true);
}

void DesktopEnvironment::OnLastDisconnect() {
  MonitorLocalInputs(false);
  ShowDisconnectWindow(false, std::string());
  ShowContinueWindow(false);
  StartContinueWindowTimer(false);
}

void DesktopEnvironment::OnPause(bool pause) {
  StartContinueWindowTimer(!pause);
}

void DesktopEnvironment::MonitorLocalInputs(bool enable) {
  if (enable == is_monitoring_local_inputs_)
    return;
  if (enable) {
    local_input_monitor_->Start(host_);
  } else {
    local_input_monitor_->Stop();
  }
  is_monitoring_local_inputs_ = enable;
}

void DesktopEnvironment::ShowDisconnectWindow(bool show,
                                              const std::string& username) {
  if (!context_->IsUIThread()) {
    context_->PostToUIThread(
        FROM_HERE,
        NewRunnableMethod(this, &DesktopEnvironment::ShowDisconnectWindow,
                          show, username));
    return;
  }

  if (show) {
    disconnect_window_->Show(host_, username);
  } else {
    disconnect_window_->Hide();
  }
}

void DesktopEnvironment::ShowContinueWindow(bool show) {
  if (!context_->IsUIThread()) {
    context_->PostToUIThread(
        FROM_HERE,
        NewRunnableMethod(this, &DesktopEnvironment::ShowContinueWindow, show));
    return;
  }

  if (show) {
    continue_window_->Show(host_);
  } else {
    continue_window_->Hide();
  }
}

void DesktopEnvironment::StartContinueWindowTimer(bool start) {
  if (context_->main_message_loop() != MessageLoop::current()) {
    context_->main_message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &DesktopEnvironment::StartContinueWindowTimer,
                          start));
    return;
  }
  if (continue_window_timer_.IsRunning() == start)
    return;
  if (start) {
    continue_window_timer_.Start(
        base::TimeDelta::FromSeconds(kContinueWindowTimeoutSecs),
        this, &DesktopEnvironment::ContinueWindowTimerFunc);
  } else {
    continue_window_timer_.Stop();
  }
}

void DesktopEnvironment::ContinueWindowTimerFunc() {
  host_->PauseSession(true);
  ShowContinueWindow(true);
}


}  // namespace remoting
