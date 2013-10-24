// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/test/app_list_test_model.h"

#include "base/strings/stringprintf.h"
#include "ui/app_list/app_list_item_model.h"

namespace app_list {
namespace test {

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
  int start_index = apps()->item_count();
  for (int i = 0; i < n; ++i)
    CreateAndAddItem(GetItemName(start_index + i));
}

void AppListTestModel::PopulateAppWithId(int id) {
  CreateAndAddItem(GetItemName(id));
}

std::string AppListTestModel::GetModelContent() {
  std::string content;
  for (size_t i = 0; i < apps()->item_count(); ++i) {
    if (i > 0)
      content += ',';
    content += apps()->GetItemAt(i)->title();
  }
  return content;
}

AppListItemModel* AppListTestModel::CreateItem(const std::string& title,
                                               const std::string& full_name) {
  AppListItemModel* item = new AppListTestItemModel(title, this);
  item->SetTitleAndFullName(title, full_name);
  return item;
}

void AppListTestModel::CreateAndAddItem(const std::string& title,
                                        const std::string& full_name) {
  AddItem(CreateItem(title, full_name));
}

void AppListTestModel::CreateAndAddItem(const std::string& title) {
  CreateAndAddItem(title, title);
}

void AppListTestModel::HighlightItemAt(int index) {
  AppListItemModel* item = apps()->GetItemAt(index);
  item->SetHighlighted(true);
}

void AppListTestModel::ItemActivated(AppListTestItemModel* item) {
  last_activated_ = item;
  ++activate_count_;
}

}  // namespace test
}  // namespace app_list
