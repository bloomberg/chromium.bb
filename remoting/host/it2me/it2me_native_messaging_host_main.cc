// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "remoting/host/it2me/it2me_native_messaging_host.h"
#include "remoting/host/logging.h"

namespace remoting {

// Creates a It2MeNativeMessagingHost instance, attaches it to stdin/stdout and
// runs the message loop until It2MeNativeMessagingHost signals shutdown.
int It2MeNativeMessagingHostMain() {
#if defined(OS_WIN)
  // GetStdHandle() returns pseudo-handles for stdin and stdout even if
  // the hosting executable specifies "Windows" subsystem. However the returned
  // handles are invalid in that case unless standard input and output are
  // redirected to a pipe or file.
  base::PlatformFile read_file = GetStdHandle(STD_INPUT_HANDLE);
  base::PlatformFile write_file = GetStdHandle(STD_OUTPUT_HANDLE);
#elif defined(OS_POSIX)
  base::PlatformFile read_file = STDIN_FILENO;
  base::PlatformFile write_file = STDOUT_FILENO;
#else
#error Not implemented.
#endif

  base::MessageLoopForUI message_loop;
  base::RunLoop run_loop;

  scoped_refptr<AutoThreadTaskRunner> task_runner =
      new remoting::AutoThreadTaskRunner(message_loop.message_loop_proxy(),
                                         run_loop.QuitClosure());

  scoped_ptr<It2MeHostFactory> factory(new It2MeHostFactory());

  // Set up the native messaging channel.
  scoped_ptr<NativeMessagingChannel> channel(
      new NativeMessagingChannel(read_file, write_file));

  scoped_ptr<It2MeNativeMessagingHost> host(
      new It2MeNativeMessagingHost(
          task_runner, channel.Pass(), factory.Pass()));
  host->Start(run_loop.QuitClosure());

  // Run the loop until channel is alive.
  run_loop.Run();
  return kSuccessExitCode;
}

}  // namespace remoting

int main(int argc, char** argv) {
  // This object instance is required by Chrome code (such as MessageLoop).
  base::AtExitManager exit_manager;

  CommandLine::Init(argc, argv);
  remoting::InitHostLogging();

  return remoting::It2MeNativeMessagingHostMain();
}
