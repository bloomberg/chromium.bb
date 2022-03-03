// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_PHONEHUB_RECENT_APPS_INTERACTION_HANDLER_IMPL_H_
#define ASH_COMPONENTS_PHONEHUB_RECENT_APPS_INTERACTION_HANDLER_IMPL_H_

#include <stdint.h>

#include "ash/components/phonehub/notification.h"
#include "ash/components/phonehub/recent_app_click_observer.h"
#include "ash/components/phonehub/recent_apps_interaction_handler.h"
#include "base/gtest_prod_util.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {
namespace phonehub {

// The handler that exposes APIs to interact with Phone Hub Recent Apps.
class RecentAppsInteractionHandlerImpl : public RecentAppsInteractionHandler {
 public:
  static void RegisterPrefs(PrefRegistrySimple* registry);

  explicit RecentAppsInteractionHandlerImpl(PrefService* pref_service);
  ~RecentAppsInteractionHandlerImpl() override;

  // RecentAppsInteractionHandler:
  void NotifyRecentAppClicked(
      const Notification::AppMetadata& app_metadata) override;
  void AddRecentAppClickObserver(RecentAppClickObserver* observer) override;
  void RemoveRecentAppClickObserver(RecentAppClickObserver* observer) override;
  void NotifyRecentAppAddedOrUpdated(
      const Notification::AppMetadata& app_metadata,
      base::Time last_accessed_timestamp) override;
  std::vector<Notification::AppMetadata> FetchRecentAppMetadataList() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(RecentAppsInteractionHandlerTest, RecentAppsUpdated);

  void LoadRecentAppMetadataListFromPrefIfNeed();
  void SaveRecentAppMetadataListToPref();

  // Whether this class has finished loading |recent_app_metadata_list_| from
  // pref.
  bool has_loaded_prefs_ = false;

  base::ObserverList<RecentAppClickObserver> observer_list_;
  std::vector<std::pair<Notification::AppMetadata, base::Time>>
      recent_app_metadata_list_;
  PrefService* pref_service_;
};

}  // namespace phonehub
}  // namespace ash

#endif  // ASH_COMPONENTS_PHONEHUB_RECENT_APPS_INTERACTION_HANDLER_IMPL_H_
