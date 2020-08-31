// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/utility/shell_content_utility_client.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/no_destructor.h"
#include "base/process/process.h"
#include "build/build_config.h"
#include "components/services/storage/test_api/test_api.h"
#include "content/public/child/child_thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_service.h"
#include "content/public/test/test_service.mojom.h"
#include "content/public/utility/utility_thread.h"
#include "content/shell/common/power_monitor_test_impl.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/service_factory.h"
#include "mojo/public/cpp/system/buffer.h"
#include "services/service_manager/sandbox/sandbox.h"
#include "services/test/echo/echo_service.h"

#if defined(OS_LINUX)
#include "services/service_manager/tests/sandbox_status_service.h"
#endif

namespace content {

namespace {

class TestUtilityServiceImpl : public mojom::TestService {
 public:
  static void Create(mojo::PendingReceiver<mojom::TestService> receiver) {
    mojo::MakeSelfOwnedReceiver(base::WrapUnique(new TestUtilityServiceImpl),
                                std::move(receiver));
  }

  // mojom::TestService implementation:
  void DoSomething(DoSomethingCallback callback) override {
    std::move(callback).Run();
  }

  void DoTerminateProcess(DoTerminateProcessCallback callback) override {
    base::Process::TerminateCurrentProcessImmediately(0);
  }

  void DoCrashImmediately(DoCrashImmediatelyCallback callback) override {
    IMMEDIATE_CRASH();
  }

  void CreateFolder(CreateFolderCallback callback) override {
    // Note: This is used to check if the sandbox is disabled or not since
    //       creating a folder is forbidden when it is enabled.
    std::move(callback).Run(base::ScopedTempDir().CreateUniqueTempDir());
  }

  void GetRequestorName(GetRequestorNameCallback callback) override {
    NOTREACHED();
  }

  void CreateReadOnlySharedMemoryRegion(
      const std::string& message,
      CreateReadOnlySharedMemoryRegionCallback callback) override {
    base::MappedReadOnlyRegion map_and_region =
        base::ReadOnlySharedMemoryRegion::Create(message.size());
    CHECK(map_and_region.IsValid());
    std::copy(message.begin(), message.end(),
              map_and_region.mapping.GetMemoryAsSpan<char>().begin());
    std::move(callback).Run(std::move(map_and_region.region));
  }

  void CreateWritableSharedMemoryRegion(
      const std::string& message,
      CreateWritableSharedMemoryRegionCallback callback) override {
    auto region = base::WritableSharedMemoryRegion::Create(message.size());
    CHECK(region.IsValid());
    base::WritableSharedMemoryMapping mapping = region.Map();
    CHECK(mapping.IsValid());
    std::copy(message.begin(), message.end(),
              mapping.GetMemoryAsSpan<char>().begin());
    std::move(callback).Run(std::move(region));
  }

  void CreateUnsafeSharedMemoryRegion(
      const std::string& message,
      CreateUnsafeSharedMemoryRegionCallback callback) override {
    auto region = base::UnsafeSharedMemoryRegion::Create(message.size());
    CHECK(region.IsValid());
    base::WritableSharedMemoryMapping mapping = region.Map();
    CHECK(mapping.IsValid());
    std::copy(message.begin(), message.end(),
              mapping.GetMemoryAsSpan<char>().begin());
    std::move(callback).Run(std::move(region));
  }

  void IsProcessSandboxed(IsProcessSandboxedCallback callback) override {
    std::move(callback).Run(service_manager::Sandbox::IsProcessSandboxed());
  }

 private:
  TestUtilityServiceImpl() = default;

  DISALLOW_COPY_AND_ASSIGN(TestUtilityServiceImpl);
};

auto RunEchoService(mojo::PendingReceiver<echo::mojom::EchoService> receiver) {
  return std::make_unique<echo::EchoService>(std::move(receiver));
}

}  // namespace

ShellContentUtilityClient::ShellContentUtilityClient(bool is_browsertest) {
  if (is_browsertest &&
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessType) == switches::kUtilityProcess) {
    network_service_test_helper_ = std::make_unique<NetworkServiceTestHelper>();
    audio_service_test_helper_ = std::make_unique<AudioServiceTestHelper>();
    storage::InjectTestApiImplementation();
    register_sandbox_status_helper_ = true;
  }
}

ShellContentUtilityClient::~ShellContentUtilityClient() = default;

void ShellContentUtilityClient::ExposeInterfacesToBrowser(
    mojo::BinderMap* binders) {
  binders->Add(base::BindRepeating(&TestUtilityServiceImpl::Create),
               base::ThreadTaskRunnerHandle::Get());
  binders->Add<mojom::PowerMonitorTest>(
      base::BindRepeating(&PowerMonitorTestImpl::MakeSelfOwnedReceiver),
      base::ThreadTaskRunnerHandle::Get());
#if defined(OS_LINUX)
  if (register_sandbox_status_helper_) {
    binders->Add<service_manager::mojom::SandboxStatusService>(
        base::BindRepeating(
            &service_manager::SandboxStatusService::MakeSelfOwnedReceiver),
        base::ThreadTaskRunnerHandle::Get());
  }
#endif
}

bool ShellContentUtilityClient::HandleServiceRequest(
    const std::string& service_name,
    service_manager::mojom::ServiceRequest request) {
  std::unique_ptr<service_manager::Service> service;
  if (service_name == kTestServiceUrl) {
    service = std::make_unique<TestService>(std::move(request));
  }

  if (service) {
    service_manager::Service::RunAsyncUntilTermination(
        std::move(service), base::BindOnce([] {
          content::UtilityThread::Get()->ReleaseProcess();
        }));
    return true;
  }

  return false;
}

mojo::ServiceFactory* ShellContentUtilityClient::GetIOThreadServiceFactory() {
  static base::NoDestructor<mojo::ServiceFactory> factory{
      RunEchoService,
  };
  return factory.get();
}

void ShellContentUtilityClient::RegisterNetworkBinders(
    service_manager::BinderRegistry* registry) {
  if (network_service_test_helper_)
    network_service_test_helper_->RegisterNetworkBinders(registry);
}

}  // namespace content
