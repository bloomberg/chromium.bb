// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/model/app_list_item.h"

#include "ash/app_list/model/app_list_item_observer.h"
#include "ash/public/cpp/app_list/app_list_config_provider.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

namespace {

// The maximum number of children that a folder is allowed to have. The
// restriction was a result of UI restrictions for paged folder views, with a
// goal to reduce size taken by the page switcher within the folder UI. While
// this is not relevant concern when the folder items grid is scrollabe, the
// restriction is kept to reduce risk of creating overflown folders via sync on
// devices that do not yet use scrollable folder UI.
constexpr int kMaxFolderChildren = 48;

}  // namespace

AppListItem::AppListItem(const std::string& id)
    : metadata_(std::make_unique<AppListItemMetadata>()) {
  metadata_->id = id;
}

AppListItem::~AppListItem() {
  for (auto& observer : observers_)
    observer.ItemBeingDestroyed();
}

void AppListItem::SetIcon(AppListConfigType config_type,
                          const gfx::ImageSkia& icon) {
  per_config_icons_[config_type] = icon;

  for (auto& observer : observers_)
    observer.ItemIconChanged(config_type);
}

const gfx::ImageSkia& AppListItem::GetIcon(
    AppListConfigType config_type) const {
  const auto& it = per_config_icons_.find(config_type);
  if (it != per_config_icons_.end())
    return it->second;

  // If icon for requested config cannt be found, default to the shared config
  // icon.
  return metadata_->icon;
}

void AppListItem::SetDefaultIcon(const gfx::ImageSkia& icon) {
  metadata_->icon = icon;

  // If the item does not have a config specific icon, it will be represented by
  // the (possibly scaled) default icon, which means that changing the default
  // icon will also change item icons for configs that don't have a designated
  // icon.
  for (auto config_type :
       AppListConfigProvider::Get().GetAvailableConfigTypes()) {
    if (per_config_icons_.find(config_type) == per_config_icons_.end()) {
      for (auto& observer : observers_)
        observer.ItemIconChanged(config_type);
    }
  }
}

const gfx::ImageSkia& AppListItem::GetDefaultIcon() const {
  return metadata_->icon;
}

void AppListItem::SetIconVersion(int icon_version) {
  if (metadata_->icon_version == icon_version)
    return;

  // Clears last set icon if any. AppIconLoadHelper use that to decide
  // whether to trigger an icon load when it is created with UI.
  metadata_->icon = gfx::ImageSkia();

  metadata_->icon_version = icon_version;
  for (auto& observer : observers_) {
    observer.ItemIconVersionChanged();
  }
}

void AppListItem::SetNotificationBadgeColor(const SkColor color) {
  metadata_->badge_color = color;
  for (auto& observer : observers_) {
    observer.ItemBadgeColorChanged();
  }
}

void AppListItem::SetIconColor(const IconColor color) {
  metadata_->icon_color = color;
}

void AppListItem::AddObserver(AppListItemObserver* observer) {
  observers_.AddObserver(observer);
}

void AppListItem::RemoveObserver(AppListItemObserver* observer) {
  observers_.RemoveObserver(observer);
}

const char* AppListItem::GetItemType() const {
  static const char* app_type = "";
  return app_type;
}

AppListItem* AppListItem::FindChildItem(const std::string& id) {
  return nullptr;
}

AppListItem* AppListItem::GetChildItemAt(size_t index) {
  return nullptr;
}

size_t AppListItem::ChildItemCount() const {
  return 0;
}

bool AppListItem::IsFolderFull() const {
  return is_folder() && ChildItemCount() >= kMaxFolderChildren;
}

std::string AppListItem::ToDebugString() const {
  return id().substr(0, 8) + " '" + (is_page_break() ? "page_break" : name()) +
         "'" + " [" + position().ToDebugString() + "]";
}

// Protected methods

void AppListItem::SetName(const std::string& name) {
  if (metadata_->name == name && (short_name_.empty() || short_name_ == name))
    return;
  metadata_->name = name;
  short_name_.clear();
  for (auto& observer : observers_)
    observer.ItemNameChanged();
}

void AppListItem::SetNameAndShortName(const std::string& name,
                                      const std::string& short_name) {
  if (metadata_->name == name && short_name_ == short_name)
    return;
  metadata_->name = name;
  short_name_ = short_name;
  for (auto& observer : observers_)
    observer.ItemNameChanged();
}

void AppListItem::UpdateNotificationBadge(bool has_badge) {
  if (has_notification_badge_ == has_badge)
    return;

  has_notification_badge_ = has_badge;
  for (auto& observer : observers_) {
    observer.ItemBadgeVisibilityChanged();
  }
}

void AppListItem::SetIsNewInstall(bool is_new_install) {
  if (metadata_->is_new_install == is_new_install)
    return;

  metadata_->is_new_install = is_new_install;
  for (auto& observer : observers_) {
    observer.ItemIsNewInstallChanged();
  }
}
}  // namespace ash
