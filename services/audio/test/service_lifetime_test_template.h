// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_TEST_SERVICE_LIFETIME_TEST_TEMPLATE_H_
#define SERVICES_AUDIO_TEST_SERVICE_LIFETIME_TEST_TEMPLATE_H_

#include "base/run_loop.h"
#include "services/audio/public/mojom/constants.mojom.h"
#include "services/audio/public/mojom/system_info.mojom.h"
#include "services/audio/test/service_observer_mock.h"
#include "services/service_manager/public/cpp/service_context.h"

namespace audio {

// Template for Audio service lifetime tests regarding client
// connect/disconnect events. Audio service under test must be configured to
// quit after a timeout if there are no incoming connections.
template <class TestBase>
class ServiceLifetimeTestTemplate : public TestBase {
 public:
  ServiceLifetimeTestTemplate() {}

  ~ServiceLifetimeTestTemplate() override {}

 protected:
  void SetUp() override {
    TestBase::SetUp();

    service_manager::mojom::ServiceManagerPtr service_manager;
    TestBase::connector()->BindInterface(service_manager::mojom::kServiceName,
                                         &service_manager);

    service_manager::mojom::ServiceManagerListenerPtr listener;
    service_observer_ = std::make_unique<ServiceObserverMock>(
        mojom::kServiceName, mojo::MakeRequest(&listener));

    base::RunLoop wait_loop;
    EXPECT_CALL(*service_observer_, Initialized())
        .WillOnce(testing::Invoke(&wait_loop, &base::RunLoop::Quit));
    service_manager->AddListener(std::move(listener));
    wait_loop.Run();
  }

 protected:
  std::unique_ptr<ServiceObserverMock> service_observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceLifetimeTestTemplate);
};

TYPED_TEST_CASE_P(ServiceLifetimeTestTemplate);

TYPED_TEST_P(ServiceLifetimeTestTemplate, ServiceQuitsWhenClientDisconnects) {
  mojom::SystemInfoPtr info;
  {
    base::RunLoop wait_loop;
    EXPECT_CALL(*this->service_observer_, ServiceStarted())
        .WillOnce(testing::Invoke(&wait_loop, &base::RunLoop::Quit));
    this->connector()->BindInterface(mojom::kServiceName, &info);
    wait_loop.Run();
  }
  {
    base::RunLoop wait_loop;
    EXPECT_CALL(*this->service_observer_, ServiceStopped())
        .WillOnce(testing::Invoke(&wait_loop, &base::RunLoop::Quit));
    info.reset();
    wait_loop.Run();
  }
}

TYPED_TEST_P(ServiceLifetimeTestTemplate,
             ServiceQuitsWhenLastClientDisconnects) {
  mojom::SystemInfoPtr info;
  {
    base::RunLoop wait_loop;
    EXPECT_CALL(*this->service_observer_, ServiceStarted())
        .WillOnce(testing::Invoke(&wait_loop, &base::RunLoop::Quit));
    this->connector()->BindInterface(mojom::kServiceName, &info);
    wait_loop.Run();
  }
  {
    base::RunLoop wait_loop;
    EXPECT_CALL(*this->service_observer_, ServiceStopped())
        .WillOnce(testing::Invoke(&wait_loop, &base::RunLoop::Quit));
    EXPECT_CALL(*this->service_observer_, ServiceStarted())
        .Times(testing::Exactly(0));

    mojom::SystemInfoPtr info2;
    this->connector()->BindInterface(mojom::kServiceName, &info2);
    info2.FlushForTesting();

    mojom::SystemInfoPtr info3;
    this->connector()->BindInterface(mojom::kServiceName, &info3);
    info3.FlushForTesting();

    info.reset();
    info2.reset();
    info3.reset();
    wait_loop.Run();
  }
}

TYPED_TEST_P(ServiceLifetimeTestTemplate, ServiceRestartsWhenClientReconnects) {
  mojom::SystemInfoPtr info;
  {
    base::RunLoop wait_loop;
    EXPECT_CALL(*this->service_observer_, ServiceStarted())
        .WillOnce(testing::Invoke(&wait_loop, &base::RunLoop::Quit));
    this->connector()->BindInterface(mojom::kServiceName, &info);
    wait_loop.Run();
  }
  {
    base::RunLoop wait_loop;
    EXPECT_CALL(*this->service_observer_, ServiceStopped())
        .WillOnce(testing::Invoke(&wait_loop, &base::RunLoop::Quit));
    info.reset();
    wait_loop.Run();
  }
  {
    base::RunLoop wait_loop;
    EXPECT_CALL(*this->service_observer_, ServiceStarted())
        .WillOnce(testing::Invoke(&wait_loop, &base::RunLoop::Quit));
    this->connector()->BindInterface(mojom::kServiceName, &info);
    wait_loop.Run();
  }
  {
    base::RunLoop wait_loop;
    EXPECT_CALL(*this->service_observer_, ServiceStopped())
        .WillOnce(testing::Invoke(&wait_loop, &base::RunLoop::Quit));
    info.reset();
    wait_loop.Run();
  }
}

REGISTER_TYPED_TEST_CASE_P(ServiceLifetimeTestTemplate,
                           ServiceQuitsWhenClientDisconnects,
                           ServiceQuitsWhenLastClientDisconnects,
                           ServiceRestartsWhenClientReconnects);
}  // namespace audio

#endif  // SERVICES_AUDIO_TEST_SERVICE_LIFETIME_TEST_TEMPLATE_H_
