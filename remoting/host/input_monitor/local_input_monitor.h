// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INPUT_MONITOR_LOCAL_INPUT_MONITOR_H_
#define REMOTING_HOST_INPUT_MONITOR_LOCAL_INPUT_MONITOR_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

class ClientSessionControl;

// Monitors local input and sends notifications for mouse movements and
// keyboard shortcuts when they are detected.
class LocalInputMonitor {
 public:
  virtual ~LocalInputMonitor() = default;

  // Creates a platform-specific instance of LocalInputMonitor.
  // |client_session_control| is called on the |caller_task_runner| thread.
  static std::unique_ptr<LocalInputMonitor> Create(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      base::WeakPtr<ClientSessionControl> client_session_control);

 protected:
  LocalInputMonitor() = default;
};

}  // namespace remoting

#endif  // REMOTING_HOST_INPUT_MONITOR_LOCAL_INPUT_MONITOR_H_
