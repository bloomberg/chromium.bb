// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/chrome_shelf_item_factory.h"

#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/shelf_types.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/shelf/app_shortcut_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/arc_playstore_shortcut_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/browser_app_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "chrome/browser/ui/ash/shelf/standalone_browser_extension_app_shelf_item_controller.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/cpp/types_util.h"

ChromeShelfItemFactory::ChromeShelfItemFactory() = default;

ChromeShelfItemFactory::~ChromeShelfItemFactory() = default;

bool ChromeShelfItemFactory::CreateShelfItemForAppId(
    const std::string& app_id,
    ash::ShelfItem* item,
    std::unique_ptr<ash::ShelfItemDelegate>* delegate) {
  ash::ShelfID shelf_id = ash::ShelfID(app_id);
  ash::ShelfItem shelf_item;
  shelf_item.id = shelf_id;
  *item = shelf_item;

  if (app_id == arc::kPlayStoreAppId) {
    *delegate = std::make_unique<ArcPlaystoreShortcutShelfItemController>();
    return true;
  }

  Profile* profile = GetPrimaryProfile();
  auto* proxy =
      apps::AppServiceProxyFactory::GetInstance()->GetForProfile(profile);
  apps::mojom::AppType app_type = proxy->AppRegistryCache().GetAppType(app_id);

  if (BrowserAppShelfControllerShouldHandleApp(app_id, profile)) {
    *delegate =
        std::make_unique<BrowserAppShelfItemController>(shelf_id, profile);
    return true;
  }

  if (app_type == apps::mojom::AppType::kStandaloneBrowserChromeApp) {
    *delegate =
        std::make_unique<StandaloneBrowserExtensionAppShelfItemController>(
            shelf_id);
    return true;
  }

  *delegate = std::make_unique<AppShortcutShelfItemController>(shelf_id);
  return true;
}

Profile* ChromeShelfItemFactory::GetPrimaryProfile() {
  return ProfileManager::GetPrimaryUserProfile();
}
