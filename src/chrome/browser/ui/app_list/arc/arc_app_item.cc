// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_app_item.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/arc/arc_app_context_menu.h"
#include "content/public/browser/browser_thread.h"

// static
const char ArcAppItem::kItemType[] = "ArcAppItem";

ArcAppItem::ArcAppItem(
    Profile* profile,
    AppListModelUpdater* model_updater,
    const app_list::AppListSyncableService::SyncItem* sync_item,
    const std::string& id,
    const std::string& name)
    : ChromeAppListItem(profile, id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  SetName(name);

  if (sync_item && sync_item->item_ordinal.IsValid())
    UpdateFromSync(sync_item);
  else
    SetDefaultPositionIfApplicable(model_updater);

  // Set model updater last to avoid being called during construction.
  set_model_updater(model_updater);
}

ArcAppItem::~ArcAppItem() = default;

const char* ArcAppItem::GetItemType() const {
  return ArcAppItem::kItemType;
}

void ArcAppItem::Activate(int event_flags) {
  Launch(event_flags, arc::UserInteractionType::APP_STARTED_FROM_LAUNCHER);
}

void ArcAppItem::ExecuteLaunchCommand(int event_flags) {
  Launch(event_flags,
         arc::UserInteractionType::APP_STARTED_FROM_LAUNCHER_CONTEXT_MENU);
  MaybeDismissAppList();
}

void ArcAppItem::SetName(const std::string& name) {
  SetNameAndShortName(name, name);
}

void ArcAppItem::GetContextMenuModel(GetMenuModelCallback callback) {
  context_menu_ = std::make_unique<ArcAppContextMenu>(this, profile(), id(),
                                                      GetController());
  context_menu_->GetMenuModel(std::move(callback));
}

void ArcAppItem::Launch(int event_flags, arc::UserInteractionType interaction) {
  arc::LaunchApp(profile(), id(), event_flags, interaction,
                 GetController()->GetAppListDisplayId());
}

app_list::AppContextMenu* ArcAppItem::GetAppContextMenu() {
  return context_menu_.get();
}
