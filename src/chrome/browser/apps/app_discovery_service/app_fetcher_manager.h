// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_APP_FETCHER_MANAGER_H_
#define CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_APP_FETCHER_MANAGER_H_

#include <memory>

#include "chrome/browser/apps/app_discovery_service/app_discovery_util.h"

class Profile;

namespace apps {

// Interface implemented by app providers.
class AppFetcher {
 public:
  virtual ~AppFetcher() = default;

  virtual void GetApps(ResultCallback callback) = 0;
};

// Backend for app fetching requests.
class AppFetcherManager {
 public:
  explicit AppFetcherManager(Profile* profile);
  AppFetcherManager(const AppFetcherManager&) = delete;
  AppFetcherManager& operator=(const AppFetcherManager&) = delete;
  ~AppFetcherManager();

  void GetApps(ResultType result_type, ResultCallback callback);

  static void SetOverrideFetcherForTesting(AppFetcher* fetcher);

 private:
  std::unique_ptr<AppFetcher> recommended_arc_app_fetcher_;
  std::unique_ptr<AppFetcher> remote_url_fetcher_;

  static AppFetcher* g_test_fetcher_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_APP_FETCHER_MANAGER_H_
