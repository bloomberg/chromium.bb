// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/me2me_native_messaging_host_main.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/i18n/icu_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_scheduler/task_scheduler.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "net/url_request/url_fetcher.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/breakpad.h"
#include "remoting/base/gaia_oauth_client.h"
#include "remoting/base/url_request_context_getter.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/host/logging.h"
#include "remoting/host/native_messaging/native_messaging_pipe.h"
#include "remoting/host/native_messaging/pipe_messaging_channel.h"
#include "remoting/host/pairing_registry_delegate.h"
#include "remoting/host/setup/me2me_native_messaging_host.h"
#include "remoting/host/switches.h"
#include "remoting/host/usage_stats_consent.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN)
#include "base/process/process_info.h"
#include "base/win/registry.h"
#include "remoting/host/pairing_registry_delegate_win.h"
#endif  // defined(OS_WIN)

#if defined(USE_GLIB) && !defined(OS_CHROMEOS)
#include <glib-object.h>
#endif  // defined(USE_GLIB) && !defined(OS_CHROMEOS)

using remoting::protocol::PairingRegistry;

namespace remoting {

int Me2MeNativeMessagingHostMain(int argc, char** argv) {
  // This object instance is required by Chrome code (such as MessageLoop).
  base::AtExitManager exit_manager;

  base::CommandLine::Init(argc, argv);
  remoting::InitHostLogging();

#if defined(OS_MACOSX)
  // Needed so we don't leak objects when threads are created.
  base::mac::ScopedNSAutoreleasePool pool;
#endif  // defined(OS_MACOSX)

#if defined(USE_GLIB) && !defined(OS_CHROMEOS)
// g_type_init will be deprecated in 2.36. 2.35 is the development
// version for 2.36, hence do not call g_type_init starting 2.35.
// http://developer.gnome.org/gobject/unstable/gobject-Type-Information.html#g-type-init
#if !GLIB_CHECK_VERSION(2, 35, 0)
  g_type_init();
#endif
#endif  // defined(USE_GLIB) && !defined(OS_CHROMEOS)

  // Required to find the ICU data file, used by some file_util routines.
  base::i18n::InitializeICU();

#if defined(REMOTING_ENABLE_BREAKPAD)
  // Initialize Breakpad as early as possible. On Mac the command-line needs to
  // be initialized first, so that the preference for crash-reporting can be
  // looked up in the config file.
  if (IsUsageStatsAllowed()) {
    InitializeCrashReporting();
  }
#endif  // defined(REMOTING_ENABLE_BREAKPAD)

  base::TaskScheduler::CreateAndStartWithDefaultParams("Me2Me");

  // Mac OS X requires that the main thread be a UI message loop in order to
  // receive distributed notifications from the System Preferences pane. An
  // IO thread is needed for the pairing registry and URL context getter.
  base::Thread io_thread("io_thread");
  io_thread.StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0));

  base::Thread file_thread("file_thread");
  file_thread.StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0));

  base::MessageLoopForUI message_loop;
  base::RunLoop run_loop;

  scoped_refptr<DaemonController> daemon_controller =
      DaemonController::Create();

  // Pass handle of the native view to the controller so that the UAC prompts
  // are focused properly.
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  int64_t native_view_handle = 0;
  if (command_line->HasSwitch(kParentWindowSwitchName)) {
    std::string native_view =
        command_line->GetSwitchValueASCII(kParentWindowSwitchName);
    if (!base::StringToInt64(native_view, &native_view_handle)) {
      LOG(WARNING) << "Invalid parameter value --" << kParentWindowSwitchName
                   << "=" << native_view;
    }
  }

  base::File read_file;
  base::File write_file;
  bool needs_elevation = false;

