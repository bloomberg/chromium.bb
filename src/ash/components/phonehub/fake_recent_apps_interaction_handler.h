// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_PHONEHUB_FAKE_RECENT_APPS_INTERACTION_HANDLER_H_
#define ASH_COMPONENTS_PHONEHUB_FAKE_RECENT_APPS_INTERACTION_HANDLER_H_

#include <stdint.h>

#include "ash/components/phonehub/notification.h"
#include "ash/components/phonehub/recent_apps_interaction_handler.h"
#include "ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "base/time/time.h"

namespace ash {
namespace phonehub {

class FakeRecentAppsInteractionHandler : public RecentAppsInteractionHandler {
 public:
  FakeRecentAppsInteractionHandler();
  FakeRecentAppsInteractionHandler(const FakeRecentAppsInteractionHandler&) =
      delete;
  FakeRecentAppsInteractionHandler* operator=(
      const FakeRecentAppsInteractionHandler&) = delete;
  ~FakeRecentAppsInteractionHandler() override;

  void OnFeatureStateChanged(
      multidevice_setup::mojom::FeatureState feature_state);

  size_t HandledRecentAppsCount(const std::string& package_name) const {
    return package_name_to_click_count_.at(package_name);
  }

  size_t recent_app_click_observer_count() const {
    return recent_app_click_observer_count_;
  }

  void NotifyRecentAppClicked(
      const Notification::AppMetadata& app_metadata) override;
  void AddRecentAppClickObserver(RecentAppClickObserver* observer) override;
  void RemoveRecentAppClickObserver(RecentAppClickObserver* observer) override;
  void NotifyRecentAppAddedOrUpdated(
      const Notification::AppMetadata& app_metadata,
      base::Time last_accessed_timestamp) override;
  std::vector<Notification::AppMetadata> FetchRecentAppMetadataList() override;
  void SetStreamableApps(const proto::StreamableApps& streamable_apps) override;

 private:
  void ComputeAndUpdateUiState();

  size_t recent_app_click_observer_count_ = 0;
  multidevice_setup::mojom::FeatureState feature_state_ =
      multidevice_setup::mojom::FeatureState::kDisabledByUser;

  std::vector<std::pair<Notification::AppMetadata, base::Time>>
      recent_apps_metadata_;
  std::map<std::string, size_t> package_name_to_click_count_;
};

}  // namespace phonehub
}  // namespace ash

#endif  // ASH_COMPONENTS_PHONEHUB_FAKE_RECENT_APPS_INTERACTION_HANDLER_H_
