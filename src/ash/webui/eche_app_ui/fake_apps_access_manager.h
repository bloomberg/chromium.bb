// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_FAKE_APPS_ACCESS_MANAGER_H_
#define ASH_WEBUI_ECHE_APP_UI_FAKE_APPS_ACCESS_MANAGER_H_

#include "ash/webui/eche_app_ui/apps_access_manager.h"

namespace chromeos {
namespace settings {
class MultideviceHandlerTest;
}  // namespace settings
}  // namespace chromeos

namespace ash {
namespace eche_app {

class FakeAppsAccessManager : public AppsAccessManager {
 public:
  explicit FakeAppsAccessManager(
      AccessStatus access_status = AccessStatus::kAvailableButNotGranted);
  ~FakeAppsAccessManager() override;

  void SetAppsSetupOperationStatus(AppsAccessSetupOperation::Status new_status);

  using AppsAccessManager::IsSetupOperationInProgress;

  // AppsAccessManager:
  AccessStatus GetAccessStatus() const override;
  void OnSetupRequested() override;

 private:
  friend class chromeos::settings::MultideviceHandlerTest;
  // AppsAccessManager:
  void SetAccessStatusInternal(AccessStatus access_status) override;

  AccessStatus access_status_;
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_FAKE_APPS_ACCESS_MANAGER_H_
