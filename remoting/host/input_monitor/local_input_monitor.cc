// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_monitor/local_input_monitor.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/input_monitor/local_hotkey_input_monitor.h"
#include "remoting/host/input_monitor/local_mouse_input_monitor.h"

namespace remoting {
namespace {

class LocalInputMonitorImpl : public LocalInputMonitor {
 public:
  LocalInputMonitorImpl(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      base::WeakPtr<ClientSessionControl> client_session_control);
  ~LocalInputMonitorImpl() override;

 private:
  std::unique_ptr<LocalHotkeyInputMonitor> hotkey_input_monitor_;
  std::unique_ptr<LocalMouseInputMonitor> mouse_input_monitor_;

  DISALLOW_COPY_AND_ASSIGN(LocalInputMonitorImpl);
};

LocalInputMonitorImpl::LocalInputMonitorImpl(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control)
    : hotkey_input_monitor_(LocalHotkeyInputMonitor::Create(
          caller_task_runner,
          input_task_runner,
          ui_task_runner,
          base::BindOnce(&ClientSessionControl::DisconnectSession,
                         client_session_control,
                         protocol::OK))),
      mouse_input_monitor_(LocalMouseInputMonitor::Create(
          caller_task_runner,
          input_task_runner,
          ui_task_runner,
          base::BindRepeating(&ClientSessionControl::OnLocalMouseMoved,
                              client_session_control),
          base::BindOnce(&ClientSessionControl::DisconnectSession,
                         client_session_control,
                         protocol::OK))) {}

LocalInputMonitorImpl::~LocalInputMonitorImpl() = default;

}  // namespace

std::unique_ptr<LocalInputMonitor> LocalInputMonitor::Create(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control) {
  return std::make_unique<LocalInputMonitorImpl>(
      caller_task_runner, input_task_runner, ui_task_runner,
      client_session_control);
}

}  // namespace remoting
