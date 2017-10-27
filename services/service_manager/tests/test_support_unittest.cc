// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context_ref.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "services/service_manager/tests/test.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace service_manager {

namespace {

// A simple test interface on the service, which can be pinged by test code to
// verify a working service connection.
class TestCImpl : public TestC {
 public:
  explicit TestCImpl(std::unique_ptr<ServiceContextRef> service_ref)
      : service_ref_(std::move(service_ref)) {}
  ~TestCImpl() override = default;

 private:
  // TestC:
  void C(CCallback callback) override { std::move(callback).Run(); }

  const std::unique_ptr<service_manager::ServiceContextRef> service_ref_;

  DISALLOW_COPY_AND_ASSIGN(TestCImpl);
};

void OnTestCRequest(service_manager::ServiceContextRefFactory* ref_factory,
                    TestCRequest request) {
  mojo::MakeStrongBinding(std::make_unique<TestCImpl>(ref_factory->CreateRef()),
                          std::move(request));
}

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
  void OnStart() override {
    ref_factory_.reset(
        new ServiceContextRefFactory(base::Bind(&base::DoNothing)));
    registry_.AddInterface(base::Bind(&OnTestCRequest, ref_factory_.get()));
  }

  void OnBindInterface(const BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    registry_.BindInterface(interface_name, std::move(interface_pipe));
    if (on_bind_interface_callback_)
      on_bind_interface_callback_.Run(source_info.identity);
  }

 private:
  // State needed to manage service lifecycle and lifecycle of bound clients.
  std::unique_ptr<service_manager::ServiceContextRefFactory> ref_factory_;
  service_manager::BinderRegistry registry_;

  OnBindInterfaceCallback on_bind_interface_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestServiceImpl);
};

}  // namespace

TEST(ServiceManagerTestSupport, TestConnectorFactory) {
  base::test::ScopedTaskEnvironment task_environment;

  TestConnectorFactory factory(std::make_unique<TestServiceImpl>());
  std::unique_ptr<Connector> connector = factory.CreateConnector();
  TestCPtr c;
  connector->BindInterface(TestC::Name_, &c);
  base::RunLoop loop;
  c->C(loop.QuitClosure());
  loop.Run();
}

TEST(ServiceManagerTestSupport, TestConnectorFactoryOverrideSourceIdentity) {
  base::test::ScopedTaskEnvironment task_environment;

  static const std::string kTestSourceIdentity = "worst. service. ever.";
  // Verify that the test service sees our overridden source identity.
  auto bind_identity = std::make_unique<Identity>();
  auto service = std::make_unique<TestServiceImpl>();
  service->set_on_bind_interface_callback(base::Bind(
      [](std::unique_ptr<Identity>* identity, const Identity& source_identity) {
        *identity = std::make_unique<Identity>(source_identity);
      },
      &bind_identity));

  TestConnectorFactory factory(std::move(service));
  factory.set_source_identity(Identity(kTestSourceIdentity));
  std::unique_ptr<Connector> connector = factory.CreateConnector();

  TestCPtr c;
  connector->BindInterface(TestC::Name_, &c);

  base::RunLoop loop;
  c->C(loop.QuitClosure());
  loop.Run();
  EXPECT_EQ(kTestSourceIdentity, bind_identity->name());
}

}  // namespace service_manager
