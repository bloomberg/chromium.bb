// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/apps_grid_view.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/app_list/app_list_item_model.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_item_view.h"
#include "ui/app_list/pagination_model.h"

namespace app_list {
namespace test {

namespace {

const int kIconDimension = 48;
const int kCols = 2;
const int kRows = 2;

const int kWidth = 320;
const int kHeight = 240;

}  // namespace

class AppsGridViewTest : public testing::Test {
 public:
  AppsGridViewTest() {}
  virtual ~AppsGridViewTest() {}

  // testing::Test overrides:
  virtual void SetUp() OVERRIDE {
    apps_model_.reset(new AppListModel::Apps);
    pagination_model_.reset(new PaginationModel);

    apps_grid_view_.reset(new AppsGridView(NULL, pagination_model_.get()));
    apps_grid_view_->SetLayout(kIconDimension, kCols, kRows);
    apps_grid_view_->SetBoundsRect(gfx::Rect(gfx::Size(kWidth, kHeight)));
    apps_grid_view_->SetModel(apps_model_.get());
  }
  virtual void TearDown() OVERRIDE {
    apps_grid_view_.reset();  // Release apps grid view before models.
  }

 protected:
  void PopulateApps(int n) {
    for (int i = 0; i < n; ++i) {
      std::string title = base::StringPrintf("Item %d", i);
      apps_model_->Add(CreateItem(title));
    }
  }

  AppListItemModel* CreateItem(const std::string& title) {
    AppListItemModel* item = new AppListItemModel;
    item->SetTitle(title);
    return item;
  }

  void HighlightItemAt(int index) {
    AppListItemModel* item = apps_model_->GetItemAt(index);
    item->SetHighlighted(true);
  }

  scoped_ptr<AppListModel::Apps> apps_model_;
  scoped_ptr<PaginationModel> pagination_model_;
  scoped_ptr<AppsGridView> apps_grid_view_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppsGridViewTest);
};

TEST_F(AppsGridViewTest, CreatePage) {
  // Fully populates a page.
  const int kPages = 1;
  PopulateApps(kPages * apps_grid_view_->tiles_per_page());
  EXPECT_EQ(kPages, pagination_model_->total_pages());

  // Adds one more and gets a new page created.
  apps_model_->Add(CreateItem(std::string("Extra")));
  EXPECT_EQ(kPages + 1, pagination_model_->total_pages());
}

TEST_F(AppsGridViewTest, EnsureHighlightedVisible) {
  const int kPages = 3;
  PopulateApps(kPages * apps_grid_view_->tiles_per_page());
  EXPECT_EQ(kPages, pagination_model_->total_pages());
  EXPECT_EQ(0, pagination_model_->selected_page());

  // Highlight first one and last one one first page and first page should be
  // selected.
  HighlightItemAt(0);
  EXPECT_EQ(0, pagination_model_->selected_page());
  HighlightItemAt(apps_grid_view_->tiles_per_page() - 1);
  EXPECT_EQ(0, pagination_model_->selected_page());

  // Highlight first one on 2nd page and 2nd page should be selected.
  HighlightItemAt(apps_grid_view_->tiles_per_page() + 1);
  EXPECT_EQ(1, pagination_model_->selected_page());

  // Highlight last one in the model and last page should be selected.
  HighlightItemAt(apps_model_->item_count() - 1);
  EXPECT_EQ(kPages - 1, pagination_model_->selected_page());
}

}  // namespace test
}  // namespace app_list
