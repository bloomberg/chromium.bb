// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/presenter/app_list_presenter_impl.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "ui/app_list/presenter/app_list_presenter_delegate_factory.h"
#include "ui/app_list/presenter/test/app_list_presenter_impl_test_api.h"
#include "ui/app_list/test/app_list_test_view_delegate.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/window.h"
#include "ui/wm/core/default_activation_client.h"
#include "ui/wm/core/window_util.h"

namespace app_list {

namespace {

// Test stub for AppListPresenterDelegate
class AppListPresenterDelegateTest : public AppListPresenterDelegate {
 public:
  AppListPresenterDelegateTest(aura::Window* container,
                               test::AppListTestViewDelegate* view_delegate)
      : container_(container), view_delegate_(view_delegate) {}
  ~AppListPresenterDelegateTest() override {}

  bool init_called() const { return init_called_; }
  bool on_shown_called() const { return on_shown_called_; }
  bool on_dismissed_called() const { return on_dismissed_called_; }
  bool update_bounds_called() const { return update_bounds_called_; }

 private:
  // AppListPresenterDelegate:
  AppListViewDelegate* GetViewDelegate() override { return view_delegate_; }
  void Init(AppListView* view,
            int64_t display_id,
            int current_apps_page) override {
    init_called_ = true;
    view_ = view;
    view->InitAsFramelessWindow(container_, current_apps_page,
                                gfx::Rect(100, 50, 300, 200));
  }
  void OnShown(int64_t display_id) override { on_shown_called_ = true; }
  void OnDismissed() override { on_dismissed_called_ = true; }
  void UpdateBounds() override { update_bounds_called_ = true; }
  gfx::Vector2d GetVisibilityAnimationOffset(aura::Window*) override {
    return gfx::Vector2d(0, 0);
  }

 private:
  aura::Window* container_;
  test::AppListTestViewDelegate* view_delegate_;
  AppListView* view_ = nullptr;
  bool init_called_ = false;
  bool on_shown_called_ = false;
  bool on_dismissed_called_ = false;
  bool update_bounds_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(AppListPresenterDelegateTest);
};

// Test fake for AppListPresenterDelegateFactory, creates instances of
// AppListPresenterDelegateTest.
class AppListPresenterDelegateFactoryTest
    : public AppListPresenterDelegateFactory {
 public:
  explicit AppListPresenterDelegateFactoryTest(aura::Window* container)
      : container_(container) {}
  ~AppListPresenterDelegateFactoryTest() override {}

  AppListPresenterDelegateTest* current_delegate() { return current_delegate_; }

  // AppListPresenterDelegateFactory:
  std::unique_ptr<AppListPresenterDelegate> GetDelegate(
      AppListPresenter* presenter) override {
    current_delegate_ =
        new AppListPresenterDelegateTest(container_, &app_list_view_delegate_);
    return base::WrapUnique(current_delegate_);
  }

 private:
  aura::Window* container_;
  AppListPresenterDelegateTest* current_delegate_ = nullptr;
  test::AppListTestViewDelegate app_list_view_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AppListPresenterDelegateFactoryTest);
};

}  // namespace

class AppListPresenterImplTest : public aura::test::AuraTestBase {
 public:
  AppListPresenterImplTest();
  ~AppListPresenterImplTest() override;

  AppListPresenterImpl* presenter() { return presenter_.get(); }
  aura::Window* container() { return container_.get(); }
  int64_t GetDisplayId() { return test_screen()->GetPrimaryDisplay().id(); }

  // Don't cache the return of this method - a new delegate is created every
  // time the app list is shown.
  AppListPresenterDelegateTest* delegate() {
    return factory_->current_delegate();
  }

  // aura::test::AuraTestBase:
  void SetUp() override;
  void TearDown() override;

 private:
  std::unique_ptr<AppListPresenterDelegateFactoryTest> factory_;
  std::unique_ptr<AppListPresenterImpl> presenter_;
  std::unique_ptr<aura::Window> container_;

