// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/caps_lock_notification_controller.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/test_accessibility_controller_client.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"

namespace ash {

using CapsLockNotificationControllerTest = AshTestBase;

// Tests that a11y alert is sent on toggling caps lock.
TEST_F(CapsLockNotificationControllerTest, A11yAlert) {
  auto caps_lock = std::make_unique<CapsLockNotificationController>();

  TestAccessibilityControllerClient client;
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  controller->SetClient(client.CreateInterfacePtrAndBind());

  // Simulate turning on caps lock.
  caps_lock->OnCapsLockChanged(true);
  controller->FlushMojoForTest();
  EXPECT_EQ(mojom::AccessibilityAlert::CAPS_ON, client.last_a11y_alert());

  // Simulate turning off caps lock.
  caps_lock->OnCapsLockChanged(false);
  controller->FlushMojoForTest();
  EXPECT_EQ(mojom::AccessibilityAlert::CAPS_OFF, client.last_a11y_alert());
}

}  // namespace ash
