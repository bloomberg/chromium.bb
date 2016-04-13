// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "remoting/host/local_input_monitor.h"

namespace remoting {

namespace {

class LocalInputMonitorAndroid : public LocalInputMonitor {
 public:
  LocalInputMonitorAndroid() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(LocalInputMonitorAndroid);
};

}  // namespace

std::unique_ptr<LocalInputMonitor> LocalInputMonitor::Create(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control) {
  return base::WrapUnique(new LocalInputMonitorAndroid());
}

}  // namespace remoting
