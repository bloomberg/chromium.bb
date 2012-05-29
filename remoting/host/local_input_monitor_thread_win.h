// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOCAL_INPUT_MONITOR_THREAD_WIN_H_
#define LOCAL_INPUT_MONITOR_THREAD_WIN_H_

#include <set>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/threading/simple_thread.h"

struct SkIPoint;

namespace remoting {

class MouseMoveObserver;

class LocalInputMonitorThread : public base::SimpleThread {
 public:
  static void AddMouseMoveObserver(MouseMoveObserver* mouse_move_observer);
  static void RemoveMouseMoveObserver(MouseMoveObserver* mouse_move_observer);

 private:
  LocalInputMonitorThread();
  virtual ~LocalInputMonitorThread();

  void AddObserver(MouseMoveObserver* mouse_move_observer);
  bool RemoveObserver(MouseMoveObserver* mouse_move_observer);

  void Stop();
  virtual void Run() OVERRIDE;  // Overridden from SimpleThread.

  void LocalMouseMoved(const SkIPoint& mouse_position);
  static LRESULT WINAPI HandleLowLevelMouseEvent(int code,
                                                 WPARAM event_type,
                                                 LPARAM event_data);

  base::Lock lock_;
  typedef std::set<MouseMoveObserver*> MouseMoveObservers;
  MouseMoveObservers observers_;

  DISALLOW_COPY_AND_ASSIGN(LocalInputMonitorThread);
};

}  // namespace remoting

#endif
