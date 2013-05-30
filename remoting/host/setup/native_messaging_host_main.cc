// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/native_messaging_host.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "remoting/host/logging.h"

#if defined(OS_POSIX)
#include <unistd.h>
#endif

int main(int argc, char** argv) {
  // This object instance is required by Chrome code (such as MessageLoop).
  base::AtExitManager exit_manager;

  CommandLine::Init(argc, argv);
  remoting::InitHostLogging();

#if defined(OS_WIN)
  base::PlatformFile read_file = GetStdHandle(STD_INPUT_HANDLE);
  base::PlatformFile write_file = GetStdHandle(STD_OUTPUT_HANDLE);
#elif defined(OS_POSIX)
  base::PlatformFile read_file = STDIN_FILENO;
  base::PlatformFile write_file = STDOUT_FILENO;
#else
#error Not implemented.
#endif

  base::MessageLoop message_loop(base::MessageLoop::TYPE_IO);
  base::RunLoop run_loop;
  remoting::NativeMessagingHost host(remoting::DaemonController::Create(),
                                     read_file, write_file,
                                     message_loop.message_loop_proxy(),
                                     run_loop.QuitClosure());
  host.Start();
  run_loop.Run();
  return 0;
}
