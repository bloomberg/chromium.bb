// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/app_list_view.h"

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/pagination_model.h"
#include "ui/app_list/test/app_list_test_model.h"
#include "ui/app_list/test/app_list_test_view_delegate.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/app_list/views/search_box_view.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/window.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"

namespace app_list {
namespace test {

namespace {

// Choose a set that is 3 regular app list pages and 2 landscape app list pages.
const int kInitialItems = 34;

// Allows the same tests to run with different contexts: either an Ash-style
// root window or a desktop window tree host.
class AppListViewTestContext {
 public:
  AppListViewTestContext(bool is_landscape, aura::Window* parent);
  ~AppListViewTestContext();

  // Test displaying the app list and performs a standard set of checks on its
  // top level views. Then closes the window.
  void RunDisplayTest();

  // A standard set of checks on a view, e.g., ensuring it is drawn and visible.
  static void CheckView(views::View* subview);

  // Invoked when the Widget is closing, and the view it contains is about to
  // be torn down. This only occurs in a run loop and will be used as a signal
  // to quit.
  void NativeWidgetClosing() {
    view_ = NULL;
    run_loop_->Quit();
  }

  // Whether the experimental "landscape" app launcher UI is being tested.
  bool is_landscape() const { return is_landscape_; }

 private:
  const bool is_landscape_;
  scoped_ptr<base::RunLoop> run_loop_;
  PaginationModel pagination_model_;
  app_list::AppListView* view_;  // Owned by native widget.
  app_list::test::AppListTestViewDelegate* delegate_;  // Owned by |view_|;

  DISALLOW_COPY_AND_ASSIGN(AppListViewTestContext);
};

// Extend the regular AppListTestViewDelegate to communicate back to the test
// context. Note the test context doesn't simply inherit this, because the
// delegate is owned by the view.
class UnitTestViewDelegate : public app_list::test::AppListTestViewDelegate {
 public:
  UnitTestViewDelegate(AppListViewTestContext* parent) : parent_(parent) {}

  // Overridden from app_list::AppListViewDelegate:
  virtual bool ShouldCenterWindow() const OVERRIDE {
    return app_list::switches::IsCenteredAppListEnabled();
  }

  // Overridden from app_list::test::AppListTestViewDelegate:
  virtual void ViewClosing() OVERRIDE { parent_->NativeWidgetClosing(); }

 private:
  AppListViewTestContext* parent_;

  DISALLOW_COPY_AND_ASSIGN(UnitTestViewDelegate);
};

AppListViewTestContext::AppListViewTestContext(bool is_landscape,
                                               aura::Window* parent)
    : is_landscape_(is_landscape) {
  if (is_landscape_) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableCenteredAppList);
  }

  delegate_ = new UnitTestViewDelegate(this);
  view_ = new app_list::AppListView(delegate_);

  // Initialize centered around a point that ensures the window is wholly shown.
  view_->InitAsBubbleAtFixedLocation(parent,
                                     &pagination_model_,
                                     gfx::Point(300, 300),
                                     views::BubbleBorder::FLOAT,
                                     false /* border_accepts_events */);
}

AppListViewTestContext::~AppListViewTestContext() {
  // The view observes the PaginationModel which is about to get destroyed, so
  // if the view is not already deleted by the time this destructor is called,
  // there will be problems.
  EXPECT_FALSE(view_);
}

// static
void AppListViewTestContext::CheckView(views::View* subview) {
  ASSERT_TRUE(subview);
  EXPECT_TRUE(subview->parent());
  EXPECT_TRUE(subview->visible());
  EXPECT_TRUE(subview->IsDrawn());
}

void AppListViewTestContext::RunDisplayTest() {
  EXPECT_FALSE(view_->GetWidget()->IsVisible());
  EXPECT_EQ(-1, pagination_model_.total_pages());
  view_->GetWidget()->Show();
  delegate_->GetTestModel()->PopulateApps(kInitialItems);

  run_loop_.reset(new base::RunLoop);
  view_->SetNextPaintCallback(run_loop_->QuitClosure());
  run_loop_->Run();

  EXPECT_TRUE(view_->GetWidget()->IsVisible());

  if (is_landscape_)
    EXPECT_EQ(2, pagination_model_.total_pages());
  else
    EXPECT_EQ(3, pagination_model_.total_pages());
  EXPECT_EQ(0, pagination_model_.selected_page());

  // Checks on the main view.
  AppListMainView* main_view = view_->app_list_main_view();
  EXPECT_NO_FATAL_FAILURE(CheckView(main_view));
  EXPECT_NO_FATAL_FAILURE(CheckView(main_view->search_box_view()));
  EXPECT_NO_FATAL_FAILURE(CheckView(main_view->contents_view()));

  view_->GetWidget()->Close();
  run_loop_.reset(new base::RunLoop);
  run_loop_->Run();

  // |view_| should have been deleted and set to NULL via ViewClosing().
  EXPECT_FALSE(view_);
}

class AppListViewTestAura : public views::ViewsTestBase,
                            public ::testing::WithParamInterface<bool> {
 public:
  AppListViewTestAura() {}
  virtual ~AppListViewTestAura() {}

  // testing::Test overrides:
  virtual void SetUp() OVERRIDE {
    views::ViewsTestBase::SetUp();
    test_context_.reset(new AppListViewTestContext(GetParam(), GetContext()));
  }

  virtual void TearDown() OVERRIDE {
    test_context_.reset();
    views::ViewsTestBase::TearDown();
  }

 protected:
  scoped_ptr<AppListViewTestContext> test_context_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListViewTestAura);
};

