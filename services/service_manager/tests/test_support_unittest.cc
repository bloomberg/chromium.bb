// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "services/service_manager/tests/test.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace service_manager {

namespace {

// This is a test service used to demonstrate usage of TestConnectorFactory.
// See documentation on TestConnectorFactory for more details about usage.
class TestServiceImpl : public Service {
 public:
  using OnBindInterfaceCallback = base::Callback<void(const Identity&)>;

  TestServiceImpl() = default;
  ~TestServiceImpl() override = default;

  void set_on_bind_interface_callback(const OnBindInterfaceCallback& callback) {
    on_bind_interface_callback_ = callback;
  }

  // Service:
  void OnBindInterface(const BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    if (interface_name == TestC::Name_)
      c_bindings_.AddBinding(&c_impl_, TestCRequest(std::move(interface_pipe)));

    if (on_bind_interface_callback_)
      on_bind_interface_callback_.Run(source_info.identity);
  }

 private:
  // A simple test interface on the service, which can be pinged by test code
  // to verify a working service connection.
  class TestCImpl : public TestC {
   public:
    TestCImpl() = default;
    ~TestCImpl() override = default;

    // TestC:
    void C(CCallback callback) override { std::move(callback).Run(); }

   private:
    DISALLOW_COPY_AND_ASSIGN(TestCImpl);
  };

  TestCImpl c_impl_;
  mojo::BindingSet<TestC> c_bindings_;
  OnBindInterfaceCallback on_bind_interface_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestServiceImpl);
};

}  // namespace

TEST(ServiceManagerTestSupport, TestConnectorFactory) {
  base::test::ScopedTaskEnvironment task_environment;

  TestServiceImpl service;
  TestConnectorFactory factory(&service);
  std::unique_ptr<Connector> connector = factory.CreateConnector();

  TestCPtr c;
  connector->BindInterface("ignored", &c);

  base::RunLoop loop;
  c->C(loop.QuitClosure());
  loop.Run();
}

TEST(ServiceManagerTestSupport, TestConnectorFactoryOverrideSourceIdentity) {
  base::test::ScopedTaskEnvironment task_environment;

  TestServiceImpl service;

  static const std::string kTestSourceIdentity = "worst. service. ever.";
  TestConnectorFactory factory(&service);
  factory.set_source_identity(Identity(kTestSourceIdentity));
  std::unique_ptr<Connector> connector = factory.CreateConnector();

  // Verify that the test service sees our overridden source identity.
  service.set_on_bind_interface_callback(
      base::Bind([](const Identity& source_identity) {
        EXPECT_EQ(kTestSourceIdentity, source_identity.name());
      }));

  TestCPtr c;
  connector->BindInterface("ignored", &c);

  base::RunLoop loop;
  c->C(loop.QuitClosure());
  loop.Run();
}

}  // namespace service_manager
