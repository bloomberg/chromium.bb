// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/remote_security_key_main.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/host/logging.h"

namespace remoting {

int StartRemoteSecurityKey() {
  return kSuccessExitCode;
}

int RemoteSecurityKeyMain(int argc, char** argv) {
  // This object instance is required by Chrome classes (such as MessageLoop).
  base::AtExitManager exit_manager;

  base::CommandLine::Init(argc, argv);
  remoting::InitHostLogging();

  return StartRemoteSecurityKey();
}

}  // namespace remoting
