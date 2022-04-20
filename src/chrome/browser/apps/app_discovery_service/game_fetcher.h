// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_GAME_FETCHER_H_
#define CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_GAME_FETCHER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_discovery_service/app_discovery_util.h"
#include "chrome/browser/apps/app_discovery_service/app_fetcher_manager.h"
#include "chrome/browser/apps/app_provisioning_service/app_provisioning_data_manager.h"

namespace apps {

// This class is responsible for parsing & filtering the data received from
// the ComponentInstaller.
class GameFetcher : public AppFetcher,
                    public AppProvisioningDataManager::Observer {
 public:
  explicit GameFetcher(Profile* profile);
  GameFetcher(const GameFetcher&) = delete;
  GameFetcher& operator=(const GameFetcher&) = delete;
  ~GameFetcher() override;

  // AppFetcher:
  void GetApps(ResultCallback callback) override;
  base::CallbackListSubscription RegisterForAppUpdates(
      RepeatingResultCallback callback) override;

  // AppProvisioningDataManager::Observer:
  void OnAppDataUpdated(const proto::AppWithLocaleList& app_data) override;

 private:
  std::vector<Result> GetAppsForCurrentLocale(
      const proto::AppWithLocaleList& app_data);

  std::vector<Result> last_results_;

  raw_ptr<Profile> profile_;

  ResultCallbackList result_callback_list_;

  base::ScopedObservation<AppProvisioningDataManager,
                          AppProvisioningDataManager::Observer>
      app_provisioning_data_observeration_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_GAME_FETCHER_H_
