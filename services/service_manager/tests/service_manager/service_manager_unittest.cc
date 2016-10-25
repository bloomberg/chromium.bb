// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_test.h"
#include "services/service_manager/public/interfaces/service_manager.mojom.h"
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
    base::MessageLoop::current()->QuitWhenIdle();
  }

  service_manager::Identity target_identity_;

  mojo::Binding<test::mojom::CreateInstanceTest> binding_;

  DISALLOW_COPY_AND_ASSIGN(ServiceManagerTestClient);
};

}  // namespace

class ServiceManagerTest : public test::ServiceTest,
                           public mojom::ServiceManagerListener {
 public:
  ServiceManagerTest()
      : test::ServiceTest("service:service_manager_unittest"),
        service_(nullptr),
        binding_(this) {}
  ~ServiceManagerTest() override {}

  void OnDriverQuit() { base::MessageLoop::current()->QuitNow(); }

 protected:
  struct InstanceInfo {
    explicit InstanceInfo(const Identity& identity)
        : identity(identity), pid(base::kNullProcessId) {}

    Identity identity;
    base::ProcessId pid;
  };

  void AddListenerAndWaitForApplications() {
    mojom::ServiceManagerPtr service_manager;
    connector()->ConnectToInterface("service:service_manager",
                                    &service_manager);

    service_manager->AddListener(binding_.CreateInterfacePtrAndBind());

    wait_for_instances_loop_.reset(new base::RunLoop);
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

  const Identity& target_identity() const {
    DCHECK(service_);
    return service_->target_identity();
  }

  const std::vector<InstanceInfo>& instances() const { return instances_; }

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

  ServiceManagerTestClient* service_;
  mojo::Binding<mojom::ServiceManagerListener> binding_;
  std::vector<InstanceInfo> instances_;
  std::vector<InstanceInfo> initial_instances_;
  std::unique_ptr<base::RunLoop> wait_for_instances_loop_;

  DISALLOW_COPY_AND_ASSIGN(ServiceManagerTest);
};

TEST_F(ServiceManagerTest, CreateInstance) {
  AddListenerAndWaitForApplications();

  // 1. Launch a process. (Actually, have the runner launch a process that
  //    launches a process.)
  test::mojom::DriverPtr driver;
  std::unique_ptr<Connection> connection =
      connector()->Connect("exe:service_manager_unittest_driver");
  connection->GetInterface(&driver);

  // 2. Wait for the target to connect to us. (via
  //    service:service_manager_unittest)
  base::RunLoop().Run();

  EXPECT_FALSE(connection->IsPending());
  Identity remote_identity = connection->GetRemoteIdentity();

  // 3. Validate that this test suite's name was received from the application
  //    manager.
  EXPECT_TRUE(ContainsInstanceWithName("service:service_manager_unittest"));

  // 4. Validate that the right applications/processes were created.
  //    Note that the target process will be created even if the tests are
  //    run with --single-process.
  EXPECT_EQ(2u, instances().size());
  {
    auto& instance = instances().front();
    EXPECT_EQ(remote_identity, instance.identity);
    EXPECT_EQ("exe:service_manager_unittest_driver", instance.identity.name());
    EXPECT_NE(base::kNullProcessId, instance.pid);
  }
  {
    auto& instance = instances().back();
    // We learn about the target process id via a ping from it.
    EXPECT_EQ(target_identity(), instance.identity);
    EXPECT_EQ("exe:service_manager_unittest_target", instance.identity.name());
    EXPECT_NE(base::kNullProcessId, instance.pid);
  }

  driver.set_connection_error_handler(
      base::Bind(&ServiceManagerTest::OnDriverQuit, base::Unretained(this)));
  driver->QuitDriver();
  base::RunLoop().Run();
}

}  // namespace service_manager
