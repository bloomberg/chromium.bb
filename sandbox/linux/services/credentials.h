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
#include "base/memory/scoped_ptr.h"

namespace sandbox {

// This class should be used to manipulate the current process' credentials.
// It is currently a stub used to manipulate POSIX.1e capabilities as
// implemented by the Linux kernel.
class Credentials {
 public:
  Credentials();
  ~Credentials();

  // Drop all capabilities in the effective, inheritable and permitted sets for
  // the current process.
  void DropAllCapabilities();
  // Return true iff there is any capability in any of the capabilities sets
  // of the current process.
  bool HasAnyCapability();
  // Returns the capabilities of the current process in textual form, as
  // documented in libcap2's cap_to_text(3). This is mostly useful for
  // debugging and tests.
  scoped_ptr<std::string> GetCurrentCapString();

 private:
  DISALLOW_COPY_AND_ASSIGN(Credentials);
};

}  // namespace sandbox.

#endif  // SANDBOX_LINUX_SERVICES_CREDENTIALS_H_
