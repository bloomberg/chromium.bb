// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/pending_process_connection.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_test.h"
#include "services/service_manager/public/interfaces/constants.mojom.h"
#include "services/service_manager/public/interfaces/service_manager.mojom.h"
#include "services/service_manager/runner/common/client_util.h"
#include "services/service_manager/tests/service_manager/service_manager_unittest.mojom.h"

namespace service_manager {

namespace {

class ServiceManagerTestClient
    : public test::ServiceTestClient,
      public InterfaceFactory<test::mojom::CreateInstanceTest>,
      public test::mojom::CreateInstanceTest {
 public:
  explicit ServiceManagerTestClient(test::ServiceTest* test)
      : test::ServiceTestClient(test), binding_(this) {}
  ~ServiceManagerTestClient() override {}

  const Identity& target_identity() const { return target_identity_; }

  void WaitForTargetIdentityCall() {
    wait_for_target_identity_loop_ = base::MakeUnique<base::RunLoop>();
    wait_for_target_identity_loop_->Run();
  }

 private:
  // test::ServiceTestClient:
  bool OnConnect(const ServiceInfo& remote_info,
                 InterfaceRegistry* registry) override {
    registry->AddInterface<test::mojom::CreateInstanceTest>(this);
    return true;
  }

  // InterfaceFactory<test::mojom::CreateInstanceTest>:
  void Create(const Identity& remote_identity,
              test::mojom::CreateInstanceTestRequest request) override {
    binding_.Bind(std::move(request));
  }

  // test::mojom::CreateInstanceTest:
  void SetTargetIdentity(const service_manager::Identity& identity) override {
    target_identity_ = identity;
    if (!wait_for_target_identity_loop_)
      LOG(ERROR) << "SetTargetIdentity call received when not waiting for it.";
    else
      wait_for_target_identity_loop_->Quit();
  }

  service_manager::Identity target_identity_;
  std::unique_ptr<base::RunLoop> wait_for_target_identity_loop_;

  mojo::Binding<test::mojom::CreateInstanceTest> binding_;

  DISALLOW_COPY_AND_ASSIGN(ServiceManagerTestClient);
};

}  // namespace

class ServiceManagerTest : public test::ServiceTest,
                           public mojom::ServiceManagerListener {
 public:
  ServiceManagerTest()
      : test::ServiceTest("service_manager_unittest"),
        service_(nullptr),
        binding_(this) {}
  ~ServiceManagerTest() override {}

 protected:
  struct InstanceInfo {
    explicit InstanceInfo(const Identity& identity)
        : identity(identity), pid(base::kNullProcessId) {}

    Identity identity;
    base::ProcessId pid;
  };

  void AddListenerAndWaitForApplications() {
    mojom::ServiceManagerPtr service_manager;
    connector()->BindInterface(service_manager::mojom::kServiceName,
                               &service_manager);

    service_manager->AddListener(binding_.CreateInterfacePtrAndBind());

    wait_for_instances_loop_ = base::MakeUnique<base::RunLoop>();
    wait_for_instances_loop_->Run();
  }

  bool ContainsInstanceWithName(const std::string& name) const {
    for (const auto& instance : initial_instances_) {
      if (instance.identity.name() == name)
        return true;
    }
    for (const auto& instance : instances_) {
      if (instance.identity.name() == name)
        return true;
    }
    return false;
  }

  void WaitForTargetIdentityCall() {
    service_->WaitForTargetIdentityCall();
  }

  const Identity& target_identity() const {
    DCHECK(service_);
    return service_->target_identity();
  }

  const std::vector<InstanceInfo>& instances() const { return instances_; }

  using ServiceStartedCallback =
      base::Callback<void(const service_manager::Identity&)>;
  void set_service_started_callback(const ServiceStartedCallback& callback) {
    service_started_callback_ = callback;
  }

  using ServiceFailedToStartCallback =
      base::Callback<void(const service_manager::Identity&)>;
  void set_service_failed_to_start_callback(
      const ServiceFailedToStartCallback& callback) {
    service_failed_to_start_callback_ = callback;
  }

  void StartTarget() {
    base::FilePath target_path;
    CHECK(base::PathService::Get(base::DIR_EXE, &target_path));
#if defined(OS_WIN)
    target_path = target_path.Append(
        FILE_PATH_LITERAL("service_manager_unittest_target.exe"));
#else
    target_path = target_path.Append(
        FILE_PATH_LITERAL("service_manager_unittest_target"));
#endif

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

    mojo::edk::PendingProcessConnection process_connection;
    service_manager::mojom::ServicePtr client =
        service_manager::PassServiceRequestOnCommandLine(&process_connection,
                                                         &child_command_line);
    service_manager::mojom::PIDReceiverPtr receiver;

    service_manager::Identity target("service_manager_unittest_target",
                                     service_manager::mojom::kInheritUserID);
    connector()->StartService(target, std::move(client),
                              MakeRequest(&receiver));
    std::unique_ptr<service_manager::Connection> connection =
        connector()->Connect(target);
    connection->AddConnectionCompletedClosure(
        base::Bind(&ServiceManagerTest::OnConnectionCompleted,
                   base::Unretained(this)));

    base::LaunchOptions options;
#if defined(OS_WIN)
    options.handles_to_inherit = &handle_passing_info;
#elif defined(OS_POSIX)
    options.fds_to_remap = &handle_passing_info;
#endif
    target_ = base::LaunchProcess(child_command_line, options);
    DCHECK(target_.IsValid());
    receiver->SetPID(target_.Pid());
    process_connection.Connect(target_.Handle(),
                               platform_channel_pair.PassServerHandle());
  }

  void KillTarget() {
    target_.Terminate(0, false);
  }

 private:
  // test::ServiceTest:
  std::unique_ptr<Service> CreateService() override {
    service_ = new ServiceManagerTestClient(this);
    return base::WrapUnique(service_);
  }

  // mojom::ServiceManagerListener:
  void OnInit(std::vector<mojom::RunningServiceInfoPtr> instances) override {
    for (size_t i = 0; i < instances.size(); ++i)
      initial_instances_.push_back(InstanceInfo(instances[i]->identity));

    DCHECK(wait_for_instances_loop_);
    wait_for_instances_loop_->Quit();
  }
  void OnServiceCreated(mojom::RunningServiceInfoPtr instance) override {
    instances_.push_back(InstanceInfo(instance->identity));
  }
  void OnServiceStarted(const service_manager::Identity& identity,
                        uint32_t pid) override {
    for (auto& instance : instances_) {
      if (instance.identity == identity) {
        instance.pid = pid;
        break;
      }
    }
    if (!service_started_callback_.is_null())
        service_started_callback_.Run(identity);
  }
  void OnServiceFailedToStart(
      const service_manager::Identity& identity) override {
    if (!service_failed_to_start_callback_.is_null())
        service_failed_to_start_callback_.Run(identity);
  }
  void OnServiceStopped(const service_manager::Identity& identity) override {
    for (auto it = instances_.begin(); it != instances_.end(); ++it) {
      auto& instance = *it;
      if (instance.identity == identity) {
        instances_.erase(it);
        break;
      }
    }
  }

  void OnConnectionCompleted() {}

  ServiceManagerTestClient* service_;
  mojo::Binding<mojom::ServiceManagerListener> binding_;
  std::vector<InstanceInfo> instances_;
  std::vector<InstanceInfo> initial_instances_;
  std::unique_ptr<base::RunLoop> wait_for_instances_loop_;
  ServiceStartedCallback service_started_callback_;
  ServiceFailedToStartCallback service_failed_to_start_callback_;
  base::Process target_;

  DISALLOW_COPY_AND_ASSIGN(ServiceManagerTest);
};

TEST_F(ServiceManagerTest, CreateInstance) {
  AddListenerAndWaitForApplications();

  // 1. Launch a process.
  StartTarget();

  // 2. Wait for the target to connect to us. (via
  //    service:service_manager_unittest)
  WaitForTargetIdentityCall();

  // 3. Validate that this test suite's name was received from the application
  //    manager.
  EXPECT_TRUE(ContainsInstanceWithName("service_manager_unittest"));

  // 4. Validate that the right applications/processes were created.
  //    Note that the target process will be created even if the tests are
  //    run with --single-process.
  EXPECT_EQ(1u, instances().size());
  {
    auto& instance = instances().back();
    // We learn about the target process id via a ping from it.
    EXPECT_EQ(target_identity(), instance.identity);
    EXPECT_EQ("service_manager_unittest_target",
              instance.identity.name());
    EXPECT_NE(base::kNullProcessId, instance.pid);
  }

  KillTarget();
}

void OnServiceStartedCallback(int* start_count,
                              std::string* service_name,
                              const base::Closure& continuation,
                              const service_manager::Identity& identity) {
  (*start_count)++;
  *service_name = identity.name();
  continuation.Run();
}

void OnServiceFailedToStartCallback(
      bool* run,
      const base::Closure& continuation,
      const service_manager::Identity& identity) {
  *run = true;
  continuation.Run();
}

// Tests that creating connecting to a singleton packaged service work.
TEST_F(ServiceManagerTest, CreatePackagedSingletonInstance) {
  AddListenerAndWaitForApplications();

  // Connect to the embedder service first.
  {
    base::RunLoop loop;
    int start_count = 0;
    std::string service_name;
    set_service_started_callback(base::BindRepeating(
        &OnServiceStartedCallback,
        &start_count, &service_name, loop.QuitClosure()));
    bool failed_to_start = false;
    set_service_failed_to_start_callback(base::BindRepeating(
        &OnServiceFailedToStartCallback,
        &failed_to_start, loop.QuitClosure()));

    std::unique_ptr<Connection> embedder_connection =
        connector()->Connect("service_manager_unittest_embedder");
    loop.Run();
    EXPECT_FALSE(failed_to_start);
    EXPECT_FALSE(embedder_connection->IsPending());
    EXPECT_EQ(1, start_count);
    EXPECT_EQ("service_manager_unittest_embedder", service_name);
  }

  {
    base::RunLoop loop;
    int start_count = 0;
    std::string service_name;
    set_service_started_callback(base::BindRepeating(
        &OnServiceStartedCallback,
        &start_count, &service_name, loop.QuitClosure()));
    bool failed_to_start = false;
    set_service_failed_to_start_callback(base::BindRepeating(
        &OnServiceFailedToStartCallback,
        &failed_to_start, loop.QuitClosure()));

    // Connect to the packaged singleton service.
    std::unique_ptr<Connection> singleton_connection =
        connector()->Connect("service_manager_unittest_singleton");
    loop.Run();
    EXPECT_FALSE(failed_to_start);
    EXPECT_FALSE(singleton_connection->IsPending());
    EXPECT_EQ(1, start_count);
    EXPECT_EQ("service_manager_unittest_singleton", service_name);
  }
}

}  // namespace service_manager
