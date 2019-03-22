// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/ash_test_environment_chrome.h"

#include "ash/test/ash_test_views_delegate.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "ui/views/views_delegate.h"

AshTestEnvironmentChrome::AshTestEnvironmentChrome() {}

AshTestEnvironmentChrome::~AshTestEnvironmentChrome() {}

std::unique_ptr<ash::AshTestViewsDelegate>
AshTestEnvironmentChrome::CreateViewsDelegate() {
  return std::make_unique<ash::AshTestViewsDelegate>();
}

void AshTestEnvironmentChrome::TearDown() {
  // If initialized, the KioskAppManager will register an observer to
  // CrosSettings and will need to be destroyed before it. Having it destroyed
  // as part of the teardown will avoid unexpected test failures.
  chromeos::KioskAppManager::Shutdown();

  ash::AshTestEnvironment::TearDown();
}
