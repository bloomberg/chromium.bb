// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_PLATFORM_TEST_HELPER_MUS_H_
#define UI_VIEWS_TEST_PLATFORM_TEST_HELPER_MUS_H_

#include "base/macros.h"
#include "ui/compositor/test/fake_context_factory.h"
#include "ui/views/test/platform_test_helper.h"

namespace views {

class MusClient;

class PlatformTestHelperMus : public PlatformTestHelper {
 public:
  PlatformTestHelperMus();
  ~PlatformTestHelperMus() override;

  // PlatformTestHelper:
  void OnTestHelperCreated(ViewsTestHelper* helper) override;
  void SimulateNativeDestroy(Widget* widget) override;
  void InitializeContextFactory(
      ui::ContextFactory** context_factory,
      ui::ContextFactoryPrivate** context_factory_private) override;

 private:
  class ServiceManagerConnection;

  std::unique_ptr<ServiceManagerConnection> service_manager_connection_;
  std::unique_ptr<MusClient> mus_client_;
  ui::FakeContextFactory context_factory_;

  DISALLOW_COPY_AND_ASSIGN(PlatformTestHelperMus);
};

}  // namespace views

#endif  // UI_VIEWS_TEST_PLATFORM_TEST_HELPER_MUS_H_
