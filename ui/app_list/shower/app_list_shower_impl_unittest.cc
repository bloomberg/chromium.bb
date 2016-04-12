// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/shower/app_list_shower_impl.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "ui/app_list/shower/app_list_shower_delegate_factory.h"
#include "ui/app_list/shower/test/app_list_shower_impl_test_api.h"
#include "ui/app_list/test/app_list_test_view_delegate.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/window.h"
#include "ui/wm/core/default_activation_client.h"
#include "ui/wm/core/window_util.h"

namespace app_list {

namespace {

// Test stub for AppListShowerDelegate
class AppListShowerDelegateTest : public AppListShowerDelegate {
 public:
  AppListShowerDelegateTest(aura::Window* container,
                            test::AppListTestViewDelegate* view_delegate)
      : container_(container), view_delegate_(view_delegate) {}
  ~AppListShowerDelegateTest() override {}

  bool init_called() const { return init_called_; }
  bool on_shown_called() const { return on_shown_called_; }
  bool on_dismissed_called() const { return on_dismissed_called_; }
  bool update_bounds_called() const { return update_bounds_called_; }

 private:
  // AppListShowerDelegate:
  AppListViewDelegate* GetViewDelegate() override { return view_delegate_; }
  void Init(AppListView* view,
            aura::Window* root_window,
            int current_apps_page) override {
    init_called_ = true;
    view_ = view;
    view->InitAsFramelessWindow(container_, current_apps_page,
                                gfx::Rect(100, 50, 300, 200));
  }
  void OnShown(aura::Window*) override { on_shown_called_ = true; }
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

  DISALLOW_COPY_AND_ASSIGN(AppListShowerDelegateTest);
};

// Test fake for AppListShowerDelegateFactory, creates instances of
// AppListShowerDelegateTest.
class AppListShowerDelegateFactoryTest : public AppListShowerDelegateFactory {
 public:
  explicit AppListShowerDelegateFactoryTest(aura::Window* container)
      : container_(container) {}
  ~AppListShowerDelegateFactoryTest() override {}

  AppListShowerDelegateTest* current_delegate() { return current_delegate_; }

  // AppListShowerDelegateFactory:
  std::unique_ptr<AppListShowerDelegate> GetDelegate(
      AppListShower* shower) override {
    current_delegate_ =
        new AppListShowerDelegateTest(container_, &app_list_view_delegate_);
    return base::WrapUnique(current_delegate_);
  }

 private:
  aura::Window* container_;
  AppListShowerDelegateTest* current_delegate_ = nullptr;
  test::AppListTestViewDelegate app_list_view_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AppListShowerDelegateFactoryTest);
};

}  // namespace

class AppListShowerImplTest : public aura::test::AuraTestBase {
 public:
  AppListShowerImplTest();
  ~AppListShowerImplTest() override;

  AppListShowerImpl* shower() { return shower_.get(); }
  aura::Window* container() { return container_.get(); }

  // Don't cache the return of this method - a new delegate is created every
  // time the app list is shown.
  AppListShowerDelegateTest* delegate() { return factory_->current_delegate(); }

  // aura::test::AuraTestBase:
  void SetUp() override;
  void TearDown() override;

 private:
  std::unique_ptr<AppListShowerDelegateFactoryTest> factory_;
  std::unique_ptr<AppListShowerImpl> shower_;
  std::unique_ptr<aura::Window> container_;

  DISALLOW_COPY_AND_ASSIGN(AppListShowerImplTest);
};

AppListShowerImplTest::AppListShowerImplTest() {}

AppListShowerImplTest::~AppListShowerImplTest() {}

void AppListShowerImplTest::SetUp() {
  AuraTestBase::SetUp();
  new wm::DefaultActivationClient(root_window());
  container_.reset(CreateNormalWindow(0, root_window(), nullptr));
  factory_.reset(new AppListShowerDelegateFactoryTest(container_.get()));
  shower_.reset(new AppListShowerImpl(factory_.get()));
}

void AppListShowerImplTest::TearDown() {
  container_.reset();
  AuraTestBase::TearDown();
}

// Tests that app launcher is dismissed when focus moves to a window which is
// not app list window's sibling and that appropriate delegate callbacks are
// executed when the app launcher is shown and then when the app launcher is
// dismissed.
TEST_F(AppListShowerImplTest, HideOnFocusOut) {
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(root_window());
  shower()->Show(container());
  EXPECT_TRUE(delegate()->init_called());
  EXPECT_TRUE(delegate()->on_shown_called());
  EXPECT_FALSE(delegate()->on_dismissed_called());
  EXPECT_FALSE(delegate()->update_bounds_called());
  focus_client->FocusWindow(shower()->GetWindow());
  EXPECT_TRUE(shower()->GetTargetVisibility());

  std::unique_ptr<aura::Window> window(
      CreateNormalWindow(1, root_window(), nullptr));
  focus_client->FocusWindow(window.get());

  EXPECT_TRUE(delegate()->on_dismissed_called());
  EXPECT_FALSE(delegate()->update_bounds_called());
  EXPECT_FALSE(shower()->GetTargetVisibility());
}

// Tests that app launcher remains visible when focus moves to a window which
// is app list window's sibling and that appropriate delegate callbacks are
// executed when the app launcher is shown.
TEST_F(AppListShowerImplTest, RemainVisibleWhenFocusingToSibling) {
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(root_window());
  shower()->Show(container());
  focus_client->FocusWindow(shower()->GetWindow());
  EXPECT_TRUE(shower()->GetTargetVisibility());
  EXPECT_TRUE(delegate()->init_called());
  EXPECT_TRUE(delegate()->on_shown_called());
  EXPECT_FALSE(delegate()->on_dismissed_called());
  EXPECT_FALSE(delegate()->update_bounds_called());

  // Create a sibling window.
  std::unique_ptr<aura::Window> window(
      CreateNormalWindow(1, container(), nullptr));
  focus_client->FocusWindow(window.get());

  EXPECT_TRUE(shower()->GetTargetVisibility());
  EXPECT_FALSE(delegate()->on_dismissed_called());
  EXPECT_FALSE(delegate()->update_bounds_called());
}

// Tests that UpdateBounds is called on the delegate when the root window
// is resized.
TEST_F(AppListShowerImplTest, RootWindowResize) {
  shower()->Show(container());
  EXPECT_FALSE(delegate()->update_bounds_called());
  gfx::Rect bounds = root_window()->bounds();
  bounds.Inset(-10, 0);
  root_window()->SetBounds(bounds);
  EXPECT_TRUE(delegate()->update_bounds_called());
}

// Tests that the app list is dismissed and the delegate is destroyed when the
// app list's widget is destroyed.
TEST_F(AppListShowerImplTest, WidgetDestroyed) {
  shower()->Show(container());
  EXPECT_TRUE(shower()->GetTargetVisibility());
  shower()->GetView()->GetWidget()->CloseNow();
  EXPECT_FALSE(shower()->GetTargetVisibility());
  test::AppListShowerImplTestApi shower_test_api(shower());
  EXPECT_FALSE(shower_test_api.shower_delegate());
}

}  // namespace app_list
