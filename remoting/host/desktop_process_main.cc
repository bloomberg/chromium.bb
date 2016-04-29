// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements the Windows service controlling Me2Me host processes
// running within user sessions.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/desktop_process.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/host/host_main.h"
#include "remoting/host/me2me_desktop_environment.h"
#include "remoting/host/switches.h"
#include "remoting/host/win/session_desktop_environment.h"

namespace remoting {

int DesktopProcessMain() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  std::string channel_name =
      command_line->GetSwitchValueASCII(kDaemonPipeSwitchName);

  if (channel_name.empty())
    return kInvalidCommandLineExitCode;

  base::MessageLoopForUI message_loop;
  base::RunLoop run_loop;
  scoped_refptr<AutoThreadTaskRunner> ui_task_runner =
      new AutoThreadTaskRunner(message_loop.task_runner(),
                               run_loop.QuitClosure());

  // Launch the video capture thread.
  scoped_refptr<AutoThreadTaskRunner> video_capture_task_runner =
      AutoThread::Create("Video capture thread", ui_task_runner);

  // Launch the input thread.
  scoped_refptr<AutoThreadTaskRunner> input_task_runner =
      AutoThread::CreateWithType(
          "Input thread", ui_task_runner, base::MessageLoop::TYPE_IO);

  DesktopProcess desktop_process(ui_task_runner,
                                 input_task_runner,
                                 channel_name);

  // Create a platform-dependent environment factory.
  std::unique_ptr<DesktopEnvironmentFactory> desktop_environment_factory;
#if defined(OS_WIN)
  desktop_environment_factory.reset(new SessionDesktopEnvironmentFactory(
      ui_task_runner, video_capture_task_runner, input_task_runner,
      ui_task_runner,
      base::Bind(&DesktopProcess::InjectSas, desktop_process.AsWeakPtr()),
      base::Bind(&DesktopProcess::LockWorkStation,
                 desktop_process.AsWeakPtr())));
#else  // !defined(OS_WIN)
  desktop_environment_factory.reset(new Me2MeDesktopEnvironmentFactory(
      ui_task_runner, video_capture_task_runner, input_task_runner,
      ui_task_runner));
#endif  // !defined(OS_WIN)

  if (!desktop_process.Start(std::move(desktop_environment_factory)))
    return kInitializationFailed;

  // Run the UI message loop.
  ui_task_runner = nullptr;
  run_loop.Run();

  return kSuccessExitCode;
}

}  // namespace remoting

#if !defined(OS_WIN)
int main(int argc, char** argv) {
  return remoting::HostMain(argc, argv);
}
#endif  // !defined(OS_WIN)
