// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_RESTORE_FULL_RESTORE_DATA_HANDLER_H_
#define CHROME_BROWSER_ASH_APP_RESTORE_FULL_RESTORE_DATA_HANDLER_H_

#include "components/services/app_service/public/cpp/app_registry_cache.h"

class Profile;

namespace ash {
namespace full_restore {

// The FullRestoreDataHandler class observes AppRegistryCache to remove the app
// launching and app windows when the app is removed.
class FullRestoreDataHandler : public apps::AppRegistryCache::Observer {
 public:
  explicit FullRestoreDataHandler(Profile* profile);
  ~FullRestoreDataHandler() override;

  FullRestoreDataHandler(const FullRestoreDataHandler&) = delete;
  FullRestoreDataHandler& operator=(const FullRestoreDataHandler&) = delete;

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

 private:
  Profile* profile_ = nullptr;

  base::WeakPtrFactory<FullRestoreDataHandler> weak_ptr_factory_{this};
};

}  // namespace full_restore
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_RESTORE_FULL_RESTORE_DATA_HANDLER_H_
