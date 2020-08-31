// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CHILD_PROCESS_LAUNCHER_HELPER_H_
#define CONTENT_BROWSER_CHILD_PROCESS_LAUNCHER_HELPER_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/process/kill.h"
#include "base/process/process.h"
#include "base/sequenced_task_runner.h"
#include "build/build_config.h"
#include "content/public/common/result_codes.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "services/service_manager/zygote/common/zygote_buildflags.h"

#if !defined(OS_FUCHSIA)
#include "mojo/public/cpp/platform/named_platform_channel.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

#if defined(OS_WIN)
#include "sandbox/win/src/sandbox_types.h"
#else
#include "content/public/browser/posix_file_descriptor_info.h"
#endif

#if defined(OS_MACOSX)
#include "sandbox/mac/seatbelt_exec.h"
#endif

#if defined(OS_FUCHSIA)
#include "services/service_manager/sandbox/fuchsia/sandbox_policy_fuchsia.h"
#endif

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
#include "services/service_manager/zygote/common/zygote_handle.h"  // nogncheck
#endif

namespace base {
class CommandLine;
}

namespace content {

class ChildProcessLauncher;
class SandboxedProcessLauncherDelegate;
struct ChildProcessLauncherPriority;
struct ChildProcessTerminationInfo;

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
class PosixFileDescriptorInfo;
#endif

namespace internal {

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
using FileMappedForLaunch = PosixFileDescriptorInfo;
#else
using FileMappedForLaunch = base::HandlesToInheritVector;
#endif

// ChildProcessLauncherHelper is used by ChildProcessLauncher to start a
// process. Since ChildProcessLauncher can be deleted by its client at any time,
// this class is used to keep state as the process is started asynchronously.
// It also contains the platform specific pieces.
class ChildProcessLauncherHelper :
    public base::RefCountedThreadSafe<ChildProcessLauncherHelper> {
 public:
  // Abstraction around a process required to deal in a platform independent way
  // between Linux (which can use zygotes) and the other platforms.
  struct Process {
    Process() {}
    Process(Process&& other);
    ~Process() {}
    Process& operator=(Process&& other);

    base::Process process;

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
    service_manager::ZygoteHandle zygote = nullptr;
#endif  // BUILDFLAG(USE_ZYGOTE_HANDLE)
  };

  ChildProcessLauncherHelper(
      int child_process_id,
      std::unique_ptr<base::CommandLine> command_line,
      std::unique_ptr<SandboxedProcessLauncherDelegate> delegate,
      const base::WeakPtr<ChildProcessLauncher>& child_process_launcher,
      bool terminate_on_shutdown,
#if defined(OS_ANDROID)
      bool is_pre_warmup_required,
#endif
      mojo::OutgoingInvitation mojo_invitation,
      const mojo::ProcessErrorCallback& process_error_callback,
      std::map<std::string, base::FilePath> files_to_preload);

  // The methods below are defined in the order they are called.

  // Starts the flow of launching the process.
  void StartLaunchOnClientThread();

  // Platform specific.
  void BeforeLaunchOnClientThread();

#if !defined(OS_FUCHSIA)
  // Called to give implementors a chance at creating a server pipe. Platform-
  // specific. Returns |base::nullopt| if the helper should initialize
  // a regular PlatformChannel for communication instead.
  base::Optional<mojo::NamedPlatformChannel>
  CreateNamedPlatformChannelOnClientThread();
#endif

  // Returns the list of files that should be mapped in the child process.
  // Platform specific.
  std::unique_ptr<FileMappedForLaunch> GetFilesToMap();

  // Platform specific, returns success or failure. If failure is returned,
  // LaunchOnLauncherThread will not call LaunchProcessOnLauncherThread and
  // AfterLaunchOnLauncherThread, and the launch_result will be reported as
  // LAUNCH_RESULT_FAILURE.
  bool BeforeLaunchOnLauncherThread(FileMappedForLaunch& files_to_register,
                                    base::LaunchOptions* options);

  // Does the actual starting of the process.
  // |is_synchronous_launch| is set to false if the starting of the process is
  // asynchonous (this is the case on Android), in which case the returned
  // Process is not valid (and PostLaunchOnLauncherThread() will provide the
  // process once it is available).
  // Platform specific.
  ChildProcessLauncherHelper::Process LaunchProcessOnLauncherThread(
      const base::LaunchOptions& options,
      std::unique_ptr<FileMappedForLaunch> files_to_register,
#if defined(OS_ANDROID)
      bool is_pre_warmup_required,
#endif
      bool* is_synchronous_launch,
      int* launch_result);

  // Called right after the process has been launched, whether it was created
  // successfully or not. If the process launch is asynchronous, the process may
  // not yet be created. Platform specific.
  void AfterLaunchOnLauncherThread(
      const ChildProcessLauncherHelper::Process& process,
      const base::LaunchOptions& options);

  // Called once the process has been created, successfully or not.
  void PostLaunchOnLauncherThread(ChildProcessLauncherHelper::Process process,
                                  int launch_result);

  // Posted by PostLaunchOnLauncherThread onto the client thread.
  void PostLaunchOnClientThread(ChildProcessLauncherHelper::Process process,
                                int error_code);

  // See ChildProcessLauncher::GetChildTerminationInfo for more info.
  ChildProcessTerminationInfo GetTerminationInfo(
      const ChildProcessLauncherHelper::Process& process,
      bool known_dead);

  // Terminates |process|.
  // Returns true if the process was stopped, false if the process had not been
  // started yet or could not be stopped.
  // Note that |exit_code| is not used on Android.
  static bool TerminateProcess(const base::Process& process, int exit_code);

  // Terminates the process with the normal exit code and ensures it has been
  // stopped. By returning a normal exit code this ensures UMA won't treat this
  // as a crash.
  // Returns immediately and perform the work on the launcher thread.
  static void ForceNormalProcessTerminationAsync(
      ChildProcessLauncherHelper::Process process);

  void SetProcessPriorityOnLauncherThread(
      base::Process process,
      const ChildProcessLauncherPriority& priority);

#if defined(OS_ANDROID)
  void OnChildProcessStarted(JNIEnv* env,
                             jint handle);

  // Dumps the stack of the child process without crashing it.
  void DumpProcessStack(const base::Process& process);
#endif  // OS_ANDROID

 private:
  friend class base::RefCountedThreadSafe<ChildProcessLauncherHelper>;

  ~ChildProcessLauncherHelper();

  void LaunchOnLauncherThread();

  base::CommandLine* command_line() { return command_line_.get(); }
  int child_process_id() const { return child_process_id_; }

  std::string GetProcessType();

  static void ForceNormalProcessTerminationSync(
      ChildProcessLauncherHelper::Process process);

#if defined(OS_ANDROID)
  void set_java_peer_available_on_client_thread() {
    java_peer_avaiable_on_client_thread_ = true;
  }
#endif

  const int child_process_id_;
  const scoped_refptr<base::SequencedTaskRunner> client_task_runner_;
  base::TimeTicks begin_launch_time_;
  std::unique_ptr<base::CommandLine> command_line_;
  std::unique_ptr<SandboxedProcessLauncherDelegate> delegate_;
  base::WeakPtr<ChildProcessLauncher> child_process_launcher_;

  // The PlatformChannel that will be used to transmit an invitation to the
  // child process in most cases. Only used if the platform's helper
  // implementation doesn't return a server endpoint from
  // |CreateNamedPlatformChannelOnClientThread()|.
  base::Optional<mojo::PlatformChannel> mojo_channel_;

#if !defined(OS_FUCHSIA)
  // May be used in exclusion to the above if the platform helper implementation
  // returns a valid server endpoint from
  // |CreateNamedPlatformChannelOnClientThread()|.
  base::Optional<mojo::NamedPlatformChannel> mojo_named_channel_;
#endif

  bool terminate_on_shutdown_;
  mojo::OutgoingInvitation mojo_invitation_;
  const mojo::ProcessErrorCallback process_error_callback_;
  const std::map<std::string, base::FilePath> files_to_preload_;

#if defined(OS_MACOSX)
  std::unique_ptr<sandbox::SeatbeltExecClient> seatbelt_exec_client_;
#endif  // defined(OS_MACOSX)

#if defined(OS_ANDROID)
  base::android::ScopedJavaGlobalRef<jobject> java_peer_;
  bool java_peer_avaiable_on_client_thread_ = false;
  // Whether the process can use warmed up connection.
  bool can_use_warm_up_connection_;
#endif

#if defined(OS_FUCHSIA)
  std::unique_ptr<service_manager::SandboxPolicyFuchsia> sandbox_policy_;
#endif
};

}  // namespace internal

}  // namespace content

#endif  // CONTENT_BROWSER_CHILD_PROCESS_LAUNCHER_HELPER_H_
