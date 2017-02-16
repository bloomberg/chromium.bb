// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_item.h"

#include "base/logging.h"
#include "ui/app_list/app_list_item_observer.h"

namespace app_list {

AppListItem::AppListItem(const std::string& id)
    : id_(id),
      highlighted_(false),
      is_installing_(false),
      percent_downloaded_(-1) {
}

AppListItem::~AppListItem() {
  for (auto& observer : observers_)
    observer.ItemBeingDestroyed();
}

void AppListItem::SetIcon(const gfx::ImageSkia& icon) {
  icon_ = icon;
  icon_.EnsureRepsForSupportedScales();
  for (auto& observer : observers_)
    observer.ItemIconChanged();
}

void AppListItem::SetIsInstalling(bool is_installing) {
  if (is_installing_ == is_installing)
    return;

  is_installing_ = is_installing;
  for (auto& observer : observers_)
    observer.ItemIsInstallingChanged();
}

void AppListItem::SetPercentDownloaded(int percent_downloaded) {
  if (percent_downloaded_ == percent_downloaded)
    return;

  percent_downloaded_ = percent_downloaded;
  for (auto& observer : observers_)
    observer.ItemPercentDownloadedChanged();
}

void AppListItem::AddObserver(AppListItemObserver* observer) {
  observers_.AddObserver(observer);
}

void AppListItem::RemoveObserver(AppListItemObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AppListItem::Activate(int event_flags) {
}

const char* AppListItem::GetItemType() const {
  static const char* app_type = "";
  return app_type;
}

ui::MenuModel* AppListItem::GetContextMenuModel() {
  return NULL;
}

AppListItem* AppListItem::FindChildItem(const std::string& id) {
  return NULL;
}

size_t AppListItem::ChildItemCount() const {
  return 0;
}

void AppListItem::OnExtensionPreferenceChanged() {}

bool AppListItem::CompareForTest(const AppListItem* other) const {
  return id_ == other->id_ &&
      folder_id_ == other->folder_id_ &&
      name_ == other->name_ &&
      short_name_ == other->short_name_ &&
      GetItemType() == other->GetItemType() &&
      position_.Equals(other->position_);
}

std::string AppListItem::ToDebugString() const {
  return id_.substr(0, 8) + " '" + name_ + "'"
      + " [" + position_.ToDebugString() + "]";
}

// Protected methods

void AppListItem::SetName(const std::string& name) {
  if (name_ == name && (short_name_.empty() || short_name_ == name))
    return;
  name_ = name;
  short_name_.clear();
  for (auto& observer : observers_)
    observer.ItemNameChanged();
}

void AppListItem::SetNameAndShortName(const std::string& name,
                                      const std::string& short_name) {
  if (name_ == name && short_name_ == short_name)
    return;
  name_ = name;
  short_name_ = short_name;
  for (auto& observer : observers_)
    observer.ItemNameChanged();
}

}  // namespace app_list
