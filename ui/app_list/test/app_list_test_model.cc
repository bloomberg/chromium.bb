// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/test/app_list_test_model.h"

#include "base/strings/stringprintf.h"
#include "ui/app_list/app_list_item_model.h"

namespace app_list {
namespace test {

// static
const char AppListTestModel::kAppType[] = "FolderItem";

class AppListTestModel::AppListTestItemModel : public AppListItemModel {
 public:
  AppListTestItemModel(const std::string& id, AppListTestModel* model)
      : AppListItemModel(id),
        model_(model) {
  }
  virtual ~AppListTestItemModel() {}

  virtual void Activate(int event_flags) OVERRIDE {
    model_->ItemActivated(this);
  }

  virtual const char* GetAppType() const OVERRIDE {
    return AppListTestModel::kAppType;
  }

 private:
  AppListTestModel* model_;
  DISALLOW_COPY_AND_ASSIGN(AppListTestItemModel);
};

AppListTestModel::AppListTestModel()
    : activate_count_(0),
      last_activated_(NULL) {
  SetSignedIn(true);
}

std::string AppListTestModel::GetItemName(int id) {
  return base::StringPrintf("Item %d", id);
}

void AppListTestModel::PopulateApps(int n) {
  int start_index = item_list()->item_count();
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

AppListItemModel* AppListTestModel::CreateItem(const std::string& title,
                                               const std::string& full_name) {
  AppListItemModel* item = new AppListTestItemModel(title, this);
  size_t nitems = item_list()->item_count();
  syncer::StringOrdinal position;
  if (nitems == 0)
    position = syncer::StringOrdinal::CreateInitialOrdinal();
  else
    position = item_list()->item_at(nitems - 1)->position().CreateAfter();
  item->set_position(position);
  item->SetTitleAndFullName(title, full_name);
  return item;
}

void AppListTestModel::CreateAndAddItem(const std::string& title,
                                        const std::string& full_name) {
  item_list()->AddItem(CreateItem(title, full_name));
}

void AppListTestModel::CreateAndAddItem(const std::string& title) {
  CreateAndAddItem(title, title);
}

void AppListTestModel::HighlightItemAt(int index) {
  AppListItemModel* item = item_list()->item_at(index);
  item->SetHighlighted(true);
}

void AppListTestModel::ItemActivated(AppListTestItemModel* item) {
  last_activated_ = item;
  ++activate_count_;
}

}  // namespace test
}  // namespace app_list
