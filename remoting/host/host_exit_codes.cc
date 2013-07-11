// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_exit_codes.h"

#include "remoting/protocol/name_value_map.h"

using remoting::protocol::NameMapElement;

namespace remoting {

const NameMapElement<HostExitCodes> kHostExitCodeStrings[] = {
  { kSuccessExitCode, "SUCCESS_EXIT" },
  { kInitializationFailed, "INITIALIZATION_FAILED" },
  { kInvalidHostConfigurationExitCode, "INVALID_HOST_CONFIGURATION" },
  { kInvalidHostIdExitCode, "INVALID_HOST_ID" },
  { kInvalidOauthCredentialsExitCode, "INVALID_OAUTH_CREDENTIALS" },
  { kInvalidHostDomainExitCode, "INVALID_HOST_DOMAIN" },
  { kLoginScreenNotSupportedExitCode, "LOGIN_SCREEN_NOT_SUPPORTED" },
  { kUsernameMismatchExitCode, "USERNAME_MISMATCH" },
};

const char* ExitCodeToString(HostExitCodes exit_code) {
  return ValueToName(kHostExitCodeStrings, exit_code);
}

}  // namespace remoting
