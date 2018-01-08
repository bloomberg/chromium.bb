// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/test/app_list_test_model.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/image/image_skia.h"

namespace app_list {
namespace test {

gfx::ImageSkia CreateImageSkia(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseARGB(255, 0, 255, 0);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

// static
const char AppListTestModel::kItemType[] = "TestItem";

// AppListTestModel::AppListTestItem

AppListTestModel::AppListTestItem::AppListTestItem(
    const std::string& id,
    AppListTestModel* model)
    : AppListItem(id),
      model_(model) {
  SetIcon(CreateImageSkia(kGridIconDimension, kGridIconDimension));
}

AppListTestModel::AppListTestItem::~AppListTestItem() {
}

void AppListTestModel::AppListTestItem::Activate(int event_flags) {
  model_->ItemActivated(this);
}

ui::MenuModel* AppListTestModel::AppListTestItem::GetContextMenuModel() {
  return nullptr;
}

const char* AppListTestModel::AppListTestItem::GetItemType() const {
  return AppListTestModel::kItemType;
}

void AppListTestModel::AppListTestItem::SetPosition(
    const syncer::StringOrdinal& new_position) {
  set_position(new_position);
}

// AppListTestModel

AppListTestModel::AppListTestModel()
    : activate_count_(0),
      last_activated_(NULL) {
}

AppListItem* AppListTestModel::AddItem(AppListItem* item) {
  return AppListModel::AddItem(base::WrapUnique(item));
}

AppListItem* AppListTestModel::AddItemToFolder(AppListItem* item,
                                               const std::string& folder_id) {
  return AppListModel::AddItemToFolder(base::WrapUnique(item), folder_id);
}

void AppListTestModel::MoveItemToFolder(AppListItem* item,
                                        const std::string& folder_id) {
  AppListModel::MoveItemToFolder(item, folder_id);
}


std::string AppListTestModel::GetItemName(int id) {
  return base::StringPrintf("Item %d", id);
}

void AppListTestModel::PopulateApps(int n) {
  int start_index = static_cast<int>(top_level_item_list()->item_count());
  for (int i = 0; i < n; ++i)
    CreateAndAddItem(GetItemName(start_index + i));
}

AppListFolderItem* AppListTestModel::CreateAndPopulateFolderWithApps(int n) {
  DCHECK_GT(n, 1);
  int start_index = static_cast<int>(top_level_item_list()->item_count());
  AppListTestItem* item = CreateAndAddItem(GetItemName(start_index));
  std::string merged_item_id = item->id();
  for (int i = 1; i < n; ++i) {
    AppListTestItem* new_item = CreateAndAddItem(GetItemName(start_index + i));
    merged_item_id = AppListModel::MergeItems(merged_item_id, new_item->id());
  }
  AppListItem* merged_item = FindItem(merged_item_id);
  DCHECK(merged_item->GetItemType() == AppListFolderItem::kItemType);
  return static_cast<AppListFolderItem*>(merged_item);
}

AppListFolderItem* AppListTestModel::CreateAndAddOemFolder(
    const std::string& id) {
  AppListFolderItem* folder =
      new AppListFolderItem(id, AppListFolderItem::FOLDER_TYPE_OEM);
  return static_cast<AppListFolderItem*>(AddItem(folder));
}

AppListFolderItem* AppListTestModel::CreateSingleItemFolder(
    const std::string& folder_id,
    const std::string& item_id) {
  AppListTestItem* item = CreateItem(item_id);
  AddItemToFolder(item, folder_id);
  AppListItem* folder_item = FindItem(folder_id);
  DCHECK(folder_item->GetItemType() == AppListFolderItem::kItemType);
  return static_cast<AppListFolderItem*>(folder_item);
}

void AppListTestModel::PopulateAppWithId(int id) {
  CreateAndAddItem(GetItemName(id));
}

std::string AppListTestModel::GetModelContent() {
  std::string content;
  for (size_t i = 0; i < top_level_item_list()->item_count(); ++i) {
    if (i > 0)
      content += ',';
    content += top_level_item_list()->item_at(i)->id();
  }
  return content;
}

AppListTestModel::AppListTestItem* AppListTestModel::CreateItem(
    const std::string& id) {
  AppListTestItem* item = new AppListTestItem(id, this);
  size_t nitems = top_level_item_list()->item_count();
  syncer::StringOrdinal position;
  if (nitems == 0) {
    position = syncer::StringOrdinal::CreateInitialOrdinal();
  } else {
    position =
        top_level_item_list()->item_at(nitems - 1)->position().CreateAfter();
  }
  item->SetPosition(position);
  SetItemName(item, id);
  return item;
}

AppListTestModel::AppListTestItem* AppListTestModel::CreateAndAddItem(
    const std::string& id) {
  std::unique_ptr<AppListTestItem> test_item(CreateItem(id));
  AppListItem* item = AppListModel::AddItem(std::move(test_item));
  return static_cast<AppListTestItem*>(item);
}
void AppListTestModel::HighlightItemAt(int index) {
  AppListItem* item = top_level_item_list()->item_at(index);
  top_level_item_list()->HighlightItemInstalledFromUI(item->id());
}

void AppListTestModel::ItemActivated(AppListTestItem* item) {
  last_activated_ = item;
  ++activate_count_;
}

}  // namespace test
}  // namespace app_list
