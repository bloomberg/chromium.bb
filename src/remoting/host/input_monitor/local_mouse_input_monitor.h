// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INPUT_MONITOR_LOCAL_MOUSE_INPUT_MONITOR_H_
#define REMOTING_HOST_INPUT_MONITOR_LOCAL_MOUSE_INPUT_MONITOR_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "remoting/host/input_monitor/local_input_monitor.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

// Monitors the local input and sends a notification via the callback passed in
// for every mouse move event received.
class LocalMouseInputMonitor {
 public:
  virtual ~LocalMouseInputMonitor() = default;

  // Creates a platform-specific instance of LocalMouseInputMonitor.
  // Callbacks are called on the |caller_task_runner| thread.
  // |on_mouse_move| is called for each mouse movement detected.
  // |disconnect_callback| is called if monitoring cannot be started.
  static std::unique_ptr<LocalMouseInputMonitor> Create(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      LocalInputMonitor::MouseMoveCallback mouse_move_callback,
      base::OnceClosure disconnect_callback);

 protected:
  LocalMouseInputMonitor() = default;
};

}  // namespace remoting

#endif  // REMOTING_HOST_INPUT_MONITOR_LOCAL_MOUSE_INPUT_MONITOR_H_
