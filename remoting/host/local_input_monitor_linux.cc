// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/local_input_monitor.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "remoting/host/local_input_monitor_thread_linux.h"

namespace remoting {

namespace {

class LocalInputMonitorLinux : public LocalInputMonitor {
 public:
  LocalInputMonitorLinux();
  virtual ~LocalInputMonitorLinux();

  virtual void Start(MouseMoveObserver* mouse_move_observer,
                     const base::Closure& disconnect_callback) OVERRIDE;
  virtual void Stop() OVERRIDE;

 private:
  LocalInputMonitorThread* thread_;
};

LocalInputMonitorLinux::LocalInputMonitorLinux()
    : thread_(NULL) {
}

LocalInputMonitorLinux::~LocalInputMonitorLinux() {
  CHECK(!thread_);
}

void LocalInputMonitorLinux::Start(
    MouseMoveObserver* mouse_move_observer,
    const base::Closure& disconnect_callback) {
  CHECK(!thread_);
  thread_ = new LocalInputMonitorThread(mouse_move_observer,
                                        disconnect_callback);
  thread_->Start();
}

void LocalInputMonitorLinux::Stop() {
  CHECK(thread_);
  thread_->Stop();
  thread_->Join();
  delete thread_;
  thread_ = 0;
}

}  // namespace

scoped_ptr<LocalInputMonitor> LocalInputMonitor::Create() {
  return scoped_ptr<LocalInputMonitor>(new LocalInputMonitorLinux());
}

}  // namespace remoting