#if defined(OS_WIN)
  needs_elevation = !base::IsCurrentProcessElevated();

  if (command_line->HasSwitch(kElevateSwitchName)) {
    DCHECK(!needs_elevation);

    // The "elevate" switch is always accompanied by the "input" and "output"
    // switches whose values name named pipes that should be used in place of
    // stdin and stdout.
    DCHECK(command_line->HasSwitch(kInputSwitchName));
    DCHECK(command_line->HasSwitch(kOutputSwitchName));

    // presubmit: allow wstring
    std::wstring input_pipe_name =
      command_line->GetSwitchValueNative(kInputSwitchName);
    // presubmit: allow wstring
    std::wstring output_pipe_name =
      command_line->GetSwitchValueNative(kOutputSwitchName);

    // A NULL SECURITY_ATTRIBUTES signifies that the handle can't be inherited.
    read_file =
        base::File(CreateFile(input_pipe_name.c_str(), GENERIC_READ, 0, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!read_file.IsValid()) {
      PLOG(ERROR) << "CreateFile failed on '" << input_pipe_name << "'";
      return kInitializationFailed;
    }

    write_file = base::File(CreateFile(
        output_pipe_name.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!write_file.IsValid()) {
      PLOG(ERROR) << "CreateFile failed on '" << output_pipe_name << "'";
      return kInitializationFailed;
    }
  } else {
    // GetStdHandle() returns pseudo-handles for stdin and stdout even if
    // the hosting executable specifies "Windows" subsystem. However the
    // returned handles are invalid in that case unless standard input and
    // output are redirected to a pipe or file.
    read_file = base::File(GetStdHandle(STD_INPUT_HANDLE));
    write_file = base::File(GetStdHandle(STD_OUTPUT_HANDLE));

    // After the native messaging channel starts the native messaging reader
    // will keep doing blocking read operations on the input named pipe.
    // If any other thread tries to perform any operation on STDIN, it will also
    // block because the input named pipe is synchronous (non-overlapped).
    // It is pretty common for a DLL to query the device info (GetFileType) of
    // the STD* handles at startup. So any LoadLibrary request can potentially
    // be blocked. To prevent that from happening we close STDIN and STDOUT
    // handles as soon as we retrieve the corresponding file handles.
    SetStdHandle(STD_INPUT_HANDLE, nullptr);
    SetStdHandle(STD_OUTPUT_HANDLE, nullptr);
  }
#elif defined(OS_POSIX)
  // The files will be automatically closed.
  read_file = base::File(STDIN_FILENO);
  write_file = base::File(STDOUT_FILENO);
#else
#error Not implemented.
#endif

  // OAuth client (for credential requests). IO thread is used for blocking
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter(
      new URLRequestContextGetter(io_thread.task_runner(),
                                  file_thread.task_runner()));
  std::unique_ptr<OAuthClient> oauth_client(
      new GaiaOAuthClient(url_request_context_getter));

  net::URLFetcher::SetIgnoreCertificateRequests(true);

  // Create the pairing registry.
  scoped_refptr<PairingRegistry> pairing_registry;

#if defined(OS_WIN)
  base::win::RegKey root;
  LONG result = root.Open(HKEY_LOCAL_MACHINE, kPairingRegistryKeyName,
                          KEY_READ);
  if (result != ERROR_SUCCESS) {
    SetLastError(result);
    PLOG(ERROR) << "Failed to open HKLM\\" << kPairingRegistryKeyName;
    return kInitializationFailed;
  }

  base::win::RegKey unprivileged;
  result = unprivileged.Open(root.Handle(), kPairingRegistryClientsKeyName,
                             needs_elevation ? KEY_READ : KEY_READ | KEY_WRITE);
  if (result != ERROR_SUCCESS) {
    SetLastError(result);
    PLOG(ERROR) << "Failed to open HKLM\\" << kPairingRegistryKeyName
                << "\\" << kPairingRegistryClientsKeyName;
    return kInitializationFailed;
  }

  // Only try to open the privileged key if the current process is elevated.
  base::win::RegKey privileged;
  if (!needs_elevation) {
    result = privileged.Open(root.Handle(), kPairingRegistrySecretsKeyName,
                             KEY_READ | KEY_WRITE);
    if (result != ERROR_SUCCESS) {
      SetLastError(result);
      PLOG(ERROR) << "Failed to open HKLM\\" << kPairingRegistryKeyName << "\\"
                  << kPairingRegistrySecretsKeyName;
      return kInitializationFailed;
    }
  }

  // Initialize the pairing registry delegate and set the root keys.
  std::unique_ptr<PairingRegistryDelegateWin> delegate(
      new PairingRegistryDelegateWin());
  if (!delegate->SetRootKeys(privileged.Take(), unprivileged.Take()))
    return kInitializationFailed;

  pairing_registry =
      new PairingRegistry(io_thread.task_runner(), std::move(delegate));
#else  // defined(OS_WIN)
  pairing_registry =
      CreatePairingRegistry(io_thread.task_runner());
#endif  // !defined(OS_WIN)

  std::unique_ptr<NativeMessagingPipe> native_messaging_pipe(
      new NativeMessagingPipe());

  // Set up the native messaging channel.
  std::unique_ptr<extensions::NativeMessagingChannel> channel(
      new PipeMessagingChannel(std::move(read_file), std::move(write_file)));

  std::unique_ptr<ChromotingHostContext> context =
      ChromotingHostContext::Create(new remoting::AutoThreadTaskRunner(
          message_loop.task_runner(), run_loop.QuitClosure()));

  // Create the native messaging host.
  std::unique_ptr<extensions::NativeMessageHost> host(
      new Me2MeNativeMessagingHost(needs_elevation,
                                   static_cast<intptr_t>(native_view_handle),
                                   std::move(context), daemon_controller,
                                   pairing_registry, std::move(oauth_client)));

  host->Start(native_messaging_pipe.get());

  native_messaging_pipe->Start(std::move(host), std::move(channel));

  // Run the loop until channel is alive.
  run_loop.Run();

  // Block until tasks blocking shutdown have completed their execution.
  base::TaskScheduler::GetInstance()->Shutdown();

  return kSuccessExitCode;
}

}  // namespace remoting
