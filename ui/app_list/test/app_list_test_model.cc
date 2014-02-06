// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/test/app_list_test_model.h"

#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"

namespace app_list {
namespace test {

// static
const char AppListTestModel::kItemType[] = "TestItem";

// AppListTestModel::AppListTestItem

AppListTestModel::AppListTestItem::AppListTestItem(
    const std::string& id,
    AppListTestModel* model)
    : AppListItem(id),
      model_(model) {
}
AppListTestModel::AppListTestItem::~AppListTestItem() {
}

void AppListTestModel::AppListTestItem::Activate(int event_flags) {
  model_->ItemActivated(this);
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
  return AppListModel::AddItem(make_scoped_ptr(item));
}

AppListItem* AppListTestModel::AddItemToFolder(AppListItem* item,
                                               const std::string& folder_id) {
  return AppListModel::AddItemToFolder(make_scoped_ptr(item), folder_id);
}

void AppListTestModel::MoveItemToFolder(AppListItem* item,
                                          const std::string& folder_id) {
  AppListModel::MoveItemToFolder(item, folder_id);
}


std::string AppListTestModel::GetItemName(int id) {
  return base::StringPrintf("Item %d", id);
}

void AppListTestModel::PopulateApps(int n) {
  int start_index = static_cast<int>(item_list()->item_count());
  for (int i = 0; i < n; ++i)
    CreateAndAddItem(GetItemName(start_index + i));
}

void AppListTestModel::PopulateAppWithId(int id) {
  CreateAndAddItem(GetItemName(id));
}

std::string AppListTestModel::GetModelContent() {
  std::string content;
  for (size_t i = 0; i < item_list()->item_count(); ++i) {
    if (i > 0)
      content += ',';
    content += item_list()->item_at(i)->title();
  }
  return content;
}

AppListTestModel::AppListTestItem* AppListTestModel::CreateItem(
    const std::string& title,
    const std::string& full_name) {
  AppListTestItem* item = new AppListTestItem(title, this);
  size_t nitems = item_list()->item_count();
  syncer::StringOrdinal position;
  if (nitems == 0)
    position = syncer::StringOrdinal::CreateInitialOrdinal();
  else
    position = item_list()->item_at(nitems - 1)->position().CreateAfter();
  item->SetPosition(position);
  item->SetTitleAndFullName(title, full_name);
  return item;
}

void AppListTestModel::CreateAndAddItem(const std::string& title,
                                        const std::string& full_name) {
  scoped_ptr<AppListTestItem> test_item(CreateItem(title, full_name));
  AppListModel::AddItem(test_item.PassAs<AppListItem>());
}

void AppListTestModel::CreateAndAddItem(const std::string& title) {
  CreateAndAddItem(title, title);
}

void AppListTestModel::HighlightItemAt(int index) {
  AppListItem* item = item_list()->item_at(index);
  item->SetHighlighted(true);
}

void AppListTestModel::ItemActivated(AppListTestItem* item) {
  last_activated_ = item;
  ++activate_count_;
}

}  // namespace test
}  // namespace app_list
