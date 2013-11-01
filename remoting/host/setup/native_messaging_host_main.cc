// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "remoting/host/logging.h"
#include "remoting/host/setup/native_messaging_host.h"

int main(int argc, char** argv) {
  // This object instance is required by Chrome code (such as MessageLoop).
  base::AtExitManager exit_manager;

  CommandLine::Init(argc, argv);
  remoting::InitHostLogging();

  return remoting::NativeMessagingHostMain();
}
