// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shell/tests/util.h"

#include "base/base_paths.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "services/shell/public/cpp/connection.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/interfaces/connector.mojom.h"
#include "services/shell/public/interfaces/shell_client_factory.mojom.h"
#include "services/shell/runner/common/switches.h"

namespace shell {
namespace test {

namespace {

void QuitLoop(base::RunLoop* loop) {
  loop->Quit();
}

}  // namespace

std::unique_ptr<Connection> LaunchAndConnectToProcess(
    const std::string& target_exe_name,
    const Identity target,
    shell::Connector* connector,
    base::Process* process) {
  base::FilePath target_path;
  CHECK(base::PathService::Get(base::DIR_EXE, &target_path));
  target_path = target_path.AppendASCII(target_exe_name);

  base::CommandLine child_command_line(target_path);
  // Forward the wait-for-debugger flag but nothing else - we don't want to
  // stamp on the platform-channel flag.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWaitForDebugger)) {
    child_command_line.AppendSwitch(switches::kWaitForDebugger);
  }

  // Create the channel to be shared with the target process. Pass one end
  // on the command line.
  mojo::edk::PlatformChannelPair platform_channel_pair;
  mojo::edk::HandlePassingInformation handle_passing_info;
  platform_channel_pair.PrepareToPassClientHandleToChildProcess(
      &child_command_line, &handle_passing_info);

  // Generate a token for the child to find and connect to a primordial pipe
  // and pass that as well.
  std::string primordial_pipe_token = mojo::edk::GenerateRandomToken();
  child_command_line.AppendSwitchASCII(switches::kPrimordialPipeToken,
                                        primordial_pipe_token);

  // Allocate the pipe locally.
  std::string child_token = mojo::edk::GenerateRandomToken();
  mojo::ScopedMessagePipeHandle pipe =
      mojo::edk::CreateParentMessagePipe(primordial_pipe_token, child_token);

  shell::mojom::ShellClientPtr client;
  client.Bind(
      mojo::InterfacePtrInfo<shell::mojom::ShellClient>(std::move(pipe), 0u));
  shell::mojom::PIDReceiverPtr receiver;

  shell::Connector::ConnectParams params(target);
  params.set_client_process_connection(std::move(client), GetProxy(&receiver));
  std::unique_ptr<shell::Connection> connection = connector->Connect(&params);
  {
    base::RunLoop loop;
    connection->AddConnectionCompletedClosure(base::Bind(&QuitLoop, &loop));
    base::MessageLoop::ScopedNestableTaskAllower allow(
        base::MessageLoop::current());
    loop.Run();
  }

  base::LaunchOptions options;
#if defined(OS_WIN)
  options.handles_to_inherit = &handle_passing_info;
#elif defined(OS_POSIX)
  options.fds_to_remap = &handle_passing_info;
#endif
  *process = base::LaunchProcess(child_command_line, options);
  DCHECK(process->IsValid());
  receiver->SetPID(process->Pid());
  mojo::edk::ChildProcessLaunched(process->Handle(),
                                  platform_channel_pair.PassServerHandle(),
                                  child_token);
  return connection;
}

}  // namespace test
}  // namespace shell
