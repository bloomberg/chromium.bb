// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_BROWSER_WATCHER_EXIT_CODE_WATCHER_WIN_H_
#define COMPONENTS_BROWSER_WATCHER_EXIT_CODE_WATCHER_WIN_H_

#include "base/macros.h"
#include "base/process/process.h"
#include "base/threading/thread.h"
#include "base/win/scoped_handle.h"

namespace browser_watcher {

// Watches for the exit code of a process and records
class ExitCodeWatcher {
 public:
  ExitCodeWatcher();
  ~ExitCodeWatcher();

  // This function expects |process| to be open with sufficient privilege to
  // wait and retrieve the process exit code.
  // It checks the handle for validity and takes ownership of it.
  bool Initialize(base::Process process);

  bool StartWatching();

  void StopWatching();

  const base::Process& process() const { return process_; }
  int exit_code() const { return exit_code_; }

 private:
  // Writes |exit_code| to registry, returns true on success.
  bool WriteProcessExitCode(int exit_code);

  // Waits for the process to exit and records its exit code in registry.
  // This is a blocking call.
  void WaitForExit();

  // Watched process and its creation time.
  base::Process process_;

  // The thread that runs WaitForExit().
  base::Thread background_thread_;

  // The exit code of the watched process. Valid after WaitForExit.
  int exit_code_;

  // Event handle to use to stop exit watcher thread
  base::win::ScopedHandle stop_watching_handle_;

  DISALLOW_COPY_AND_ASSIGN(ExitCodeWatcher);
};

}  // namespace browser_watcher

#endif  // COMPONENTS_BROWSER_WATCHER_EXIT_CODE_WATCHER_WIN_H_
