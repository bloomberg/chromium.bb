// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "net/url_request/url_fetcher.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/host/logging.h"
#include "remoting/host/pairing_registry_delegate.h"
#include "remoting/host/setup/me2me_native_messaging_host.h"

namespace {

const char kParentWindowSwitchName[] = "parent-window";

}  // namespace

namespace remoting {

int Me2MeNativeMessagingHostMain() {
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

  // Mac OS X requires that the main thread be a UI message loop in order to
  // receive distributed notifications from the System Preferences pane. An
  // IO thread is needed for the pairing registry and URL context getter.
  base::Thread io_thread("io_thread");
  io_thread.StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0));

  base::MessageLoopForUI message_loop;
  base::RunLoop run_loop;

  scoped_refptr<DaemonController> daemon_controller =
      DaemonController::Create();

  // Pass handle of the native view to the controller so that the UAC prompts
  // are focused properly.
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kParentWindowSwitchName)) {
    std::string native_view =
        command_line->GetSwitchValueASCII(kParentWindowSwitchName);
    int64 native_view_handle = 0;
    if (base::StringToInt64(native_view, &native_view_handle)) {
      daemon_controller->SetWindow(reinterpret_cast<void*>(native_view_handle));
    } else {
      LOG(WARNING) << "Invalid parameter value --" << kParentWindowSwitchName
                   << "=" << native_view;
    }
  }

  // OAuth client (for credential requests).
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter(
      new URLRequestContextGetter(io_thread.message_loop_proxy()));
  scoped_ptr<OAuthClient> oauth_client(
      new OAuthClient(url_request_context_getter));

  net::URLFetcher::SetIgnoreCertificateRequests(true);

  // Create the pairing registry and native messaging host.
  scoped_refptr<protocol::PairingRegistry> pairing_registry =
      CreatePairingRegistry(io_thread.message_loop_proxy());

  // Set up the native messaging channel.
  scoped_ptr<NativeMessagingChannel> channel(
      new NativeMessagingChannel(read_file, write_file));

  scoped_ptr<Me2MeNativeMessagingHost> host(
      new Me2MeNativeMessagingHost(channel.Pass(),
                              daemon_controller,
                              pairing_registry,
                              oauth_client.Pass()));
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

  return remoting::Me2MeNativeMessagingHostMain();
}
