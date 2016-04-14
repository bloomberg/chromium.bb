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
#include "services/shell/public/cpp/interface_factory.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/shell/public/cpp/shell_test.h"
#include "services/shell/public/interfaces/shell.mojom.h"
#include "services/shell/tests/shell/shell_unittest.mojom.h"

namespace shell {

namespace {

class ShellTestClient
    : public test::ShellTestClient,
      public InterfaceFactory<test::mojom::CreateInstanceTest>,
      public test::mojom::CreateInstanceTest {
 public:
  explicit ShellTestClient(test::ShellTest* test)
      : test::ShellTestClient(test),
        target_id_(shell::mojom::kInvalidInstanceID),
        binding_(this) {}
  ~ShellTestClient() override {}

  uint32_t target_id() const { return target_id_; }

 private:
  // test::ShellTestClient:
  bool AcceptConnection(Connection* connection) override {
    connection->AddInterface<test::mojom::CreateInstanceTest>(this);
    return true;
  }

  // InterfaceFactory<test::mojom::CreateInstanceTest>:
  void Create(
      Connection* connection,
      test::mojom::CreateInstanceTestRequest request) override {
    binding_.Bind(std::move(request));
  }

  // test::mojom::CreateInstanceTest:
  void SetTargetID(uint32_t target_id) override {
    target_id_ = target_id;
    base::MessageLoop::current()->QuitWhenIdle();
  }

  uint32_t target_id_;

  mojo::Binding<test::mojom::CreateInstanceTest> binding_;

  DISALLOW_COPY_AND_ASSIGN(ShellTestClient);
};

}  // namespace

class ShellTest : public test::ShellTest, public mojom::InstanceListener {
 public:
  ShellTest()
      : test::ShellTest("mojo:shell_unittest"),
        shell_client_(nullptr),
        binding_(this) {}
  ~ShellTest() override {}

  void OnDriverQuit() {
    base::MessageLoop::current()->QuitNow();
  }

 protected:
  struct InstanceInfo {
    InstanceInfo(uint32_t id, const std::string& name)
        : id(id), name(name), pid(base::kNullProcessId) {}

    uint32_t id;
    std::string name;
    base::ProcessId pid;
  };

  void AddListenerAndWaitForApplications() {
    mojom::ShellPtr shell;
    connector()->ConnectToInterface("mojo:shell", &shell);

    shell->AddInstanceListener(binding_.CreateInterfacePtrAndBind());

    wait_for_instances_loop_.reset(new base::RunLoop);
    wait_for_instances_loop_->Run();
  }

  bool ContainsInstanceWithName(const std::string& name) const {
    for (const auto& instance : initial_instances_) {
      if (instance.name == name)
        return true;
    }
    for (const auto& instance : instances_) {
      if (instance.name == name)
        return true;
    }
    return false;
  }

  uint32_t target_id() const {
    DCHECK(shell_client_);
    return shell_client_->target_id();
  }

  const std::vector<InstanceInfo>& instances() const {
    return instances_;
  }

 private:
  // test::ShellTest:
  std::unique_ptr<ShellClient> CreateShellClient() override {
    shell_client_ = new ShellTestClient(this);
    return base::WrapUnique(shell_client_);
  }

  // mojom::InstanceListener:
  void SetExistingInstances(
      mojo::Array<mojom::InstanceInfoPtr> instances) override {
    for (size_t i = 0; i < instances.size(); ++i) {
      initial_instances_.push_back(InstanceInfo(instances[i]->id,
                                                instances[i]->identity->name));
    }

    DCHECK(wait_for_instances_loop_);
    wait_for_instances_loop_->Quit();
  }

  void InstanceCreated(mojom::InstanceInfoPtr instance) override {
    instances_.push_back(InstanceInfo(instance->id, instance->identity->name));
  }

  void InstanceDestroyed(uint32_t id) override {
    for (auto it = instances_.begin(); it != instances_.end(); ++it) {
      auto& instance = *it;
      if (instance.id == id) {
        instances_.erase(it);
        break;
      }
    }
  }

  void InstancePIDAvailable(uint32_t id, uint32_t pid) override {
    for (auto& instance : instances_) {
      if (instance.id == id) {
        instance.pid = pid;
        break;
      }
    }
  }

  ShellTestClient* shell_client_;
  mojo::Binding<mojom::InstanceListener> binding_;
  std::vector<InstanceInfo> instances_;
  std::vector<InstanceInfo> initial_instances_;
  std::unique_ptr<base::RunLoop> wait_for_instances_loop_;

  DISALLOW_COPY_AND_ASSIGN(ShellTest);
};

TEST_F(ShellTest, CreateInstance) {
  AddListenerAndWaitForApplications();

  // 1. Launch a process. (Actually, have the runner launch a process that
  //    launches a process.)
  test::mojom::DriverPtr driver;
  std::unique_ptr<Connection> connection =
      connector()->Connect("exe:shell_unittest_driver");
  connection->GetInterface(&driver);

  // 2. Wait for the target to connect to us. (via
  //    mojo:shell_unittest)
  base::MessageLoop::current()->Run();

  EXPECT_FALSE(connection->IsPending());
  uint32_t remote_id = connection->GetRemoteInstanceID();
  EXPECT_NE(mojom::kInvalidInstanceID, remote_id);

  // 3. Validate that this test suite's name was received from the application
  //    manager.
  EXPECT_TRUE(ContainsInstanceWithName("mojo:shell_unittest"));

  // 4. Validate that the right applications/processes were created.
  //    Note that the target process will be created even if the tests are
  //    run with --single-process.
  EXPECT_EQ(2u, instances().size());
  {
    auto& instance = instances().front();
    EXPECT_EQ(remote_id, instance.id);
    EXPECT_EQ("exe:shell_unittest_driver", instance.name);
    EXPECT_NE(base::kNullProcessId, instance.pid);
  }
  {
    auto& instance = instances().back();
    // We learn about the target process id via a ping from it.
    EXPECT_EQ(target_id(), instance.id);
    EXPECT_EQ("exe:shell_unittest_target", instance.name);
    EXPECT_NE(base::kNullProcessId, instance.pid);
  }

  driver.set_connection_error_handler(
      base::Bind(&ShellTest::OnDriverQuit,
                 base::Unretained(this)));
  driver->QuitDriver();
  base::MessageLoop::current()->Run();
}

}  // namespace shell
