// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_WINDOW_SERVER_SERVICE_TEST_BASE_H_
#define SERVICES_WS_WINDOW_SERVER_SERVICE_TEST_BASE_H_

#include "base/macros.h"
#include "base/test/scoped_task_environment.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/cpp/test/test_service_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ws {

// Base class for all window manager service tests to perform some common setup.
// This fixture brings up a test Service Manager and acts as a service instance
// which identifies as the service named "ui_ws2_service_unittests". Subclasses
// can implement |OnBindInterface()| to handle interface requests targeting that
// instance in tests.
class WindowServerServiceTestBase : public testing::Test,
                                    public service_manager::Service {
 public:
  WindowServerServiceTestBase();
  ~WindowServerServiceTestBase() override;

  service_manager::Connector* connector() {
    return test_service_binding_.GetConnector();
  }

  const char* test_name() const;

 private:
  base::test::ScopedTaskEnvironment task_environment_;
  service_manager::TestServiceManager test_service_manager_;
  service_manager::ServiceBinding test_service_binding_;

  DISALLOW_COPY_AND_ASSIGN(WindowServerServiceTestBase);
};

}  // namespace ws

#endif  // SERVICES_WS_WINDOW_SERVER_SERVICE_TEST_BASE_H_
