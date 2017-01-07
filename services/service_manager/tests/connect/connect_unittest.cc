// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/test/test_suite.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/service_manager/public/cpp/service_test.h"
#include "services/service_manager/public/interfaces/service_manager.mojom.h"
#include "services/service_manager/tests/connect/connect_test.mojom.h"
#include "services/service_manager/tests/util.h"

// Tests that multiple services can be packaged in a single service by
// implementing ServiceFactory; that these services can be specified by
// the package's manifest and are thus registered with the PackageManager.

namespace service_manager {

namespace {

const char kTestPackageName[] = "connect_test_package";
const char kTestAppName[] = "connect_test_app";
const char kTestAppAName[] = "connect_test_a";
const char kTestAppBName[] = "connect_test_b";
const char kTestClassAppName[] = "connect_test_class_app";
const char kTestSingletonAppName[] = "connect_test_singleton_app";

void ReceiveOneString(std::string* out_string,
                      base::RunLoop* loop,
                      const std::string& in_string) {
  *out_string = in_string;
  loop->Quit();
}

void ReceiveTwoStrings(std::string* out_string_1,
                       std::string* out_string_2,
                       base::RunLoop* loop,
                       const std::string& in_string_1,
                       const std::string& in_string_2) {
  *out_string_1 = in_string_1;
  *out_string_2 = in_string_2;
  loop->Quit();
}

void ReceiveConnectionResult(mojom::ConnectResult* out_result,
                             Identity* out_target,
                             base::RunLoop* loop,
                             int32_t in_result,
                             const service_manager::Identity& in_identity) {
  *out_result = static_cast<mojom::ConnectResult>(in_result);
  *out_target = in_identity;
  loop->Quit();
}

void QuitLoop(base::RunLoop* loop) {
  loop->Quit();
}

}  // namespace

class ConnectTest : public test::ServiceTest,
                    public InterfaceFactory<test::mojom::ExposedInterface>,
                    public test::mojom::ExposedInterface {
 public:
  ConnectTest() : ServiceTest("connect_unittests") {}
  ~ConnectTest() override {}

 protected:
  std::unique_ptr<Connection> ConnectTo(const Identity& target) {
    std::unique_ptr<Connection> connection = connector()->Connect(target);
    base::RunLoop loop;
    connection->AddConnectionCompletedClosure(base::Bind(&QuitLoop, &loop));
    loop.Run();
    return connection;
  }

  void CompareConnectionState(
      const std::string& connection_local_name,
      const std::string& connection_remote_name,
      const std::string& connection_remote_userid,
      const std::string& initialize_local_name,
      const std::string& initialize_userid) {
    EXPECT_EQ(connection_remote_name,
              connection_state_->connection_remote_name);
    EXPECT_EQ(connection_remote_userid,
              connection_state_->connection_remote_userid);
    EXPECT_EQ(initialize_local_name, connection_state_->initialize_local_name);
    EXPECT_EQ(initialize_userid, connection_state_->initialize_userid);
  }

 private:
  class TestService : public test::ServiceTestClient {
   public:
    explicit TestService(ConnectTest* connect_test)
        : test::ServiceTestClient(connect_test),
          connect_test_(connect_test) {}
    ~TestService() override {}

   private:
    bool OnConnect(const ServiceInfo& remote_info,
                   InterfaceRegistry* registry) override {
      registry->AddInterface<test::mojom::ExposedInterface>(connect_test_);
      return true;
    }

    ConnectTest* connect_test_;

    DISALLOW_COPY_AND_ASSIGN(TestService);
  };

  // test::ServiceTest:
  void SetUp() override {
    test::ServiceTest::SetUp();
    // We need to connect to the package first to force the service manager to
    // read the
    // package app's manifest and register aliases for the applications it
    // provides.
    test::mojom::ConnectTestServicePtr root_service;
    std::unique_ptr<Connection> connection =
        connector()->Connect(kTestPackageName);
    connection->GetInterface(&root_service);
    base::RunLoop run_loop;
    std::string root_name;
    root_service->GetTitle(
        base::Bind(&ReceiveOneString, &root_name, &run_loop));
    run_loop.Run();
  }
  std::unique_ptr<Service> CreateService() override {
    return base::MakeUnique<TestService>(this);
  }

  // InterfaceFactory<test::mojom::ExposedInterface>:
  void Create(const Identity& remote_identity,
              test::mojom::ExposedInterfaceRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  void ConnectionAccepted(test::mojom::ConnectionStatePtr state) override {
    connection_state_ = std::move(state);
  }

  test::mojom::ConnectionStatePtr connection_state_;

  mojo::BindingSet<test::mojom::ExposedInterface> bindings_;

  DISALLOW_COPY_AND_ASSIGN(ConnectTest);
};

// Ensure the connection was properly established and that a round trip
// method call/response is completed.
TEST_F(ConnectTest, Connect) {
  std::unique_ptr<Connection> connection = connector()->Connect(kTestAppName);
  test::mojom::ConnectTestServicePtr service;
  connection->GetInterface(&service);
  base::RunLoop run_loop;
  std::string title;
  service->GetTitle(base::Bind(&ReceiveOneString, &title, &run_loop));
  run_loop.Run();
  EXPECT_EQ("APP", title);
  EXPECT_FALSE(connection->IsPending());
  EXPECT_EQ(connection->GetRemoteIdentity().name(), kTestAppName);
}

TEST_F(ConnectTest, Instances) {
  Identity identity_a(kTestAppName, mojom::kInheritUserID, "A");
  std::unique_ptr<Connection> connection_a1 = ConnectTo(identity_a);
  std::unique_ptr<Connection> connection_a2 = ConnectTo(identity_a);
  std::string instance_a1, instance_a2;
  test::mojom::ConnectTestServicePtr service_a1;
  {
    connection_a1->GetInterface(&service_a1);
    base::RunLoop loop;
    service_a1->GetInstance(base::Bind(&ReceiveOneString, &instance_a1, &loop));
    loop.Run();
  }
  test::mojom::ConnectTestServicePtr service_a2;
  {
    connection_a2->GetInterface(&service_a2);
    base::RunLoop loop;
    service_a2->GetInstance(base::Bind(&ReceiveOneString, &instance_a2, &loop));
    loop.Run();
  }
  EXPECT_EQ(instance_a1, instance_a2);

  Identity identity_b(kTestAppName, mojom::kInheritUserID, "B");
  std::unique_ptr<Connection> connection_b = ConnectTo(identity_b);
  std::string instance_b;
  test::mojom::ConnectTestServicePtr service_b;
  {
    connection_b->GetInterface(&service_b);
    base::RunLoop loop;
    service_b->GetInstance(base::Bind(&ReceiveOneString, &instance_b, &loop));
    loop.Run();
  }

  EXPECT_NE(instance_a1, instance_b);
}

// BlockedInterface should not be exposed to this application because it is not
// in our CapabilityFilter whitelist.
TEST_F(ConnectTest, BlockedInterface) {
  std::unique_ptr<Connection> connection = connector()->Connect(kTestAppName);
  base::RunLoop run_loop;
  test::mojom::BlockedInterfacePtr blocked;
  connection->GetInterface(&blocked);
  blocked.set_connection_error_handler(base::Bind(&QuitLoop, &run_loop));
  std::string title = "unchanged";
  blocked->GetTitleBlocked(base::Bind(&ReceiveOneString, &title, &run_loop));
  run_loop.Run();
  EXPECT_EQ("unchanged", title);
}

// Connects to an app provided by a package.
TEST_F(ConnectTest, PackagedApp) {
  std::unique_ptr<Connection> connection = connector()->Connect(kTestAppAName);
  test::mojom::ConnectTestServicePtr service_a;
  connection->GetInterface(&service_a);
  base::RunLoop run_loop;
  std::string a_name;
  service_a->GetTitle(base::Bind(&ReceiveOneString, &a_name, &run_loop));
  run_loop.Run();
  EXPECT_EQ("A", a_name);
  EXPECT_FALSE(connection->IsPending());
  EXPECT_EQ(connection->GetRemoteIdentity().name(), kTestAppAName);
}

// Ask the target application to attempt to connect to a third application
// provided by a package whose id is permitted by the primary target's
// CapabilityFilter but whose package is not. The connection should be
// allowed regardless of the target's CapabilityFilter with respect to the
// package.
TEST_F(ConnectTest, BlockedPackage) {
  std::unique_ptr<Connection> connection = connector()->Connect(kTestAppName);
  test::mojom::StandaloneAppPtr standalone_app;
  connection->GetInterface(&standalone_app);
  base::RunLoop run_loop;
  std::string title;
  standalone_app->ConnectToAllowedAppInBlockedPackage(
      base::Bind(&ReceiveOneString, &title, &run_loop));
  run_loop.Run();
  EXPECT_EQ("A", title);
}

// BlockedInterface should not be exposed to this application because it is not
// in our CapabilityFilter whitelist.
TEST_F(ConnectTest, PackagedApp_BlockedInterface) {
  std::unique_ptr<Connection> connection = connector()->Connect(kTestAppAName);
  base::RunLoop run_loop;
  test::mojom::BlockedInterfacePtr blocked;
  connection->GetInterface(&blocked);
  blocked.set_connection_error_handler(base::Bind(&QuitLoop, &run_loop));
  run_loop.Run();
}

// Connection to another application provided by the same package, blocked
// because it's not in the capability filter whitelist.
TEST_F(ConnectTest, BlockedPackagedApplication) {
  std::unique_ptr<Connection> connection = connector()->Connect(kTestAppBName);
  test::mojom::ConnectTestServicePtr service_b;
  connection->GetInterface(&service_b);
  base::RunLoop run_loop;
  connection->SetConnectionLostClosure(base::Bind(&QuitLoop, &run_loop));
  run_loop.Run();
  EXPECT_FALSE(connection->IsPending());
  EXPECT_EQ(mojom::ConnectResult::ACCESS_DENIED, connection->GetResult());
}

TEST_F(ConnectTest, CapabilityClasses) {
  std::unique_ptr<Connection> connection = connector()->Connect(kTestAppName);
  test::mojom::StandaloneAppPtr standalone_app;
  connection->GetInterface(&standalone_app);
  std::string string1, string2;
  base::RunLoop loop;
  standalone_app->ConnectToClassInterface(
      base::Bind(&ReceiveTwoStrings, &string1, &string2, &loop));
  loop.Run();
  EXPECT_EQ("PONG", string1);
  EXPECT_EQ("CLASS APP", string2);
}

TEST_F(ConnectTest, ConnectWithoutExplicitClassBlocked) {
  // We not be able to bind a ClassInterfacePtr since the connect_unittest app
  // does not explicitly request the "class" capability from
  // connect_test_class_app. This test will hang if it is bound.
  std::unique_ptr<Connection> connection =
      connector()->Connect(kTestClassAppName);
  test::mojom::ClassInterfacePtr class_interface;
  connection->GetInterface(&class_interface);
  base::RunLoop loop;
  class_interface.set_connection_error_handler(base::Bind(&QuitLoop, &loop));
  loop.Run();
}

TEST_F(ConnectTest, ConnectAsDifferentUser_Allowed) {
  std::unique_ptr<Connection> connection = connector()->Connect(kTestAppName);
  test::mojom::UserIdTestPtr user_id_test;
  connection->GetInterface(&user_id_test);
  mojom::ConnectResult result;
  Identity target(kTestClassAppName, base::GenerateGUID());
  Identity result_identity;
  {
    base::RunLoop loop;
    user_id_test->ConnectToClassAppAsDifferentUser(
        target,
        base::Bind(&ReceiveConnectionResult, &result, &result_identity, &loop));
    loop.Run();
  }
  EXPECT_EQ(result, mojom::ConnectResult::SUCCEEDED);
  EXPECT_EQ(target, result_identity);
}

TEST_F(ConnectTest, ConnectAsDifferentUser_Blocked) {
  std::unique_ptr<Connection> connection = connector()->Connect(kTestAppAName);
  test::mojom::UserIdTestPtr user_id_test;
  connection->GetInterface(&user_id_test);
  mojom::ConnectResult result;
  Identity target(kTestClassAppName, base::GenerateGUID());
  Identity result_identity;
  {
    base::RunLoop loop;
    user_id_test->ConnectToClassAppAsDifferentUser(
        target,
        base::Bind(&ReceiveConnectionResult, &result, &result_identity, &loop));
    loop.Run();
  }
  EXPECT_EQ(mojom::ConnectResult::ACCESS_DENIED, result);
  EXPECT_FALSE(target == result_identity);
}

// There are various other tests (service manager, lifecycle) that test valid
// client
// process specifications. This is the only one for blocking.
TEST_F(ConnectTest, ConnectToClientProcess_Blocked) {
  base::Process process;
  std::unique_ptr<service_manager::Connection> connection =
      service_manager::test::LaunchAndConnectToProcess(
#if defined(OS_WIN)
          "connect_test_exe.exe",
#else
          "connect_test_exe",
#endif
          service_manager::Identity("connect_test_exe",
                                    service_manager::mojom::kInheritUserID),
          connector(), &process);
  EXPECT_EQ(connection->GetResult(), mojom::ConnectResult::ACCESS_DENIED);
}

// Verifies that a client with the "all_users" capability class can receive
// connections from clients run as other users.
TEST_F(ConnectTest, AllUsersSingleton) {
  // Connect to an instance with an explicitly different user_id. This supplied
  // user id should be ignored by the service manager (which will generate its
  // own
  // synthetic user id for all-user singleton instances).
  const std::string singleton_userid = base::GenerateGUID();
  Identity singleton_id(kTestSingletonAppName, singleton_userid);
  std::unique_ptr<Connection> connection = connector()->Connect(singleton_id);
  {
    base::RunLoop loop;
    connection->AddConnectionCompletedClosure(base::Bind(&QuitLoop, &loop));
    loop.Run();
    EXPECT_NE(connection->GetRemoteIdentity().user_id(), singleton_userid);
  }
  // This connects using the current client's user_id. It should be bound to the
  // same service started above, with the same service manager-generated user
  // id.
  std::unique_ptr<Connection> inherit_connection =
      connector()->Connect(kTestSingletonAppName);
  {
    base::RunLoop loop;
    inherit_connection->AddConnectionCompletedClosure(
        base::Bind(&QuitLoop, &loop));
    loop.Run();
    EXPECT_EQ(inherit_connection->GetRemoteIdentity().user_id(),
              connection->GetRemoteIdentity().user_id());
  }
}

}  // namespace service_manager
