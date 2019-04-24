// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/plugin_vm/plugin_vm_app_item.h"

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "ui/gfx/image/image_skia.h"

// static
const char PluginVmAppItem::kItemType[] = "PluginVmAppItem";

PluginVmAppItem::PluginVmAppItem(
    Profile* profile,
    AppListModelUpdater* model_updater,
    const app_list::AppListSyncableService::SyncItem* sync_item,
    const std::string& id,
    const std::string& name,
    const gfx::ImageSkia* image_skia)
    : ChromeAppListItem(profile, id) {
  SetIcon(*image_skia);
  SetName(name);
  if (sync_item && sync_item->item_ordinal.IsValid()) {
    UpdateFromSync(sync_item);
  } else {
    SetDefaultPositionIfApplicable(model_updater);
  }
}

PluginVmAppItem::~PluginVmAppItem() {}

const char* PluginVmAppItem::GetItemType() const {
  return PluginVmAppItem::kItemType;
}

void PluginVmAppItem::Activate(int event_flags) {
  if (plugin_vm::IsPluginVmConfigured(profile())) {
    // TODO(http://crbug.com/904853): Start PluginVm.
    // Manually close app_list view because focus is not changed on PluginVm app
    // start, and current view remains active.
    GetController()->DismissView();
  } else {
    plugin_vm::ShowPluginVmLauncherView(profile());
  }
}
