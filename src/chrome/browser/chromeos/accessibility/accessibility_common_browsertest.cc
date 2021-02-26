// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_pref_names.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/notification_types.h"

namespace chromeos {

class AccessibilityCommonTest : public InProcessBrowserTest {
 public:
  bool DoesComponentExtensionExist(const std::string& id) {
    return extensions::ExtensionSystem::Get(
               AccessibilityManager::Get()->profile())
        ->extension_service()
        ->component_loader()
        ->Exists(id);
  }

 protected:
  AccessibilityCommonTest() = default;
};

IN_PROC_BROWSER_TEST_F(AccessibilityCommonTest, ToggleFeatures) {
  AccessibilityManager* manager = AccessibilityManager::Get();
  const auto& enabled_features =
      manager->GetAccessibilityCommonEnabledFeaturesForTest();

  EXPECT_TRUE(enabled_features.empty());
  EXPECT_FALSE(DoesComponentExtensionExist(
      extension_misc::kAccessibilityCommonExtensionId));

  PrefService* pref_service = manager->profile()->GetPrefs();

  content::WindowedNotificationObserver
      accessibility_common_extension_load_waiter(
          extensions::NOTIFICATION_EXTENSION_HOST_DID_STOP_FIRST_LOAD,
          content::NotificationService::AllSources());
  pref_service->SetBoolean(ash::prefs::kAccessibilityAutoclickEnabled, true);
  accessibility_common_extension_load_waiter.Wait();

  EXPECT_EQ(1U, enabled_features.size());
  EXPECT_EQ(1U,
            enabled_features.count(ash::prefs::kAccessibilityAutoclickEnabled));
  EXPECT_TRUE(DoesComponentExtensionExist(
      extension_misc::kAccessibilityCommonExtensionId));

  pref_service->SetBoolean(ash::prefs::kAccessibilityScreenMagnifierEnabled,
                           true);
  EXPECT_EQ(2U, enabled_features.size());
  EXPECT_EQ(1U,
            enabled_features.count(ash::prefs::kAccessibilityAutoclickEnabled));
  EXPECT_EQ(1U, enabled_features.count(
                    ash::prefs::kAccessibilityScreenMagnifierEnabled));
  EXPECT_TRUE(DoesComponentExtensionExist(
      extension_misc::kAccessibilityCommonExtensionId));

  pref_service->SetBoolean(ash::prefs::kAccessibilityAutoclickEnabled, false);
  EXPECT_EQ(1U, enabled_features.size());
  EXPECT_EQ(1U, enabled_features.count(
                    ash::prefs::kAccessibilityScreenMagnifierEnabled));
  EXPECT_TRUE(DoesComponentExtensionExist(
      extension_misc::kAccessibilityCommonExtensionId));

  pref_service->SetBoolean(ash::prefs::kAccessibilityScreenMagnifierEnabled,
                           false);
  EXPECT_TRUE(enabled_features.empty());
  EXPECT_FALSE(DoesComponentExtensionExist(
      extension_misc::kAccessibilityCommonExtensionId));

  // Not an accessibility common feature.
  content::WindowedNotificationObserver spoken_feedback_extension_load_waiter(
      extensions::NOTIFICATION_EXTENSION_HOST_DID_STOP_FIRST_LOAD,
      content::NotificationService::AllSources());
  pref_service->SetBoolean(ash::prefs::kAccessibilitySpokenFeedbackEnabled,
                           true);
  spoken_feedback_extension_load_waiter.Wait();
  EXPECT_TRUE(enabled_features.empty());
  EXPECT_FALSE(DoesComponentExtensionExist(
      extension_misc::kAccessibilityCommonExtensionId));
  EXPECT_TRUE(
      DoesComponentExtensionExist(extension_misc::kChromeVoxExtensionId));
}

}  // namespace chromeos
