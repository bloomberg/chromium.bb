// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements the Windows service controlling Me2Me host processes
// running within user sessions.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/basic_desktop_environment.h"
#include "remoting/host/desktop_process.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/host/host_main.h"
#include "remoting/host/ipc_constants.h"
#include "remoting/host/win/session_desktop_environment.h"

namespace remoting {

int DesktopProcessMain() {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  std::string channel_name =
      command_line->GetSwitchValueASCII(kDaemonPipeSwitchName);

  if (channel_name.empty())
    return kUsageExitCode;

  MessageLoop message_loop(MessageLoop::TYPE_UI);
  base::RunLoop run_loop;
  scoped_refptr<AutoThreadTaskRunner> ui_task_runner =
      new AutoThreadTaskRunner(message_loop.message_loop_proxy(),
                               run_loop.QuitClosure());

  DesktopProcess desktop_process(ui_task_runner, channel_name);

  // Create a platform-dependent environment factory.
  scoped_ptr<DesktopEnvironmentFactory> desktop_environment_factory;
#if defined(OS_WIN)
  desktop_environment_factory.reset(
      new SessionDesktopEnvironmentFactory(
          base::Bind(&DesktopProcess::InjectSas,
                     desktop_process.AsWeakPtr())));
#else  // !defined(OS_WIN)
  desktop_environment_factory.reset(
      new BasicDesktopEnvironmentFactory(true));
#endif  // !defined(OS_WIN)

  if (!desktop_process.Start(desktop_environment_factory.Pass()))
    return kInitializationFailed;

  // Run the UI message loop.
  ui_task_runner = NULL;
  run_loop.Run();

  return kSuccessExitCode;
}

}  // namespace remoting

#if !defined(OS_WIN)
int main(int argc, char** argv) {
  return remoting::HostMain(argc, argv);
}
#endif  // !defined(OS_WIN)
