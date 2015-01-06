// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SERVICES_CREDENTIALS_H_
#define SANDBOX_LINUX_SERVICES_CREDENTIALS_H_

#include "build/build_config.h"
// Link errors are tedious to track, raise a compile-time error instead.
#if defined(OS_ANDROID)
#error "Android is not supported."
#endif  // defined(OS_ANDROID).

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {

// This class should be used to manipulate the current process' credentials.
// It is currently a stub used to manipulate POSIX.1e capabilities as
// implemented by the Linux kernel.
class SANDBOX_EXPORT Credentials {
 public:
  Credentials();
  ~Credentials();

  // Drop all capabilities in the effective, inheritable and permitted sets for
  // the current process.
  bool DropAllCapabilities() WARN_UNUSED_RESULT;
  // Return true iff there is any capability in any of the capabilities sets
  // of the current process.
  bool HasAnyCapability() const;
  // Returns the capabilities of the current process in textual form, as
  // documented in libcap2's cap_to_text(3). This is mostly useful for
  // debugging and tests.
  scoped_ptr<std::string> GetCurrentCapString() const;

  // Returns whether the kernel supports CLONE_NEWUSER and whether it would be
  // possible to immediately move to a new user namespace. There is no point
  // in using this method right before calling MoveToNewUserNS(), simply call
  // MoveToNewUserNS() immediately. This method is only useful to test kernel
  // support ahead of time.
  static bool SupportsNewUserNS();

  // Move the current process to a new "user namespace" as supported by Linux
  // 3.8+ (CLONE_NEWUSER).
  // The uid map will be set-up so that the perceived uid and gid will not
  // change.
  // If this call succeeds, the current process will be granted a full set of
  // capabilities in the new namespace.
  bool MoveToNewUserNS() WARN_UNUSED_RESULT;

  // Remove the ability of the process to access the file system. File
  // descriptors which are already open prior to calling this API remain
  // available.
  // The implementation currently uses chroot(2) and requires CAP_SYS_CHROOT.
  // CAP_SYS_CHROOT can be acquired by using the MoveToNewUserNS() API.
  // Make sure to call DropAllCapabilities() after this call to prevent
  // escapes.
  // To be secure, the caller must ensure that any directory file descriptors
  // are closed (for example, by checking the result of
  // ProcUtil::HasOpenDirectory with a file descriptor for /proc, then closing
  // that file descriptor). Otherwise it may be possible to escape the chroot.
  bool DropFileSystemAccess() WARN_UNUSED_RESULT;

 private:
  DISALLOW_COPY_AND_ASSIGN(Credentials);
};

}  // namespace sandbox.

#endif  // SANDBOX_LINUX_SERVICES_CREDENTIALS_H_
