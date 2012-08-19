// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/daemon_process.h"

#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/win/scoped_handle.h"

namespace remoting {

class DaemonProcessWin : public DaemonProcess {
 public:
  DaemonProcessWin(scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
                   scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
                   const base::Closure& stopped_callback);
  virtual ~DaemonProcessWin();

  // DaemonProcess implementation.
  virtual bool LaunchNetworkProcess() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DaemonProcessWin);
};

DaemonProcessWin::DaemonProcessWin(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    const base::Closure& stopped_callback)
    : DaemonProcess(main_task_runner, io_task_runner, stopped_callback) {
}

DaemonProcessWin::~DaemonProcessWin() {
}

bool DaemonProcessWin::LaunchNetworkProcess() {
  NOTIMPLEMENTED();
  return false;
}

scoped_ptr<DaemonProcess> DaemonProcess::Create(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    const base::Closure& stopped_callback) {
  scoped_ptr<DaemonProcessWin> daemon_process(
      new DaemonProcessWin(main_task_runner, io_task_runner, stopped_callback));
  return daemon_process.PassAs<DaemonProcess>();
}

}  // namespace remoting
