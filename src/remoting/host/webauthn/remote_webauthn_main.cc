// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/webauthn/remote_webauthn_main.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "remoting/base/logging.h"
#include "remoting/host/base/host_exit_codes.h"
#include "remoting/host/chromoting_host_services_client.h"
#include "remoting/host/native_messaging/native_messaging_pipe.h"
#include "remoting/host/native_messaging/pipe_messaging_channel.h"
#include "remoting/host/webauthn/remote_webauthn_native_messaging_host.h"

#if defined(OS_WIN)
#include <windows.h>
#endif  // defined(OS_WIN)

namespace remoting {

int RemoteWebAuthnMain(int argc, char** argv) {
  base::AtExitManager exit_manager;
  base::SingleThreadTaskExecutor task_executor(base::MessagePumpType::IO);
  auto task_runner = base::ThreadTaskRunnerHandle::Get();

  base::CommandLine::Init(argc, argv);
  InitHostLogging();

  if (!ChromotingHostServicesClient::Initialize()) {
    return kInitializationFailed;
  }

  mojo::core::Init();
  mojo::core::ScopedIPCSupport ipc_support(
      task_runner, mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST);

  base::File read_file;
  base::File write_file;

#if defined(OS_POSIX)
  read_file = base::File(STDIN_FILENO);
  write_file = base::File(STDOUT_FILENO);
#elif defined(OS_WIN)
  // GetStdHandle() returns pseudo-handles for stdin and stdout even if
  // the hosting executable specifies "Windows" subsystem. However the
  // returned handles are invalid in that case unless standard input and
  // output are redirected to a pipe or file.
  read_file = base::File(GetStdHandle(STD_INPUT_HANDLE));
  write_file = base::File(GetStdHandle(STD_OUTPUT_HANDLE));

  // After the native messaging channel starts, the native messaging reader
  // will keep doing blocking read operations on the input named pipe.
  // If any other thread tries to perform any operation on STDIN, it will also
  // block because the input named pipe is synchronous (non-overlapped).
  // It is pretty common for a DLL to query the device info (GetFileType) of
  // the STD* handles at startup. So any LoadLibrary request can potentially
  // be blocked. To prevent that from happening we close STDIN and STDOUT
  // handles as soon as we retrieve the corresponding file handles.
  SetStdHandle(STD_INPUT_HANDLE, nullptr);
  SetStdHandle(STD_OUTPUT_HANDLE, nullptr);
#endif

  base::RunLoop run_loop;

  NativeMessagingPipe native_messaging_pipe;
  auto channel = std::make_unique<PipeMessagingChannel>(std::move(read_file),
                                                        std::move(write_file));

#if defined(OS_POSIX)
  PipeMessagingChannel::ReopenStdinStdout();
#endif  // defined(OS_POSIX)

  auto native_messaging_host =
      std::make_unique<RemoteWebAuthnNativeMessagingHost>(task_runner);
  native_messaging_host->Start(&native_messaging_pipe);
  native_messaging_pipe.Start(std::move(native_messaging_host),
                              std::move(channel));

  run_loop.Run();

  return kSuccessExitCode;
}

}  // namespace remoting
