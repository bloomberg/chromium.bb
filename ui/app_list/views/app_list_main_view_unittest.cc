// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/app_list_main_view.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/app_list/pagination_model.h"
#include "ui/app_list/test/app_list_test_model.h"
#include "ui/app_list/test/app_list_test_view_delegate.h"
#include "ui/app_list/views/apps_container_view.h"
#include "ui/app_list/views/apps_grid_view.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view_model.h"
#include "ui/views/widget/widget.h"

namespace app_list {
namespace test {

namespace {

const int kInitialItems = 2;

class AppListMainViewTest : public views::ViewsTestBase {
 public:
  AppListMainViewTest()
      : widget_(NULL),
        main_view_(NULL) {}

  virtual ~AppListMainViewTest() {}

  // testing::Test overrides:
  virtual void SetUp() OVERRIDE {
    views::ViewsTestBase::SetUp();
    delegate_.reset(new AppListTestViewDelegate);
    delegate_->GetTestModel()->PopulateApps(kInitialItems);

    main_view_ =
        new AppListMainView(delegate_.get(), &pagination_model_, GetContext());
    main_view_->SetPaintToLayer(true);

    widget_ = new views::Widget;
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_POPUP);
    params.bounds.set_size(main_view_->GetPreferredSize());
    widget_->Init(params);

    widget_->SetContentsView(main_view_);
  }

  virtual void TearDown() OVERRIDE {
    widget_->Close();
    views::ViewsTestBase::TearDown();
    delegate_.reset();
  }

  const views::ViewModel* ViewModel() {
    return main_view_->contents_view()->apps_container_view()->apps_grid_view()
        ->view_model_for_test();
  }

 protected:
  views::Widget* widget_;  // Owned by native window.
  AppListMainView* main_view_;  // Owned by |widget_|.
  PaginationModel pagination_model_;
  scoped_ptr<AppListTestViewDelegate> delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListMainViewTest);
};

}  // namespace

// Tests changing the AppListModel when switching profiles.
TEST_F(AppListMainViewTest, ModelChanged) {
  EXPECT_EQ(kInitialItems, ViewModel()->view_size());

  // The model is owned by a profile keyed service, which is never destroyed
  // until after profile switching.
  scoped_ptr<AppListModel> old_model(delegate_->ReleaseTestModel());

  const int kReplacementItems = 5;
  delegate_->ReplaceTestModel(kReplacementItems);
  main_view_->ModelChanged();
  EXPECT_EQ(kReplacementItems, ViewModel()->view_size());
}

}  // namespace test
}  // namespace app_list