class AppListViewTestDesktop : public views::ViewsTestBase,
                               public ::testing::WithParamInterface<bool> {
 public:
  AppListViewTestDesktop() {}
  virtual ~AppListViewTestDesktop() {}

  // testing::Test overrides:
  virtual void SetUp() OVERRIDE {
    set_views_delegate(new AppListViewTestViewsDelegate(this));
    views::ViewsTestBase::SetUp();
    test_context_.reset(new AppListViewTestContext(GetParam(), NULL));
  }

  virtual void TearDown() OVERRIDE {
    test_context_.reset();
    views::ViewsTestBase::TearDown();
  }

 protected:
  scoped_ptr<AppListViewTestContext> test_context_;

 private:
  class AppListViewTestViewsDelegate : public views::TestViewsDelegate {
   public:
    AppListViewTestViewsDelegate(AppListViewTestDesktop* parent)
        : parent_(parent) {}

    // Overridden from views::ViewsDelegate:
    virtual void OnBeforeWidgetInit(
        views::Widget::InitParams* params,
        views::internal::NativeWidgetDelegate* delegate) OVERRIDE;

   private:
    AppListViewTestDesktop* parent_;

    DISALLOW_COPY_AND_ASSIGN(AppListViewTestViewsDelegate);
  };

  DISALLOW_COPY_AND_ASSIGN(AppListViewTestDesktop);
};

void AppListViewTestDesktop::AppListViewTestViewsDelegate::OnBeforeWidgetInit(
    views::Widget::InitParams* params,
    views::internal::NativeWidgetDelegate* delegate) {
// Mimic the logic in ChromeViewsDelegate::OnBeforeWidgetInit(). Except, for
// ChromeOS, use the root window from the AuraTestHelper rather than depending
// on ash::Shell:GetPrimaryRootWindow(). Also assume non-ChromeOS is never the
// Ash desktop, as that is covered by AppListViewTestAura.
#if defined(OS_CHROMEOS)
  if (!params->parent && !params->context)
    params->context = parent_->GetContext();
#elif defined(USE_AURA)
  if (params->parent == NULL && params->context == NULL && params->top_level)
    params->native_widget = new views::DesktopNativeWidgetAura(delegate);
#endif
}

}  // namespace

// Tests showing the app list with basic test model in an ash-style root window.
TEST_P(AppListViewTestAura, Display) {
  host()->Show();
  EXPECT_NO_FATAL_FAILURE(test_context_->RunDisplayTest());
}

// Tests showing the app list on the desktop. Note on ChromeOS, this will still
// use the regular root window.
TEST_P(AppListViewTestDesktop, Display) {
  EXPECT_NO_FATAL_FAILURE(test_context_->RunDisplayTest());
}

INSTANTIATE_TEST_CASE_P(AppListViewTestAuraInstance,
                        AppListViewTestAura,
                        ::testing::Bool());

INSTANTIATE_TEST_CASE_P(AppListViewTestDesktopInstance,
                        AppListViewTestDesktop,
                        ::testing::Bool());

}  // namespace test
}  // namespace app_list
