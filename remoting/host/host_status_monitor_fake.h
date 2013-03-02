// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_STATUS_MONITOR_FAKE_H_
#define REMOTING_HOST_HOST_STATUS_MONITOR_FAKE_H_

#include "base/memory/weak_ptr.h"
#include "remoting/host/host_status_monitor.h"

namespace remoting {

// An implementation of HostStatusMonitor that does nothing.
class HostStatusMonitorFake
    : public base::SupportsWeakPtr<HostStatusMonitorFake>,
      public HostStatusMonitor {
 public:
  virtual ~HostStatusMonitorFake() {}

  // HostStatusMonitor interface.
  virtual void AddStatusObserver(HostStatusObserver* observer) OVERRIDE {}
  virtual void RemoveStatusObserver(HostStatusObserver* observer) OVERRIDE {}
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_STATUS_MONITOR_FAKE_H_
