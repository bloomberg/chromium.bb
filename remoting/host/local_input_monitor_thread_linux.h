// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOCAL_INPUT_MONITOR_THREAD_LINUX_H_
#define LOCAL_INPUT_MONITOR_THREAD_LINUX_H_

#include "base/threading/simple_thread.h"
#include "ui/gfx/point.h"

typedef struct _XDisplay Display;

namespace remoting {

class ChromotingHost;

class LocalInputMonitorThread : public base::SimpleThread {
 public:
  explicit LocalInputMonitorThread(ChromotingHost* host);
  virtual ~LocalInputMonitorThread();

  void Stop();
  virtual void Run();

  void LocalMouseMoved(const gfx::Point& pos);
  void LocalKeyPressed(int key_code, bool down);

 private:
  ChromotingHost* host_;
  int wakeup_pipe_[2];
  Display* display_;
  bool alt_pressed_;
  bool ctrl_pressed_;

  DISALLOW_COPY_AND_ASSIGN(LocalInputMonitorThread);
};

}  // namespace remoting

#endif
