// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CONSTANTS_H_
#define REMOTING_HOST_CONSTANTS_H_

#include "base/string16.h"

namespace remoting {

// Known host exit codes.
// Please keep this enum in sync with:
// remoting/host/installer/mac/PrivilegedHelperTools/
// org.chromium.chromoting.me2me.sh
enum HostExitCodes {
  kSuccessExitCode = 0,
  kReservedForX11ExitCode = 1,
  kInvalidHostConfigurationExitCode = 2,
  kInvalidHostIdExitCode = 3,
  kInvalidOauthCredentialsExitCode = 4,

  // The range of the exit codes that should be interpreted as a permanent error
  // condition.
  kMinPermanentErrorExitCode = kInvalidHostConfigurationExitCode,
  kMaxPermanentErrorExitCode = kInvalidOauthCredentialsExitCode
};

#if defined(OS_WIN)

// The Omaha Appid of the host.
extern const char16 kHostOmahaAppid[];

#endif  // defined(OS_WIN)

}  // namespace remoting

#endif  // REMOTING_HOST_CONSTANTS_H_
