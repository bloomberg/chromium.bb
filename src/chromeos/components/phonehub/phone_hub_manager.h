// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_PHONE_HUB_MANAGER_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_PHONE_HUB_MANAGER_H_

#include <stdint.h>

namespace chromeos {
namespace phonehub {

class ConnectionScheduler;
class DoNotDisturbController;
class FeatureStatusProvider;
class FindMyDeviceController;
class NotificationAccessManager;
class NotificationManager;
class NotificationInteractionHandler;
class OnboardingUiTracker;
class PhoneModel;
class TetherController;
class UserActionRecorder;
class BrowserTabsModelProvider;

// Responsible for the core logic of the Phone Hub feature and exposes
// interfaces via its public API. This class is intended to be a singleton.
class PhoneHubManager {
 public:
  virtual ~PhoneHubManager() = default;

  PhoneHubManager(const PhoneHubManager&) = delete;
  PhoneHubManager& operator=(const PhoneHubManager&) = delete;

  // Getters for sub-elements.
  virtual BrowserTabsModelProvider* GetBrowserTabsModelProvider() = 0;
  virtual ConnectionScheduler* GetConnectionScheduler() = 0;
  virtual DoNotDisturbController* GetDoNotDisturbController() = 0;
  virtual FeatureStatusProvider* GetFeatureStatusProvider() = 0;
  virtual FindMyDeviceController* GetFindMyDeviceController() = 0;
  virtual NotificationAccessManager* GetNotificationAccessManager() = 0;
  virtual NotificationInteractionHandler*
  GetNotificationInteractionHandler() = 0;
  virtual NotificationManager* GetNotificationManager() = 0;
  virtual OnboardingUiTracker* GetOnboardingUiTracker() = 0;
  virtual PhoneModel* GetPhoneModel() = 0;
  virtual TetherController* GetTetherController() = 0;
  virtual UserActionRecorder* GetUserActionRecorder() = 0;

 protected:
  PhoneHubManager() = default;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_PHONE_HUB_MANAGER_H_
