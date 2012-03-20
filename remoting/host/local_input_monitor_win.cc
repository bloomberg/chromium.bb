// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/local_input_monitor.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/local_input_monitor_thread_win.h"

namespace remoting {

namespace {

class LocalInputMonitorWin : public LocalInputMonitor {
 public:
  LocalInputMonitorWin();
  ~LocalInputMonitorWin();

  virtual void Start(ChromotingHost* host) OVERRIDE;
  virtual void Stop() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(LocalInputMonitorWin);

  ChromotingHost* chromoting_host_;
};

LocalInputMonitorWin::LocalInputMonitorWin() : chromoting_host_(NULL) {
}

LocalInputMonitorWin::~LocalInputMonitorWin() {
  DCHECK(chromoting_host_ == NULL);
}

void LocalInputMonitorWin::Start(ChromotingHost* host) {
  DCHECK(chromoting_host_ == NULL);
  chromoting_host_ = host;
  LocalInputMonitorThread::AddHostToInputMonitor(chromoting_host_);
}

void LocalInputMonitorWin::Stop() {
  DCHECK(chromoting_host_ != NULL);
  LocalInputMonitorThread::RemoveHostFromInputMonitor(chromoting_host_);
  chromoting_host_ = NULL;
}

}  // namespace

scoped_ptr<LocalInputMonitor> LocalInputMonitor::Create() {
  return scoped_ptr<LocalInputMonitor>(new LocalInputMonitorWin());
}

}  // namespace remoting
