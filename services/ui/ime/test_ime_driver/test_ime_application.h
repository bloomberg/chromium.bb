// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_IME_TEST_IME_DRIVER_TEST_IME_APPLICATION_H_
#define SERVICES_UI_IME_TEST_IME_DRIVER_TEST_IME_APPLICATION_H_

#include "services/shell/public/cpp/service.h"

namespace ui {
namespace test {

class TestIMEApplication : public shell::Service {
 public:
  TestIMEApplication();
  ~TestIMEApplication() override;

 private:
  // shell::Service:
  bool OnConnect(const shell::Identity& remote_identity,
                 shell::InterfaceRegistry* registry) override;
  void OnStart(const shell::Identity& identity) override;

  DISALLOW_COPY_AND_ASSIGN(TestIMEApplication);
};

}  // namespace test
}  // namespace ui

#endif  // SERVICES_UI_IME_TEST_IME_DRIVER_TEST_IME_APPLICATION_H_
