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
#include "base/optional.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_suite.h"
#include "base/token.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/background/background_service_manager.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/constants.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/mojom/service_manager.mojom.h"
#include "services/service_manager/tests/connect/connect_test.mojom.h"
#include "services/service_manager/tests/util.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests that multiple services can be packaged in a single service by
// implementing ServiceFactory; that these services can be specified by
// the package's manifest and are thus registered with the PackageManager.

namespace service_manager {

namespace {

const char kTestServiceName[] = "connect_unittests";
const char kTestPackageName[] = "connect_test_package";
const char kTestAppName[] = "connect_test_app";
const char kTestAppAName[] = "connect_test_a";
const char kTestAppBName[] = "connect_test_b";
const char kTestNonexistentAppName[] = "connect_test_nonexistent_app";
const char kTestSandboxedAppName[] = "connect_test_sandboxed_app";
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

void ReceiveQueryResult(mojom::ServiceInfoPtr* out_info,
                        base::RunLoop* loop,
                        mojom::ServiceInfoPtr info) {
  *out_info = std::move(info);
  loop->Quit();
}

void ReceiveConnectionResult(mojom::ConnectResult* out_result,
                             base::Optional<Identity>* out_target,
                             base::RunLoop* loop,
                             int32_t in_result,
                             const base::Optional<Identity>& in_identity) {
  *out_result = static_cast<mojom::ConnectResult>(in_result);
  *out_target = in_identity;
  loop->Quit();
}

void StartServiceResponse(base::RunLoop* quit_loop,
                          mojom::ConnectResult* out_result,
                          base::Optional<Identity>* out_resolved_identity,
                          mojom::ConnectResult result,
                          const base::Optional<Identity>& resolved_identity) {
  if (quit_loop)
    quit_loop->Quit();
  if (out_result)
    *out_result = result;
  if (out_resolved_identity)
    *out_resolved_identity = resolved_identity;
}

void QuitLoop(base::RunLoop* loop) {
  loop->Quit();
}

class TestTargetService : public Service {
 public:
  explicit TestTargetService(mojom::ServiceRequest request)
      : binding_(this, std::move(request)) {}
  ~TestTargetService() override = default;

  const Identity& identity() const { return binding_.identity(); }
  Connector* connector() { return binding_.GetConnector(); }

  void CallOnNextBindInterface(base::OnceClosure callback) {
    next_bind_interface_callback_ = std::move(callback);
  }

  void WaitForStart() { wait_for_start_loop_.Run(); }

  void WaitForBindInterface() {
    wait_for_bind_interface_loop_.emplace();
    wait_for_bind_interface_loop_->Run();
  }

  void QuitGracefullyAndWait() {
    binding_.RequestClose();
    wait_for_disconnect_loop_.Run();
  }

 private:
  // Service:
  void OnStart() override { wait_for_start_loop_.Quit(); }
  void OnBindInterface(const BindSourceInfo& source,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    if (next_bind_interface_callback_)
      std::move(next_bind_interface_callback_).Run();
    if (wait_for_bind_interface_loop_)
      wait_for_bind_interface_loop_->Quit();
  }
  void OnDisconnected() override { wait_for_disconnect_loop_.Quit(); }

