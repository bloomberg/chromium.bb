// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_PLUGIN_VM_PLUGIN_VM_APP_ITEM_H_
#define CHROME_BROWSER_UI_APP_LIST_PLUGIN_VM_PLUGIN_VM_APP_ITEM_H_

#include "chrome/browser/ui/app_list/chrome_app_list_item.h"

namespace gfx {
class ImageSkia;
}

class PluginVmAppItem : public ChromeAppListItem {
 public:
  static const char kItemType[];
  PluginVmAppItem(Profile* profile,
                  AppListModelUpdater* model_updater,
                  const app_list::AppListSyncableService::SyncItem* sync_item,
                  const std::string& id,
                  const std::string& name,
                  const gfx::ImageSkia* image_skia);
  ~PluginVmAppItem() override;

 private:
  // ChromeAppListItem:
  void Activate(int event_flags) override;
  const char* GetItemType() const override;

  DISALLOW_COPY_AND_ASSIGN(PluginVmAppItem);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_PLUGIN_VM_PLUGIN_VM_APP_ITEM_H_
