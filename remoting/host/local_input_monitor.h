// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LOCAL_INPUT_MONITOR_H_
#define REMOTING_HOST_LOCAL_INPUT_MONITOR_H_

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

class MouseMoveObserver;

class LocalInputMonitor {
 public:
  virtual ~LocalInputMonitor() {}

  // Starts local input monitoring. This class is also responsible for
  // of catching the disconnection keyboard shortcut on Mac and Linux
  // (Ctlr-Alt-Esc). The |disconnect_callback| is called when this key
  // combination is pressed.
  //
  // TODO(sergeyu): Refactor shortcut code to a separate class.
  virtual void Start(MouseMoveObserver* mouse_move_observer,
                     const base::Closure& disconnect_callback) = 0;
  virtual void Stop() = 0;

  static scoped_ptr<LocalInputMonitor> Create(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);
};

}  // namespace remoting

#endif  // REMOTING_HOST_LOCAL_INPUT_MONITOR_H_
