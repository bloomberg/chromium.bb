// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/model/app_list_item.h"

#include "ash/app_list/model/app_list_item_observer.h"

namespace ash {

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
  if (config_type == AppListConfigType::kShared) {
    metadata_->icon = icon;
  } else {
    per_config_icons_[config_type] = icon;
  }
  icon.EnsureRepsForSupportedScales();

  for (auto& observer : observers_)
    observer.ItemIconChanged(config_type);
}

const gfx::ImageSkia& AppListItem::GetIcon(
    AppListConfigType config_type) const {
  if (config_type != AppListConfigType::kShared) {
    const auto& it = per_config_icons_.find(config_type);
    if (it != per_config_icons_.end())
      return it->second;
    // If icon for requested config cannt be found, default to the shared config
    // icon.
  }
  return metadata_->icon;
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

size_t AppListItem::ChildItemCount() const {
  return 0;
}

std::string AppListItem::ToDebugString() const {
  return id().substr(0, 8) + " '" + name() + "'" + " [" +
         position().ToDebugString() + "]";
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

}  // namespace ash
