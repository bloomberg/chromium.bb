// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_DEVICE_SERVICE_TEST_BASE_H_
#define SERVICES_DEVICE_DEVICE_SERVICE_TEST_BASE_H_

#include "base/macros.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class DeviceService;

const char kTestGeolocationApiKey[] = "FakeApiKeyForTest";

// Base class responsible to setup Device Service for test.
class DeviceServiceTestBase : public testing::Test {
 public:
  DeviceServiceTestBase();
  ~DeviceServiceTestBase() override;

 protected:
  service_manager::Connector* connector() { return connector_.get(); }

  base::test::ScopedTaskEnvironment task_environment_;
  base::Thread file_thread_;
  base::Thread io_thread_;
  network::TestURLLoaderFactory test_url_loader_factory_;

 private:
  service_manager::TestConnectorFactory test_connector_factory_;
  std::unique_ptr<service_manager::Connector> connector_;
  std::unique_ptr<DeviceService> service_;

  DISALLOW_COPY_AND_ASSIGN(DeviceServiceTestBase);
};

}  // namespace device

#endif  // SERVICES_DEVICE_DEVICE_SERVICE_TEST_BASE_H_
