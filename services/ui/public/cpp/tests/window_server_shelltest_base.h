// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_PUBLIC_CPP_TESTS_WINDOW_SERVER_SHELLTEST_BASE_H_
#define SERVICES_UI_PUBLIC_CPP_TESTS_WINDOW_SERVER_SHELLTEST_BASE_H_

#include "base/macros.h"
#include "services/shell/public/cpp/connection.h"
#include "services/shell/public/cpp/service_test.h"

namespace ui {

// Base class for all window manager ServiceTests to perform some common setup.
class WindowServerServiceTestBase : public shell::test::ServiceTest {
 public:
  WindowServerServiceTestBase();
  ~WindowServerServiceTestBase() override;

  virtual bool OnConnect(const shell::Identity& remote_identity,
                         shell::InterfaceRegistry* registry) = 0;

 private:
  // shell::test::ServiceTest:
  std::unique_ptr<shell::Service> CreateService() override;

  DISALLOW_COPY_AND_ASSIGN(WindowServerServiceTestBase);
};

}  // namespace ui

#endif  // SERVICES_UI_PUBLIC_CPP_TESTS_WINDOW_SERVER_SHELLTEST_BASE_H_
