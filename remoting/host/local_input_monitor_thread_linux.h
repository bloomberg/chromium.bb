// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOCAL_INPUT_MONITOR_THREAD_LINUX_H_
#define LOCAL_INPUT_MONITOR_THREAD_LINUX_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/threading/simple_thread.h"
#include "remoting/host/local_input_monitor.h"

typedef struct _XDisplay Display;

struct SkIPoint;

namespace remoting {

class MouseMoveObserver;

class LocalInputMonitorThread : public base::SimpleThread {
 public:
  LocalInputMonitorThread(
      MouseMoveObserver* mouse_move_observer,
      const base::Closure& disconnect_callback);
  virtual ~LocalInputMonitorThread();

  void Stop();
  virtual void Run() OVERRIDE;

  void LocalMouseMoved(const SkIPoint& pos);
  void LocalKeyPressed(int key_code, bool down);

 private:
  MouseMoveObserver* mouse_move_observer_;
  base::Closure disconnect_callback_;
  int wakeup_pipe_[2];
  Display* display_;
  bool alt_pressed_;
  bool ctrl_pressed_;

  DISALLOW_COPY_AND_ASSIGN(LocalInputMonitorThread);
};

}  // namespace remoting

#endif
