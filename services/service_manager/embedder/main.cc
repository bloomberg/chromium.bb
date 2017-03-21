// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/embedder/main.h"
#include "base/allocator/features.h"
#include "base/command_line.h"
#include "base/debug/activity_tracker.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/process/memory.h"
#include "mojo/edk/embedder/embedder.h"
#include "services/service_manager/embedder/main_delegate.h"
#include "services/service_manager/embedder/set_process_title.h"
#include "services/service_manager/embedder/shared_file_util.h"
#include "services/service_manager/embedder/switches.h"

#if defined(OS_WIN)
#include "base/win/process_startup_helper.h"
#include "ui/base/win/atl_module.h"
#endif

#if defined(OS_POSIX) && !defined(OS_ANDROID)
#include <locale.h>
#include <signal.h>

#include "base/file_descriptor_store.h"
#include "base/posix/global_descriptors.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#include "services/service_manager/embedder/mac_init.h"

#if BUILDFLAG(USE_EXPERIMENTAL_ALLOCATOR_SHIM)
#include "base/allocator/allocator_shim.h"
#endif
#endif  // defined(OS_MACOSX)

namespace service_manager {

namespace {

// Maximum message size allowed to be read from a Mojo message pipe in any
// service manager embedder process.
constexpr size_t kMaximumMojoMessageSize = 128 * 1024 * 1024;

#if defined(OS_POSIX) && !defined(OS_ANDROID)

// Setup signal-handling state: resanitize most signals, ignore SIGPIPE.
void SetupSignalHandlers() {
  // Sanitise our signal handling state. Signals that were ignored by our
  // parent will also be ignored by us. We also inherit our parent's sigmask.
  sigset_t empty_signal_set;
  CHECK_EQ(0, sigemptyset(&empty_signal_set));
  CHECK_EQ(0, sigprocmask(SIG_SETMASK, &empty_signal_set, NULL));

  struct sigaction sigact;
  memset(&sigact, 0, sizeof(sigact));
  sigact.sa_handler = SIG_DFL;
  static const int signals_to_reset[] = {
      SIGHUP,  SIGINT,  SIGQUIT, SIGILL, SIGABRT, SIGFPE, SIGSEGV,
      SIGALRM, SIGTERM, SIGCHLD, SIGBUS, SIGTRAP};  // SIGPIPE is set below.
  for (unsigned i = 0; i < arraysize(signals_to_reset); i++) {
    CHECK_EQ(0, sigaction(signals_to_reset[i], &sigact, NULL));
  }

  // Always ignore SIGPIPE.  We check the return value of write().
  CHECK_NE(SIG_ERR, signal(SIGPIPE, SIG_IGN));
}

void PopulateFDsFromCommandLine() {
  const std::string& shared_file_param =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kSharedFiles);
  if (shared_file_param.empty())
    return;

  base::Optional<std::map<int, std::string>> shared_file_descriptors =
      service_manager::ParseSharedFileSwitchValue(shared_file_param);
  if (!shared_file_descriptors)
    return;

  for (const auto& descriptor : *shared_file_descriptors) {
    base::MemoryMappedFile::Region region;
    const std::string& key = descriptor.second;
    base::ScopedFD fd = base::GlobalDescriptors::GetInstance()->TakeFD(
        descriptor.first, &region);
    base::FileDescriptorStore::GetInstance().Set(key, std::move(fd), region);
  }
}

#endif  // defined(OS_POSIX) && !defined(OS_ANDROID)

}  // namespace

MainParams::MainParams(MainDelegate* delegate) : delegate(delegate) {}

MainParams::~MainParams() {}

int Main(const MainParams& params) {
  MainDelegate* delegate = params.delegate;
  DCHECK(delegate);

#if defined(OS_MACOSX) && BUILDFLAG(USE_EXPERIMENTAL_ALLOCATOR_SHIM)
  base::allocator::InitializeAllocatorShim();
#endif
  base::EnableTerminationOnOutOfMemory();

#if defined(OS_WIN)
  base::win::RegisterInvalidParamHandler();
  ui::win::CreateATLModuleIfNeeded();
#endif  // defined(OS_WIN)

// On Android setlocale() is not supported, and we don't override the signal
// handlers so we can get a stack trace when crashing.
#if defined(OS_POSIX) && !defined(OS_ANDROID)
  // Set C library locale to make sure CommandLine can parse argument values in
  // the correct encoding.
  setlocale(LC_ALL, "");

  SetupSignalHandlers();
#endif

#if !defined(OS_ANDROID)
  // On Android, the command line is initialized when library is loaded.
  int argc = 0;
  const char** argv = nullptr;

#if defined(OS_POSIX)
  // argc/argv are ignored on Windows; see command_line.h for details.
  argc = params.argc;
  argv = params.argv;
#endif

  base::CommandLine::Init(argc, argv);

#if defined(OS_POSIX)
  PopulateFDsFromCommandLine();
#endif

  base::EnableTerminationOnHeapCorruption();

#if defined(OS_WIN)
  base::win::SetupCRT(*base::CommandLine::ForCurrentProcess());
#endif

  SetProcessTitleFromCommandLine(argv);
#endif  // !defined(OS_ANDROID)

  MainDelegate::InitializeParams init_params;

#if defined(OS_MACOSX)
  // We need this pool for all the objects created before we get to the event
  // loop, but we don't want to leave them hanging around until the app quits.
  // Each "main" needs to flush this pool right before it goes into its main
  // event loop to get rid of the cruft.
  std::unique_ptr<base::mac::ScopedNSAutoreleasePool> autorelease_pool =
      base::MakeUnique<base::mac::ScopedNSAutoreleasePool>();
  init_params.autorelease_pool = autorelease_pool.get();
  InitializeMac();
#endif

  mojo::edk::SetMaxMessageSize(kMaximumMojoMessageSize);
  mojo::edk::Init();

  base::debug::GlobalActivityTracker* tracker =
      base::debug::GlobalActivityTracker::Get();
  int exit_code = delegate->Initialize(init_params);
  if (exit_code >= 0) {
    if (tracker) {
      tracker->SetProcessPhase(
          base::debug::GlobalActivityTracker::PROCESS_LAUNCH_FAILED);
      tracker->process_data().SetInt("exit-code", exit_code);
    }
    return exit_code;
  }

  exit_code = delegate->Run();
  if (tracker) {
    if (exit_code == 0) {
      tracker->SetProcessPhaseIfEnabled(
          base::debug::GlobalActivityTracker::PROCESS_EXITED_CLEANLY);
    } else {
      tracker->SetProcessPhaseIfEnabled(
          base::debug::GlobalActivityTracker::PROCESS_EXITED_WITH_CODE);
      tracker->process_data().SetInt("exit-code", exit_code);
    }
  }

#if defined(OS_MACOSX)
  autorelease_pool.reset();
#endif

  delegate->ShutDown();

  return exit_code;
}

}  // namespace service_manager
