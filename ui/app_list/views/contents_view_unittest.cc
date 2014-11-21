// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/contents_view.h"

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/test/app_list_test_model.h"
#include "ui/app_list/test/app_list_test_view_delegate.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/contents_animator.h"
#include "ui/app_list/views/search_box_view.h"

namespace app_list {
namespace test {

class ContentsViewTest : public testing::Test {
 public:
  ContentsViewTest() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExperimentalAppList);

    delegate_.reset(new AppListTestViewDelegate());
    main_view_.reset(new app_list::AppListMainView(delegate_.get()));
    search_box_view_.reset(
        new SearchBoxView(main_view_.get(), delegate_.get()));

    main_view_->Init(NULL, 0, search_box_view_.get());
    DCHECK(GetContentsView());
  }

  ~ContentsViewTest() override {}

 protected:
  ContentsView* GetContentsView() const { return main_view_->contents_view(); }

 private:
  scoped_ptr<AppListTestViewDelegate> delegate_;
  scoped_ptr<AppListMainView> main_view_;
  scoped_ptr<SearchBoxView> search_box_view_;

  DISALLOW_COPY_AND_ASSIGN(ContentsViewTest);
};

TEST_F(ContentsViewTest, CustomAnimationsTest) {
  ContentsView* contents_view = GetContentsView();
  ContentsAnimator* animator;
  bool reverse = true;  // Set to true to make sure it is written.

  // A transition without a custom animation.
  animator = contents_view->GetAnimatorForTransitionForTests(
      contents_view->GetPageIndexForState(AppListModel::STATE_START),
      contents_view->GetPageIndexForState(AppListModel::STATE_SEARCH_RESULTS),
      &reverse);
  EXPECT_EQ("DefaultAnimator", animator->NameForTests());
  EXPECT_FALSE(reverse);

  // Test some custom animations.
  reverse = true;
  animator = contents_view->GetAnimatorForTransitionForTests(
      contents_view->GetPageIndexForState(AppListModel::STATE_START),
      contents_view->GetPageIndexForState(AppListModel::STATE_APPS), &reverse);
  EXPECT_EQ("StartToAppsAnimator", animator->NameForTests());
  EXPECT_FALSE(reverse);

  reverse = false;
  animator = contents_view->GetAnimatorForTransitionForTests(
      contents_view->GetPageIndexForState(AppListModel::STATE_APPS),
      contents_view->GetPageIndexForState(AppListModel::STATE_START), &reverse);
  EXPECT_EQ("StartToAppsAnimator", animator->NameForTests());
  EXPECT_TRUE(reverse);
}

}  // namespace test
}  // namespace app_list
