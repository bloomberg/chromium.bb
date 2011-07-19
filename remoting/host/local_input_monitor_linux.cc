// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/local_input_monitor.h"
#include "remoting/host/local_input_monitor_thread_linux.h"

#include "base/compiler_specific.h"
#include "base/logging.h"

namespace {

class LocalInputMonitorLinux : public remoting::LocalInputMonitor {
 public:
  LocalInputMonitorLinux();
  ~LocalInputMonitorLinux();

  virtual void Start(remoting::ChromotingHost* host) OVERRIDE;
  virtual void Stop() OVERRIDE;

 private:
  remoting::LocalInputMonitorThread* thread_;
};

}  // namespace


LocalInputMonitorLinux::LocalInputMonitorLinux()
    : thread_(NULL) {
}

LocalInputMonitorLinux::~LocalInputMonitorLinux() {
  CHECK(!thread_);
}

void LocalInputMonitorLinux::Start(remoting::ChromotingHost* host) {
  CHECK(!thread_);
  thread_ = new remoting::LocalInputMonitorThread(host);
  thread_->Start();
}

void LocalInputMonitorLinux::Stop() {
  CHECK(thread_);
  thread_->Stop();
  thread_->Join();
  delete thread_;
  thread_ = 0;
}

remoting::LocalInputMonitor* remoting::LocalInputMonitor::Create() {
  return new LocalInputMonitorLinux;
}
