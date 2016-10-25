// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/service_test.h"

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "services/service_manager/background/background_service_manager.h"
#include "services/service_manager/public/cpp/service.h"

namespace service_manager {
namespace test {

ServiceTestClient::ServiceTestClient(ServiceTest* test) : test_(test) {}
ServiceTestClient::~ServiceTestClient() {}

void ServiceTestClient::OnStart(const ServiceInfo& info) {
  test_->OnStartCalled(connector(), info.identity.name(),
                       info.identity.user_id());
}

ServiceTest::ServiceTest() {}
ServiceTest::ServiceTest(const std::string& test_name)
    : test_name_(test_name) {}
ServiceTest::~ServiceTest() {}

void ServiceTest::InitTestName(const std::string& test_name) {
  DCHECK(test_name_.empty());
  test_name_ = test_name;
}

std::unique_ptr<Service> ServiceTest::CreateService() {
  return base::WrapUnique(new ServiceTestClient(this));
}

std::unique_ptr<base::MessageLoop> ServiceTest::CreateMessageLoop() {
  return base::WrapUnique(new base::MessageLoop);
}

void ServiceTest::OnStartCalled(Connector* connector,
                                const std::string& name,
                                const std::string& user_id) {
  DCHECK_EQ(connector_, connector);
  initialize_name_ = name;
  initialize_userid_ = user_id;
  initialize_called_.Run();
}

void ServiceTest::SetUp() {
  service_ = CreateService();
  message_loop_ = CreateMessageLoop();
  background_service_manager_.reset(
      new service_manager::BackgroundServiceManager);
  background_service_manager_->Init(nullptr);

  // Create the service manager connection. We don't proceed until we get our
  // Service's OnStart() method is called.
  base::RunLoop run_loop;
  base::MessageLoop::ScopedNestableTaskAllower allow(
      base::MessageLoop::current());
  initialize_called_ = run_loop.QuitClosure();

  service_->set_context(base::MakeUnique<ServiceContext>(
      service_.get(),
      background_service_manager_->CreateServiceRequest(test_name_)));
  connector_ = service_->connector();

  run_loop.Run();
}

void ServiceTest::TearDown() {
  background_service_manager_.reset();
  message_loop_.reset();
  service_.reset();
}

}  // namespace test
}  // namespace service_manager
