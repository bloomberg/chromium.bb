// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_model_builder.h"

#include <utility>
#include <vector>

#include "chrome/browser/ui/app_list/chrome_app_list_item.h"

AppListModelBuilder::AppListModelBuilder(AppListControllerDelegate* controller,
                                         const char* item_type)
    : controller_(controller), item_type_(item_type) {}

AppListModelBuilder::~AppListModelBuilder() {
}

void AppListModelBuilder::Initialize(app_list::AppListSyncableService* service,
                                     Profile* profile,
                                     AppListModelUpdater* model_updater) {
  DCHECK(!service_ && !profile_ && !model_updater_);
  service_ = service;
  profile_ = profile;
  model_updater_ = model_updater;

  BuildModel();
}

void AppListModelBuilder::InsertApp(std::unique_ptr<ChromeAppListItem> app) {
  if (service_) {
    service_->AddItem(std::move(app));
    return;
  }
  model_updater_->AddItem(std::move(app));
}

void AppListModelBuilder::RemoveApp(const std::string& id,
                                    bool unsynced_change) {
  if (!unsynced_change && service_) {
    service_->RemoveUninstalledItem(id);
    return;
  }
  model_updater_->RemoveUninstalledItem(id);
}

const app_list::AppListSyncableService::SyncItem*
AppListModelBuilder::GetSyncItem(const std::string& id) {
  return service_ ? service_->GetSyncItem(id) : nullptr;
}

ChromeAppListItem* AppListModelBuilder::GetAppItem(const std::string& id) {
  ChromeAppListItem* item = model_updater_->FindItem(id);
  if (item && item->GetItemType() != item_type_) {
    VLOG(2) << "App Item matching id: " << id
            << " has incorrect type: '" << item->GetItemType() << "'";
    return nullptr;
  }
  return item;
}
