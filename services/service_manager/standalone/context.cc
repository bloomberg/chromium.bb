// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/standalone/context.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/process/process_info.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "services/catalog/catalog.h"
#include "services/service_manager/connect_params.h"
#include "services/service_manager/connect_util.h"
#include "services/service_manager/runner/common/switches.h"
#include "services/service_manager/runner/host/service_process_launcher_factory.h"
#include "services/service_manager/service_manager.h"
#include "services/service_manager/switches.h"

#if !defined(OS_IOS)
#include "services/service_manager/runner/host/service_process_launcher.h"
#endif

#if defined(OS_MACOSX)
#include "services/service_manager/public/cpp/standalone_service/mach_broker.h"
#endif

namespace base {
class TaskRunner;
}

namespace service_manager {
namespace {

#if !defined(OS_IOS)
// Used to ensure we only init once.
class ServiceProcessLauncherFactoryImpl : public ServiceProcessLauncherFactory {
 public:
  ServiceProcessLauncherFactoryImpl(base::TaskRunner* launch_process_runner,
                                    ServiceProcessLauncherDelegate* delegate)
      : launch_process_runner_(launch_process_runner), delegate_(delegate) {}

 private:
   std::unique_ptr<ServiceProcessLauncher> Create(
      const base::FilePath& service_path) override {
    return base::MakeUnique<ServiceProcessLauncher>(
        launch_process_runner_, delegate_, service_path);
  }

  base::TaskRunner* launch_process_runner_;
  ServiceProcessLauncherDelegate* delegate_;
};
#endif  // !defined(OS_IOS)

void OnInstanceQuit(const std::string& name, const Identity& identity) {
  if (name == identity.name())
    base::MessageLoop::current()->QuitWhenIdle();
}

const char kService[] = "service";

}  // namespace

Context::Context(
    ServiceProcessLauncherDelegate* service_process_launcher_delegate,
    std::unique_ptr<base::Value> catalog_contents)
    : main_entry_time_(base::Time::Now()) {
  TRACE_EVENT0("service_manager", "Context::Context");

  blocking_pool_ = new base::SequencedWorkerPool(
      kThreadPoolMaxThreads, "blocking_pool", base::TaskPriority::USER_VISIBLE);

  std::unique_ptr<ServiceProcessLauncherFactory>
      service_process_launcher_factory;

// iOS does not support launching services in their own processes (and does
// not build ServiceProcessLauncher).
#if !defined(OS_IOS)
  service_process_launcher_factory =
      base::MakeUnique<ServiceProcessLauncherFactoryImpl>(
          blocking_pool_.get(), service_process_launcher_delegate);
#endif
  service_manager_.reset(
      new ServiceManager(std::move(service_process_launcher_factory),
                         std::move(catalog_contents), nullptr));
}

Context::~Context() { blocking_pool_->Shutdown(); }

void Context::RunCommandLineApplication() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kService))
    Run(command_line->GetSwitchValueASCII(kService));
}

void Context::Run(const std::string& name) {
  service_manager_->SetInstanceQuitCallback(base::Bind(&OnInstanceQuit, name));

  std::unique_ptr<ConnectParams> params(new ConnectParams);
  params->set_source(CreateServiceManagerIdentity());
  params->set_target(Identity(name, mojom::kRootUserID));
  service_manager_->Connect(std::move(params));
}

}  // namespace service_manager
