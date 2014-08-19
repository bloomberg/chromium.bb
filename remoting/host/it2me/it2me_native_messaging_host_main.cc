// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me/it2me_native_messaging_host_main.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "media/base/media.h"
#include "net/socket/ssl_server_socket.h"
#include "remoting/base/breakpad.h"
#include "remoting/base/resources.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/host/it2me/it2me_native_messaging_host.h"
#include "remoting/host/logging.h"
#include "remoting/host/usage_stats_consent.h"

#if defined(OS_LINUX)
#include <gtk/gtk.h>
#include <X11/Xlib.h>
#endif  // defined(OS_LINUX)

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN)
#include <commctrl.h>
#endif  // defined(OS_WIN)

namespace remoting {

// Creates a It2MeNativeMessagingHost instance, attaches it to stdin/stdout and
// runs the message loop until It2MeNativeMessagingHost signals shutdown.
int StartIt2MeNativeMessagingHost() {
#if defined(OS_MACOSX)
  // Needed so we don't leak objects when threads are created.
  base::mac::ScopedNSAutoreleasePool pool;
#endif  // defined(OS_MACOSX)

#if defined(REMOTING_ENABLE_BREAKPAD)
  // Initialize Breakpad as early as possible. On Mac the command-line needs to
  // be initialized first, so that the preference for crash-reporting can be
  // looked up in the config file.
  if (IsUsageStatsAllowed()) {
    InitializeCrashReporting();
  }
#endif  // defined(REMOTING_ENABLE_BREAKPAD)

#if defined(OS_WIN)
  // Register and initialize common controls.
  INITCOMMONCONTROLSEX info;
  info.dwSize = sizeof(info);
  info.dwICC = ICC_STANDARD_CLASSES;
  InitCommonControlsEx(&info);
#endif  // defined(OS_WIN)

  // Required to find the ICU data file, used by some file_util routines.
  base::i18n::InitializeICU();

  remoting::LoadResources("");

  // Cannot use TOOLKIT_GTK because it is not defined when aura is enabled.
#if defined(OS_LINUX)
  // Required in order for us to run multiple X11 threads.
  XInitThreads();

  // Required for any calls into GTK functions, such as the Disconnect and
  // Continue windows. Calling with NULL arguments because we don't have
  // any command line arguments for gtk to consume.
  gtk_init(NULL, NULL);
#endif  // OS_LINUX

  // Enable support for SSL server sockets, which must be done while still
  // single-threaded.
  net::EnableSSLServerSockets();

  // Ensures runtime specific CPU features are initialized.
  media::InitializeCPUSpecificMediaFeatures();

#if defined(OS_WIN)
  // GetStdHandle() returns pseudo-handles for stdin and stdout even if
  // the hosting executable specifies "Windows" subsystem. However the returned
  // handles are invalid in that case unless standard input and output are
  // redirected to a pipe or file.
  base::File read_file(GetStdHandle(STD_INPUT_HANDLE));
  base::File write_file(GetStdHandle(STD_OUTPUT_HANDLE));

  // After the native messaging channel starts the native messaging reader
  // will keep doing blocking read operations on the input named pipe.
  // If any other thread tries to perform any operation on STDIN, it will also
  // block because the input named pipe is synchronous (non-overlapped).
  // It is pretty common for a DLL to query the device info (GetFileType) of
  // the STD* handles at startup. So any LoadLibrary request can potentially
  // be blocked. To prevent that from happening we close STDIN and STDOUT
  // handles as soon as we retrieve the corresponding file handles.
  SetStdHandle(STD_INPUT_HANDLE, NULL);
  SetStdHandle(STD_OUTPUT_HANDLE, NULL);
#elif defined(OS_POSIX)
  // The files are automatically closed.
  base::File read_file(STDIN_FILENO);
  base::File write_file(STDOUT_FILENO);
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
      new NativeMessagingChannel(read_file.Pass(), write_file.Pass()));

  scoped_ptr<It2MeNativeMessagingHost> host(
      new It2MeNativeMessagingHost(
          task_runner, channel.Pass(), factory.Pass()));
  host->Start(run_loop.QuitClosure());

  // Run the loop until channel is alive.
  run_loop.Run();

  return kSuccessExitCode;
}

int It2MeNativeMessagingHostMain(int argc, char** argv) {
  // This object instance is required by Chrome code (such as MessageLoop).
  base::AtExitManager exit_manager;

  base::CommandLine::Init(argc, argv);
  remoting::InitHostLogging();

  return StartIt2MeNativeMessagingHost();
}

}  // namespace remoting
