// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_REORDER_APP_LIST_REORDER_UTIL_H_
#define CHROME_BROWSER_UI_APP_LIST_REORDER_APP_LIST_REORDER_UTIL_H_

#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"
#include "components/sync/model/string_ordinal.h"

namespace app_list {
namespace reorder {

// If the entropy (i.e. the ratio of the number of items out of order to the
// total number) is greater than this value, the sort order is reset to kCustom.
extern const float kOrderResetThreshold;

struct ReorderParam {
  ReorderParam(const std::string& new_sync_item_id,
               const syncer::StringOrdinal& new_ordinal);
  ReorderParam(const ReorderParam&);
  ~ReorderParam();

  // The sync item id.
  std::string sync_item_id;

  // The new ordinal for the sync item identified by `sync_item_id`.
  syncer::StringOrdinal ordinal;
};

// Wrapping a sync item for comparison.
template <typename T>
struct SyncItemWrapper {
  explicit SyncItemWrapper(const AppListSyncableService::SyncItem& sync_item);
  explicit SyncItemWrapper(const ChromeAppListItem& app_list_item);

  std::string id;
  syncer::StringOrdinal item_ordinal;
  bool is_folder = false;

  // The attribute for comparison.
  T key_attribute;
};

template <typename T>
bool operator<(const SyncItemWrapper<T>& lhs, const SyncItemWrapper<T>& rhs) {
  return lhs.key_attribute < rhs.key_attribute;
}
template <typename T>
bool operator>(const SyncItemWrapper<T>& lhs, const SyncItemWrapper<T>& rhs) {
  return lhs.key_attribute > rhs.key_attribute;
}

// Gets a list of wrappers based on the mappings from ids to sync items.
template <typename T>
std::vector<SyncItemWrapper<T>> GenerateWrappersFromSyncItems(
    const AppListSyncableService::SyncItemMap& sync_item_map) {
  std::vector<SyncItemWrapper<T>> wrappers;
  for (const auto& id_item_pair : sync_item_map) {
    auto* sync_item = id_item_pair.second.get();

    if (sync_item->item_type == sync_pb::AppListSpecifics::TYPE_PAGE_BREAK)
      continue;

    wrappers.emplace_back(*sync_item);
  }

  return wrappers;
}

// Gets a list of sync item wrappers based on the given app list items.
template <typename T>
std::vector<SyncItemWrapper<T>> GenerateWrappersFromAppListItems(
    const std::vector<const ChromeAppListItem*>& app_list_items) {
  std::vector<SyncItemWrapper<T>> wrappers;
  for (const auto* app_list_item : app_list_items) {
    if (app_list_item->is_page_break())
      continue;

    wrappers.emplace_back(*app_list_item);
  }
  return wrappers;
}

// SyncItemWrapper<std::string> ------------------------------------------------

template <>
SyncItemWrapper<std::string>::SyncItemWrapper(
    const AppListSyncableService::SyncItem& sync_item);
template <>
SyncItemWrapper<std::string>::SyncItemWrapper(
    const ChromeAppListItem& app_list_item);

// SyncItemWrapper<ash::IconColor> ---------------------------------------------

template <>
SyncItemWrapper<ash::IconColor>::SyncItemWrapper(
    const AppListSyncableService::SyncItem& sync_item);
template <>
SyncItemWrapper<ash::IconColor>::SyncItemWrapper(
    const ChromeAppListItem& app_list_item);

// Color sorting utility methods -----------------------------------------------

// Used to calculate the color grouping of the icon image's background.
// Samples color from the left, right, and top edge of the icon image and
// determines the color group for each. Returns the most common grouping from
// the samples. If all three sampled groups are different, then returns
// 'light_vibrant_group' which is the color group for the light vibrant color of
// the whole icon image.
sync_pb::AppListSpecifics::ColorGroup CalculateBackgroundColorGroup(
    const SkBitmap& source,
    sync_pb::AppListSpecifics::ColorGroup light_vibrant_group);

// Categorize `color` into one of the ColorGroups.
sync_pb::AppListSpecifics::ColorGroup ColorToColorGroup(SkColor color);

// Returns a SortableIconColor which can be used to sort icons based on a
// combination of their background color and their light vibrant color.
ash::IconColor GetSortableIconColorForApp(const std::string& id,
                                          const gfx::ImageSkia& image);

}  // namespace reorder
}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_REORDER_APP_LIST_REORDER_UTIL_H_
