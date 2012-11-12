// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_ANDROID_FORWARDER2_DAEMON_H_
#define TOOLS_ANDROID_FORWARDER2_DAEMON_H_

#include <string>

#include "base/at_exit.h"
#include "base/basictypes.h"

namespace forwarder2 {

class Daemon {
 public:
  // |pid_file_path| is the file path to which the daemon's PID will be written.
  // Note that a lock on the file is also acquired to guarantee that a single
  // instance of daemon is running.
  explicit Daemon(const std::string& pid_file_path);

  // Returns whether the daemon was successfully spawned. Use |is_daemon| to
  // distinguish the parent from the child (daemon) process.
  bool Spawn(bool* is_daemon);

  // Kills the daemon and blocks until it exited.
  bool Kill();

 private:
  const std::string pid_file_path_;
  base::AtExitManager at_exit_manager;

  DISALLOW_COPY_AND_ASSIGN(Daemon);
};

}  // namespace forwarder2

#endif  // TOOLS_ANDROID_FORWARDER2_DAEMON_H_
