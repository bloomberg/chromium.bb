// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/chrome_content_utility_client.h"

#include <stddef.h>

#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/profiler/thread_profiler.h"
#include "chrome/common/profiler/thread_profiler_configuration.h"
#include "chrome/utility/browser_exposed_utility_interfaces.h"
#include "chrome/utility/services.h"
#include "content/public/child/child_thread.h"
#include "content/public/common/content_switches.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "sandbox/policy/sandbox_type.h"

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) && defined(OS_WIN)
#include "chrome/utility/printing_handler.h"
#endif

namespace {

base::LazyInstance<ChromeContentUtilityClient::NetworkBinderCreationCallback>::
    Leaky g_network_binder_creation_callback = LAZY_INSTANCE_INITIALIZER;

}  // namespace

ChromeContentUtilityClient::ChromeContentUtilityClient()
    : utility_process_running_elevated_(false) {
}

ChromeContentUtilityClient::~ChromeContentUtilityClient() = default;

void ChromeContentUtilityClient::ExposeInterfacesToBrowser(
    mojo::BinderMap* binders) {
#if defined(OS_WIN)
  auto& cmd_line = *base::CommandLine::ForCurrentProcess();
  auto sandbox_type = sandbox::policy::SandboxTypeFromCommandLine(cmd_line);
  utility_process_running_elevated_ =
      sandbox_type == sandbox::mojom::Sandbox::kNoSandboxAndElevatedPrivileges;
#endif

  // If our process runs with elevated privileges, only add elevated Mojo
  // interfaces to the BinderMap.
  //
  // NOTE: Do not add interfaces directly from within this method. Instead,
  // modify the definition of |ExposeElevatedChromeUtilityInterfacesToBrowser()|
  // to ensure security review coverage.
  if (!utility_process_running_elevated_)
    ExposeElevatedChromeUtilityInterfacesToBrowser(binders);
}

void ChromeContentUtilityClient::RegisterNetworkBinders(
    service_manager::BinderRegistry* registry) {
  if (g_network_binder_creation_callback.Get())
    std::move(g_network_binder_creation_callback.Get()).Run(registry);
}

void ChromeContentUtilityClient::UtilityThreadStarted() {
  // Only builds message pipes for utility processes which enable sampling
  // profilers.
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  const std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);
  if (process_type ==
          switches::kUtilityProcess &&  // An in-process utility thread may run
                                        // in other processes, only set up
                                        // collector in a utility process.
      ThreadProfilerConfiguration::Get()
          ->IsProfilerEnabledForCurrentProcess()) {
    mojo::PendingRemote<metrics::mojom::CallStackProfileCollector> collector;
    content::ChildThread::Get()->BindHostReceiver(
        collector.InitWithNewPipeAndPassReceiver());
    ThreadProfiler::SetCollectorForChildProcess(std::move(collector));
  }
}

void ChromeContentUtilityClient::RegisterMainThreadServices(
    mojo::ServiceFactory& services) {
  if (utility_process_running_elevated_)
    return ::RegisterElevatedMainThreadServices(services);
  return ::RegisterMainThreadServices(services);
}

void ChromeContentUtilityClient::PostIOThreadCreated(
    base::SingleThreadTaskRunner* io_thread_task_runner) {
  io_thread_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&ThreadProfiler::StartOnChildThread,
                                metrics::CallStackProfileParams::Thread::kIo));
}

void ChromeContentUtilityClient::RegisterIOThreadServices(
    mojo::ServiceFactory& services) {
  return ::RegisterIOThreadServices(services);
}

// static
void ChromeContentUtilityClient::SetNetworkBinderCreationCallback(
    NetworkBinderCreationCallback callback) {
  g_network_binder_creation_callback.Get() = std::move(callback);
}
