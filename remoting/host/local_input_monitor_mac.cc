// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/local_input_monitor.h"

#include "base/compiler_specific.h"
#include "base/logging.h"

namespace {

class LocalInputMonitorMac : public remoting::LocalInputMonitor {
 public:
  LocalInputMonitorMac() {}
  virtual void Start(remoting::ChromotingHost* host) OVERRIDE;
  virtual void Stop() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(LocalInputMonitorMac);
};

}  // namespace

void LocalInputMonitorMac::Start(remoting::ChromotingHost* host) {
  NOTIMPLEMENTED();
}

void LocalInputMonitorMac::Stop() {
  NOTIMPLEMENTED();
}

remoting::LocalInputMonitor* remoting::LocalInputMonitor::Create() {
  return new LocalInputMonitorMac;
}