  DISALLOW_COPY_AND_ASSIGN(AppListPresenterImplTest);
};

AppListPresenterImplTest::AppListPresenterImplTest() {}

AppListPresenterImplTest::~AppListPresenterImplTest() {}

void AppListPresenterImplTest::SetUp() {
  AuraTestBase::SetUp();
  new wm::DefaultActivationClient(root_window());
  container_.reset(CreateNormalWindow(0, root_window(), nullptr));
  factory_.reset(new AppListPresenterDelegateFactoryTest(container_.get()));
  presenter_.reset(new AppListPresenterImpl(factory_.get()));
}

void AppListPresenterImplTest::TearDown() {
  container_.reset();
  AuraTestBase::TearDown();
}

// Tests that app launcher is dismissed when focus moves to a window which is
// not app list window's sibling and that appropriate delegate callbacks are
// executed when the app launcher is shown and then when the app launcher is
// dismissed.
TEST_F(AppListPresenterImplTest, HideOnFocusOut) {
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(root_window());
  presenter()->Show(GetDisplayId());
  EXPECT_TRUE(delegate()->init_called());
  EXPECT_TRUE(delegate()->on_shown_called());
  EXPECT_FALSE(delegate()->on_dismissed_called());
  EXPECT_FALSE(delegate()->update_bounds_called());
  focus_client->FocusWindow(presenter()->GetWindow());
  EXPECT_TRUE(presenter()->GetTargetVisibility());

  std::unique_ptr<aura::Window> window(
      CreateNormalWindow(1, root_window(), nullptr));
  focus_client->FocusWindow(window.get());

  EXPECT_TRUE(delegate()->on_dismissed_called());
  EXPECT_FALSE(delegate()->update_bounds_called());
  EXPECT_FALSE(presenter()->GetTargetVisibility());
}

// Tests that app launcher remains visible when focus moves to a window which
// is app list window's sibling and that appropriate delegate callbacks are
// executed when the app launcher is shown.
TEST_F(AppListPresenterImplTest, RemainVisibleWhenFocusingToSibling) {
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(root_window());
  presenter()->Show(GetDisplayId());
  focus_client->FocusWindow(presenter()->GetWindow());
  EXPECT_TRUE(presenter()->GetTargetVisibility());
  EXPECT_TRUE(delegate()->init_called());
  EXPECT_TRUE(delegate()->on_shown_called());
  EXPECT_FALSE(delegate()->on_dismissed_called());
  EXPECT_FALSE(delegate()->update_bounds_called());

  // Create a sibling window.
  std::unique_ptr<aura::Window> window(
      CreateNormalWindow(1, container(), nullptr));
  focus_client->FocusWindow(window.get());

  EXPECT_TRUE(presenter()->GetTargetVisibility());
  EXPECT_FALSE(delegate()->on_dismissed_called());
  EXPECT_FALSE(delegate()->update_bounds_called());
}

// Tests that UpdateBounds is called on the delegate when the root window
// is resized.
TEST_F(AppListPresenterImplTest, RootWindowResize) {
  presenter()->Show(GetDisplayId());
  EXPECT_FALSE(delegate()->update_bounds_called());
  gfx::Rect bounds = root_window()->bounds();
  bounds.Inset(-10, 0);
  root_window()->SetBounds(bounds);
  EXPECT_TRUE(delegate()->update_bounds_called());
}

// Tests that the app list is dismissed and the delegate is destroyed when the
// app list's widget is destroyed.
TEST_F(AppListPresenterImplTest, WidgetDestroyed) {
  presenter()->Show(GetDisplayId());
  EXPECT_TRUE(presenter()->GetTargetVisibility());
  presenter()->GetView()->GetWidget()->CloseNow();
  EXPECT_FALSE(presenter()->GetTargetVisibility());
  test::AppListPresenterImplTestApi presenter_test_api(presenter());
  EXPECT_FALSE(presenter_test_api.presenter_delegate());
}

}  // namespace app_list
