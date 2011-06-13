// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOCAL_INPUT_MONITOR_THREAD_LINUX_H_
#define LOCAL_INPUT_MONITOR_THREAD_LINUX_H_

#include "base/threading/simple_thread.h"

namespace remoting {

class ChromotingHost;

class LocalInputMonitorThread : public base::SimpleThread {
 public:
  explicit LocalInputMonitorThread(ChromotingHost* host);
  virtual ~LocalInputMonitorThread();

  void Stop();
  virtual void Run();

 private:
  ChromotingHost* host_;
  int wakeup_pipe_[2];

  DISALLOW_COPY_AND_ASSIGN(LocalInputMonitorThread);
};

}  // namespace remoting

#endif
