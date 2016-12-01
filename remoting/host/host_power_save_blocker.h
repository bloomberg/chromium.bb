// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_POWER_SAVER_BLOCKER_H_
#define REMOTING_HOST_HOST_POWER_SAVER_BLOCKER_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "device/power_save_blocker/power_save_blocker.h"
#include "remoting/host/host_status_observer.h"

namespace base {

class SingleThreadTaskRunner;

}  // namespace base

namespace remoting {

class HostStatusMonitor;

// A HostStatusObserver to block screen saver from taking effect during the
// lifetime of a remoting connection.
class HostPowerSaveBlocker : public HostStatusObserver {
 public:
  HostPowerSaveBlocker(
      base::WeakPtr<HostStatusMonitor> monitor,
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& file_task_runner);

  ~HostPowerSaveBlocker() override;

  void OnClientConnected(const std::string& jid) override;
  void OnClientDisconnected(const std::string& jid) override;

 private:
  friend class HostPowerSaveBlockerTest;

  base::WeakPtr<HostStatusMonitor> monitor_;
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;
  std::unique_ptr<device::PowerSaveBlocker> blocker_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_POWER_SAVER_BLOCKER_H_
