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

  virtual void Start(MouseMoveObserver* mouse_move_observer,
                     const base::Closure& disconnect_callback) OVERRIDE;
  virtual void Stop() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(LocalInputMonitorWin);

  MouseMoveObserver* mouse_move_observer_;
};

LocalInputMonitorWin::LocalInputMonitorWin()
    : mouse_move_observer_(NULL) {
}

LocalInputMonitorWin::~LocalInputMonitorWin() {
  DCHECK(!mouse_move_observer_);
}

void LocalInputMonitorWin::Start(MouseMoveObserver* mouse_move_observer,
                                 const base::Closure& disconnect_callback) {
  DCHECK(!mouse_move_observer_);
  mouse_move_observer_ = mouse_move_observer;
  LocalInputMonitorThread::AddMouseMoveObserver(mouse_move_observer_);
}

void LocalInputMonitorWin::Stop() {
  DCHECK(mouse_move_observer_);
  LocalInputMonitorThread::RemoveMouseMoveObserver(mouse_move_observer_);
  mouse_move_observer_ = NULL;
}

}  // namespace

scoped_ptr<LocalInputMonitor> LocalInputMonitor::Create() {
  return scoped_ptr<LocalInputMonitor>(new LocalInputMonitorWin());
}

}  // namespace remoting