  ServiceBinding binding_;
  base::RunLoop wait_for_start_loop_;
  base::RunLoop wait_for_disconnect_loop_;
  base::Optional<base::RunLoop> wait_for_bind_interface_loop_;
  base::OnceClosure next_bind_interface_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestTargetService);
};

class ConnectTest : public testing::Test,
                    public Service,
                    public test::mojom::ExposedInterface {
 public:
  ConnectTest() : service_manager_(nullptr, nullptr) {}
  ~ConnectTest() override = default;

  Connector* connector() { return service_binding_.GetConnector(); }

 protected:
  void CompareConnectionState(
      const std::string& connection_local_name,
      const std::string& connection_remote_name,
      const base::Optional<base::Token>& connection_remote_instance_group,
      const std::string& initialize_local_name,
      const base::Optional<base::Token>& initialize_local_instance_group) {
    EXPECT_EQ(connection_remote_name,
              connection_state_->connection_remote_name);
    EXPECT_EQ(connection_remote_instance_group,
              connection_state_->connection_remote_instance_group);
    EXPECT_EQ(initialize_local_name, connection_state_->initialize_local_name);
    EXPECT_EQ(initialize_local_instance_group,
              connection_state_->initialize_local_instance_group);
  }

  mojom::ServiceRequest RegisterServiceInstance(
      const std::string& service_name) {
    mojom::ServicePtr proxy;
    mojom::ServiceRequest request = mojo::MakeRequest(&proxy);
    mojom::PIDReceiverPtr pid_receiver;
    service_manager_.RegisterService(
        Identity(service_name, kSystemInstanceGroup, base::Token{},
                 base::Token::CreateRandom()),
        std::move(proxy), mojo::MakeRequest(&pid_receiver));
    pid_receiver->SetPID(1);
    return request;
  }

 private:
  // testing::Test:
  void SetUp() override {
    service_binding_.Bind(RegisterServiceInstance(kTestServiceName));

    test::mojom::ConnectTestServicePtr root_service;
    connector()->BindInterface(kTestPackageName, &root_service);

    base::RunLoop run_loop;
    std::string root_name;
    root_service->GetTitle(
        base::Bind(&ReceiveOneString, &root_name, &run_loop));
    run_loop.Run();
  }

  // Service:
  void OnBindInterface(const BindSourceInfo& source,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    CHECK_EQ(test::mojom::ExposedInterface::Name_, interface_name);
    bindings_.AddBinding(
        this, test::mojom::ExposedInterfaceRequest(std::move(interface_pipe)));
  }

  // test::mojom::ExposedInterface:
  void ConnectionAccepted(test::mojom::ConnectionStatePtr state) override {
    connection_state_ = std::move(state);
  }

  base::test::ScopedTaskEnvironment task_environment_;
  BackgroundServiceManager service_manager_;
  ServiceBinding service_binding_{this};
  mojo::BindingSet<test::mojom::ExposedInterface> bindings_;
  test::mojom::ConnectionStatePtr connection_state_;

  DISALLOW_COPY_AND_ASSIGN(ConnectTest);
};

// Ensure the connection was properly established and that a round trip
// method call/response is completed.
TEST_F(ConnectTest, BindInterface) {
  test::mojom::ConnectTestServicePtr service;
  connector()->BindInterface(kTestAppName, &service);
  base::RunLoop run_loop;
  std::string title;
  service->GetTitle(base::Bind(&ReceiveOneString, &title, &run_loop));
  run_loop.Run();
  EXPECT_EQ("APP", title);
}

TEST_F(ConnectTest, Instances) {
  const base::Token kInstanceIdA{1, 2};
  const base::Token kInstanceIdB{3, 4};
  auto filter_a = ServiceFilter::ByNameWithId(kTestAppName, kInstanceIdA);
  base::Token instance_a1, instance_a2;
  test::mojom::ConnectTestServicePtr service_a1;
  {
    connector()->BindInterface(filter_a, &service_a1);
    base::RunLoop loop;
    service_a1->GetInstanceId(
        base::BindLambdaForTesting([&](const base::Token& instance_id) {
          instance_a1 = instance_id;
          loop.Quit();
        }));
    loop.Run();
  }
  test::mojom::ConnectTestServicePtr service_a2;
  {
    connector()->BindInterface(filter_a, &service_a2);
    base::RunLoop loop;
    service_a2->GetInstanceId(
        base::BindLambdaForTesting([&](const base::Token& instance_id) {
          instance_a2 = instance_id;
          loop.Quit();
        }));
    loop.Run();
  }
  EXPECT_EQ(instance_a1, instance_a2);

  auto filter_b = ServiceFilter::ByNameWithId(kTestAppName, kInstanceIdB);
  base::Token instance_b;
  test::mojom::ConnectTestServicePtr service_b;
  {
    connector()->BindInterface(filter_b, &service_b);
    base::RunLoop loop;
    service_b->GetInstanceId(
        base::BindLambdaForTesting([&](const base::Token& instance_id) {
          instance_b = instance_id;
          loop.Quit();
        }));
    loop.Run();
  }

  EXPECT_NE(instance_a1, instance_b);
}

TEST_F(ConnectTest, ConnectWithGloballyUniqueId) {
  base::Optional<TestTargetService> target(
      base::in_place, RegisterServiceInstance(kTestAppAName));
  target->WaitForStart();

  Identity specific_identity = target->identity();
  EXPECT_TRUE(specific_identity.IsValid());

  // First connect with a basic identity.
  test::mojom::ConnectTestServicePtr proxy;
  connector()->BindInterface(kTestAppAName, &proxy);
  target->WaitForBindInterface();

  // Now connect with a very specific identity, including globally unique ID.
  connector()->BindInterface(specific_identity, &proxy);
  target->WaitForBindInterface();

  // Now quit the test service and start a new instance.
  target->QuitGracefullyAndWait();

  target.emplace(RegisterServiceInstance(kTestAppAName));
  target->WaitForStart();

  Identity new_specific_identity = target->identity();

  // This must differ from the old identity because all instances should have
  // a globally unique ID.
  EXPECT_NE(specific_identity, new_specific_identity);

  // Connect to the new instance with a basic identity, and with its specific
  // identity. Both should succeed.
  connector()->BindInterface(kTestAppAName, &proxy);
  target->WaitForBindInterface();
  connector()->BindInterface(new_specific_identity, &proxy);
  target->WaitForBindInterface();

  // Now attempt to connect using the specific identity of the previous
  // instance. This request should not be seen by the new instance, and |proxy|
  // should be disconnected when the Service Manager drops the request.
  base::RunLoop wait_for_error_loop;
  target->CallOnNextBindInterface(base::BindOnce([] { NOTREACHED(); }));
  connector()->BindInterface(specific_identity, &proxy);
  proxy.set_connection_error_handler(wait_for_error_loop.QuitClosure());
  wait_for_error_loop.Run();
}

TEST_F(ConnectTest, QueryService) {
  mojom::ServiceInfoPtr service_info;
  base::RunLoop run_loop;
  connector()->QueryService(
      kTestSandboxedAppName,
      base::BindOnce(&ReceiveQueryResult, &service_info, &run_loop));
  run_loop.Run();
  ASSERT_TRUE(service_info);
  EXPECT_EQ("superduper", service_info->sandbox_type);
}

TEST_F(ConnectTest, QueryNonexistentService) {
  mojom::ServiceInfoPtr service_info;
  base::RunLoop run_loop;
  connector()->QueryService(
      kTestNonexistentAppName,
      base::BindOnce(&ReceiveQueryResult, &service_info, &run_loop));
  run_loop.Run();
  EXPECT_FALSE(service_info);
}

#if DCHECK_IS_ON()
// This test triggers intentional DCHECKs but is not suitable for death testing.
#define MAYBE_BlockedInterface DISABLED_BlockedInterface
#else
#define MAYBE_BlockedInterface BlockedInterface
#endif

// BlockedInterface should not be exposed to this application because it is not
// in our CapabilityFilter whitelist.
TEST_F(ConnectTest, MAYBE_BlockedInterface) {
  base::RunLoop run_loop;
  test::mojom::BlockedInterfacePtr blocked;
  connector()->BindInterface(kTestAppName, &blocked);
  blocked.set_connection_error_handler(base::Bind(&QuitLoop, &run_loop));
  std::string title = "unchanged";
  blocked->GetTitleBlocked(base::Bind(&ReceiveOneString, &title, &run_loop));
  run_loop.Run();
  EXPECT_EQ("unchanged", title);
}

// Connects to an app provided by a package.
TEST_F(ConnectTest, PackagedApp) {
  base::Optional<Identity> resolved_identity;
  base::RunLoop run_loop;
  test::mojom::ConnectTestServicePtr service_a;
  connector()->BindInterface(ServiceFilter::ByName(kTestAppAName),
                             mojo::MakeRequest(&service_a),
                             base::BindOnce(&StartServiceResponse, nullptr,
                                            nullptr, &resolved_identity));
  std::string a_name;
  service_a->GetTitle(base::Bind(&ReceiveOneString, &a_name, &run_loop));
  run_loop.Run();
  EXPECT_EQ("A", a_name);
  ASSERT_TRUE(resolved_identity);
  EXPECT_EQ(resolved_identity->name(), kTestAppAName);
}

#if DCHECK_IS_ON()
// This test triggers intentional DCHECKs but is not suitable for death testing.
#define MAYBE_BlockedPackage DISABLED_BlockedPackage
#else
#define MAYBE_BlockedPackage BlockedPackage
#endif

// Ask the target application to attempt to connect to a third application
// provided by a package whose id is permitted by the primary target's
// CapabilityFilter but whose package is not. The connection should be
// allowed regardless of the target's CapabilityFilter with respect to the
// package.
TEST_F(ConnectTest, MAYBE_BlockedPackage) {
  test::mojom::StandaloneAppPtr standalone_app;
  connector()->BindInterface(kTestAppName, &standalone_app);
  base::RunLoop run_loop;
  std::string title;
  standalone_app->ConnectToAllowedAppInBlockedPackage(
      base::Bind(&ReceiveOneString, &title, &run_loop));
  run_loop.Run();
  EXPECT_EQ("A", title);
}

#if DCHECK_IS_ON()
// This test triggers intentional DCHECKs but is not suitable for death testing.
#define MAYBE_PackagedApp_BlockedInterface DISABLED_PackagedApp_BlockedInterface
#else
#define MAYBE_PackagedApp_BlockedInterface PackagedApp_BlockedInterface
#endif

// BlockedInterface should not be exposed to this application because it is not
// in our CapabilityFilter whitelist.
TEST_F(ConnectTest, MAYBE_PackagedApp_BlockedInterface) {
  base::RunLoop run_loop;
  test::mojom::BlockedInterfacePtr blocked;
  connector()->BindInterface(kTestAppAName, &blocked);
  blocked.set_connection_error_handler(base::Bind(&QuitLoop, &run_loop));
  run_loop.Run();
}

#if DCHECK_IS_ON()
// This test triggers intentional DCHECKs but is not suitable for death testing.
#define MAYBE_BlockedPackagedApplication DISABLED_BlockedPackagedApplication
#else
#define MAYBE_BlockedPackagedApplication BlockedPackagedApplication
#endif

// Connection to another application provided by the same package, blocked
// because it's not in the capability filter whitelist.
TEST_F(ConnectTest, MAYBE_BlockedPackagedApplication) {
  mojom::ConnectResult result;
  base::RunLoop run_loop;
  test::mojom::ConnectTestServicePtr service_b;
  connector()->BindInterface(
      ServiceFilter::ByName(kTestAppBName), mojo::MakeRequest(&service_b),
      base::BindOnce(&StartServiceResponse, nullptr, &result, nullptr));
  service_b.set_connection_error_handler(run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(mojom::ConnectResult::ACCESS_DENIED, result);
}

TEST_F(ConnectTest, CapabilityClasses) {
  test::mojom::StandaloneAppPtr standalone_app;
  connector()->BindInterface(kTestAppName, &standalone_app);
  std::string string1, string2;
  base::RunLoop loop;
  standalone_app->ConnectToClassInterface(
      base::Bind(&ReceiveTwoStrings, &string1, &string2, &loop));
  loop.Run();
  EXPECT_EQ("PONG", string1);
  EXPECT_EQ("CLASS APP", string2);
}

#if DCHECK_IS_ON()
// This test triggers intentional DCHECKs but is not suitable for death testing.
#define MAYBE_ConnectWithoutExplicitClassBlocked \
  DISABLED_ConnectWithoutExplicitClassBlocked
#else
#define MAYBE_ConnectWithoutExplicitClassBlocked \
  ConnectWithoutExplicitClassBlocked
#endif

TEST_F(ConnectTest, MAYBE_ConnectWithoutExplicitClassBlocked) {
  // We not be able to bind a ClassInterfacePtr since the connect_unittest app
  // does not explicitly request the "class" capability from
  // connect_test_class_app. This test will hang if it is bound.
  test::mojom::ClassInterfacePtr class_interface;
  connector()->BindInterface(kTestClassAppName, &class_interface);
  base::RunLoop loop;
  class_interface.set_connection_error_handler(base::Bind(&QuitLoop, &loop));
  loop.Run();
}

TEST_F(ConnectTest, ConnectToDifferentGroup_Allowed) {
  test::mojom::IdentityTestPtr identity_test;
  connector()->BindInterface(ServiceFilter::ByName(kTestAppName),
                             &identity_test);
  mojom::ConnectResult result;
  auto filter = ServiceFilter::ByNameInGroup(kTestClassAppName,
                                             base::Token::CreateRandom());
  base::Optional<Identity> result_identity;
  {
    base::RunLoop loop;
    identity_test->ConnectToClassAppWithFilter(
        filter,
        base::Bind(&ReceiveConnectionResult, &result, &result_identity, &loop));
    loop.Run();
  }
  EXPECT_EQ(result, mojom::ConnectResult::SUCCEEDED);
  ASSERT_TRUE(result_identity);
  EXPECT_EQ(filter.service_name(), result_identity->name());
  EXPECT_EQ(*filter.instance_group(), result_identity->instance_group());
  EXPECT_TRUE(result_identity->instance_id().is_zero());
  EXPECT_FALSE(result_identity->globally_unique_id().is_zero());
}

TEST_F(ConnectTest, ConnectToDifferentGroup_Blocked) {
  test::mojom::IdentityTestPtr identity_test;
  connector()->BindInterface(ServiceFilter::ByName(kTestAppAName),
                             &identity_test);
  mojom::ConnectResult result;
  auto filter = ServiceFilter::ByNameInGroup(kTestClassAppName,
                                             base::Token::CreateRandom());
  base::Optional<Identity> result_identity;
  {
    base::RunLoop loop;
    identity_test->ConnectToClassAppWithFilter(
        filter,
        base::Bind(&ReceiveConnectionResult, &result, &result_identity, &loop));
    loop.Run();
  }
  EXPECT_EQ(mojom::ConnectResult::ACCESS_DENIED, result);
  EXPECT_FALSE(result_identity);
}

TEST_F(ConnectTest, ConnectWithDifferentInstanceId_Blocked) {
  test::mojom::IdentityTestPtr identity_test;
  connector()->BindInterface(ServiceFilter::ByName(kTestAppAName),
                             &identity_test);

  mojom::ConnectResult result;
  auto filter = ServiceFilter::ByNameWithId(kTestClassAppName,
                                            base::Token::CreateRandom());
  base::Optional<Identity> result_identity;
  base::RunLoop loop;
  identity_test->ConnectToClassAppWithFilter(
      filter, base::BindRepeating(&ReceiveConnectionResult, &result,
                                  &result_identity, &loop));
  loop.Run();
  EXPECT_EQ(mojom::ConnectResult::ACCESS_DENIED, result);
  EXPECT_FALSE(result_identity);
}

// There are various other tests (service manager, lifecycle) that test valid
// client process specifications. This is the only one for blocking.
TEST_F(ConnectTest, ConnectToClientProcess_Blocked) {
  base::Process process;
  mojom::ConnectResult result =
      service_manager::test::LaunchAndConnectToProcess(
#if defined(OS_WIN)
          "connect_test_exe.exe",
#else
          "connect_test_exe",
#endif
          service_manager::Identity("connect_test_exe", kSystemInstanceGroup,
                                    base::Token{}, base::Token::CreateRandom()),
          connector(), &process);
  EXPECT_EQ(result, mojom::ConnectResult::ACCESS_DENIED);
}

// Verifies that a client with the "shared_instance_across_users" value of
// "instance_sharing" option can receive connections from clients run as other
// users.
TEST_F(ConnectTest, AllUsersSingleton) {
  base::Optional<Identity> first_resolved_identity;
  {
    base::RunLoop loop;
    const base::Token singleton_instance_group = base::Token::CreateRandom();
    // Connect to an instance with an explicit different instance group. This
    // supplied group should be ignored by the Service Manager because the
    // target service is a singleton, and the Service Manager always generates a
    // random instance group to host singleton service instances.
    connector()->WarmService(
        service_manager::ServiceFilter::ByNameInGroup(kTestSingletonAppName,
                                                      singleton_instance_group),
        base::BindOnce(&StartServiceResponse, &loop, nullptr,
                       &first_resolved_identity));
    loop.Run();
    ASSERT_TRUE(first_resolved_identity);
    EXPECT_NE(first_resolved_identity->instance_group(),
              singleton_instance_group);
  }
  {
    base::RunLoop loop;
    base::Optional<Identity> resolved_identity;
    // This connects using the current client's instance group. It should be
    // get routed to the same service instance started above.
    connector()->WarmService(
        service_manager::ServiceFilter::ByName(kTestSingletonAppName),
        base::BindOnce(&StartServiceResponse, &loop, nullptr,
                       &resolved_identity));
    loop.Run();
    ASSERT_TRUE(resolved_identity);
    EXPECT_EQ(*resolved_identity, *first_resolved_identity);
  }
}

}  // namespace
}  // namespace service_manager
