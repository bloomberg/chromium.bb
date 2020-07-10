// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_power_save_blocker.h"

#include <utility>

#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "remoting/host/host_status_monitor.h"
#include "services/device/public/mojom/wake_lock.mojom.h"

namespace remoting {

HostPowerSaveBlocker::HostPowerSaveBlocker(
    scoped_refptr<HostStatusMonitor> monitor,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& file_task_runner)
    : monitor_(monitor),
      ui_task_runner_(ui_task_runner),
      file_task_runner_(file_task_runner) {
  DCHECK(monitor_);
  monitor_->AddStatusObserver(this);
}

HostPowerSaveBlocker::~HostPowerSaveBlocker() {
  monitor_->RemoveStatusObserver(this);
}

void HostPowerSaveBlocker::OnClientConnected(const std::string& jid) {
  blocker_.reset(new device::PowerSaveBlocker(
      device::mojom::WakeLockType::kPreventDisplaySleep,
      device::mojom::WakeLockReason::kOther, "Remoting session is active",
      ui_task_runner_, file_task_runner_));
}

void HostPowerSaveBlocker::OnClientDisconnected(const std::string& jid) {
  blocker_.reset();
}

}  // namespace remoting
