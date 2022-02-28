// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_APP_DISCOVERY_SERVICE_H_
#define CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_APP_DISCOVERY_SERVICE_H_

#include <memory>

#include "chrome/browser/apps/app_discovery_service/app_discovery_util.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace apps {

class AppFetcherManager;

// API for consumers to use to fetch apps.
class AppDiscoveryService : public KeyedService {
 public:
  explicit AppDiscoveryService(Profile* profile);
  AppDiscoveryService(const AppDiscoveryService&) = delete;
  AppDiscoveryService& operator=(const AppDiscoveryService&) = delete;
  ~AppDiscoveryService() override;

  // Queries for apps of the requested |result_type|.
  // |callback| is called when a response to the request is ready.
  void GetApps(ResultType result_type, ResultCallback callback);

 private:
  std::unique_ptr<AppFetcherManager> app_fetcher_manager_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_APP_DISCOVERY_SERVICE_H_
