// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CONSTANTS_H_
#define REMOTING_HOST_CONSTANTS_H_

#include "base/compiler_specific.h"

namespace remoting {

// Known host exit codes.
// Please keep this enum in sync with:
// remoting/host/installer/mac/PrivilegedHelperTools/
// org.chromium.chromoting.me2me.sh
// and remoting/tools/me2me_virtual_host.py.
enum HostExitCodes {
  // Error codes that don't indicate a permanent error condition.
  kSuccessExitCode = 0,
  kReservedForX11ExitCode = 1,
  kHostInitializationFailed = 7,

  // Error codes that do indicate a permanent error condition.
  kInvalidHostConfigurationExitCode = 2,
  kInvalidHostIdExitCode = 3,
  kInvalidOauthCredentialsExitCode = 4,
  kInvalidHostDomainExitCode = 5,
  kLoginScreenNotSupportedExitCode = 6,

  // The range of the exit codes that should be interpreted as a permanent error
  // condition.
  kMinPermanentErrorExitCode = kInvalidHostConfigurationExitCode,
  kMaxPermanentErrorExitCode = kLoginScreenNotSupportedExitCode
};

#if defined(OS_WIN)
// The Omaha Appid of the host.
extern const wchar_t kHostOmahaAppid[];
#endif  // defined(OS_WIN)

}  // namespace remoting

#endif  // REMOTING_HOST_CONSTANTS_H_
