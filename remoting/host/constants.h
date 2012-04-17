// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CONSTANTS_H_
#define REMOTING_HOST_CONSTANTS_H_

namespace remoting {

// Known host exit codes.
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

}  // namespace remoting

#endif  // REMOTING_HOST_CONSTANTS_H_
