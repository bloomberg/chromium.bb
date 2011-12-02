// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOCAL_INPUT_MONITOR_THREAD_WIN_H_
#define LOCAL_INPUT_MONITOR_THREAD_WIN_H_

#include <set>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/threading/simple_thread.h"
#include "remoting/host/chromoting_host.h"

namespace remoting {

class LocalInputMonitorThread : public base::SimpleThread {
 public:
  static void AddHostToInputMonitor(ChromotingHost* host);
  static void RemoveHostFromInputMonitor(ChromotingHost* host);

 private:
  LocalInputMonitorThread();
  virtual ~LocalInputMonitorThread();

  void AddHost(ChromotingHost* host);
  bool RemoveHost(ChromotingHost* host);

  void Stop();
  virtual void Run() OVERRIDE;  // Overridden from SimpleThread.

  void LocalMouseMoved(const SkIPoint& mouse_position);
  static LRESULT WINAPI HandleLowLevelMouseEvent(int code,
                                                 WPARAM event_type,
                                                 LPARAM event_data);

  base::Lock hosts_lock_;
  typedef std::set<scoped_refptr<ChromotingHost> > ChromotingHosts;
  ChromotingHosts hosts_;

  DISALLOW_COPY_AND_ASSIGN(LocalInputMonitorThread);
};

}  // namespace remoting

#endif
