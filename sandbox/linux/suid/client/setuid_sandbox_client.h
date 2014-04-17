// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SUID_SETUID_SANDBOX_CLIENT_H_
#define SANDBOX_LINUX_SUID_SETUID_SANDBOX_CLIENT_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "sandbox/linux/sandbox_export.h"

namespace base {
class CommandLine;
class Environment;
struct LaunchOptions;
}

namespace sandbox {

// Helper class to use the setuid sandbox. This class is to be used both
// before launching the setuid helper and after being executed through the
// setuid helper.
// This class is difficult to use. It has been created by refactoring very old
// code scathered through the Chromium code base.
//
// A typical use for "A" launching a sandboxed process "B" would be:
// 1. A calls SetupLaunchEnvironment()
// 2. A sets up a CommandLine and then amends it with
//    PrependWrapper() (or manually, by relying on GetSandboxBinaryPath()).
// 3. A makes sure that GetUniqueToChildFileDescriptor() is an existing file
//    descriptor that can be closed by the helper process created by the
//    setuid sandbox. (This is the right file descriptor to use for magic
//    "must-be-unique" sockets that are use to identify processes across
//    pid namespaces.)
// 4. A launches B with base::LaunchProcess, using the amended CommandLine.
// 5. B performs various initializations that require access to the file
//    system.
// 5.b (optional) B uses sandbox::Credentials::HasOpenDirectory() to verify
//    that no directory is kept open (which would allow bypassing the setuid
//    sandbox).
// 6. B should be prepared to assume the role of init(1). In particular, B
//    cannot receive any signal from any other process, excluding SIGKILL.
//    If B dies, all the processes in the namespace will die.
//    B can fork() and the parent can assume the role of init(1), by using
//    CreateInitProcessReaper().
// 7. B requests being chroot-ed through ChrootMe() and
//    requests other sandboxing status via the status functions.
class SANDBOX_EXPORT SetuidSandboxClient {
 public:
  // All instantation should go through this factory method.
  static class SetuidSandboxClient* Create();
  ~SetuidSandboxClient();

  // Ask the setuid helper over the setuid sandbox IPC channel to chroot() us
  // to an empty directory.
  // Will only work if we have been launched through the setuid helper.
  bool ChrootMe();
  // When a new PID namespace is created, the process with pid == 1 should
  // assume the role of init.
  // See sandbox/linux/services/init_process_reaper.h for more information
  // on this API.
  bool CreateInitProcessReaper(base::Closure* post_fork_parent_callback);

  // Did we get launched through an up to date setuid binary ?
  bool IsSuidSandboxUpToDate() const;
  // Did we get launched through the setuid helper ?
  bool IsSuidSandboxChild() const;
  // Did the setuid helper create a new PID namespace ?
  bool IsInNewPIDNamespace() const;
  // Did the setuid helper create a new network namespace ?
  bool IsInNewNETNamespace() const;
  // Are we done and fully sandboxed ?
  bool IsSandboxed() const;

  // The setuid sandbox may still be disabled via the environment.
  // This is tracked in crbug.com/245376.
  bool IsDisabledViaEnvironment();
  // When using the setuid sandbox, an extra helper process is created.
  // Unfortunately, this helper process is hard-wired to close a specific file
  // descriptor.
  // The caller must make sure that GetUniqueToChildFileDescriptor() is an
  // existing file descriptor that can be closed by the helper process. It's ok
  // to make it a dummy, useless file descriptor if needed.
  int GetUniqueToChildFileDescriptor();
  // Get the sandbox binary path. This method knows about the
  // CHROME_DEVEL_SANDBOX environment variable used for user-managed builds. If
  // the sandbox binary cannot be found, it will return an empty FilePath.
  base::FilePath GetSandboxBinaryPath();
  // Modify |cmd_line| and |options| to launch via the setuid sandbox. Crash if
  // the setuid sandbox binary cannot be found. Either can be NULL if the caller
  // needs additional control.
  void PrependWrapper(base::CommandLine* cmd_line,
                      base::LaunchOptions* options);
  // Set-up the environment. This should be done prior to launching the setuid
  // helper.
  void SetupLaunchEnvironment();

 private:
  // Holds the environment. Will never be NULL.
  base::Environment* env_;
  bool sandboxed_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(SetuidSandboxClient);
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SUID_SETUID_SANDBOX_CLIENT_H_

