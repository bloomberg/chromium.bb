// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame/non_client_frame_view_ash.h"

#include <memory>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/frame/header_view.h"
#include "ash/frame/wide_frame_view.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/caption_buttons/frame_caption_button_container_view.h"
#include "ash/public/cpp/default_frame_header.h"
#include "ash/public/cpp/immersive/immersive_fullscreen_controller.h"
#include "ash/public/cpp/immersive/immersive_fullscreen_controller_test_api.h"
#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/wm_event.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/test_accelerator_target.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/test/draw_waiter_for_test.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/caption_button_layout_constants.h"
#include "ui/views/window/frame_caption_button.h"
#include "ui/views/window/vector_icons/vector_icons.h"
#include "ui/wm/core/window_util.h"

namespace ash {

// A views::WidgetDelegate which uses a NonClientFrameViewAsh.
class NonClientFrameViewAshTestWidgetDelegate
    : public views::WidgetDelegateView {
 public:
  NonClientFrameViewAshTestWidgetDelegate() = default;
  ~NonClientFrameViewAshTestWidgetDelegate() override = default;

  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override {
    non_client_frame_view_ = new NonClientFrameViewAsh(widget);
    return non_client_frame_view_;
  }

  int GetNonClientFrameViewTopBorderHeight() {
    return non_client_frame_view_->NonClientTopBorderHeight();
  }

  NonClientFrameViewAsh* non_client_frame_view() const {
    return non_client_frame_view_;
  }

  HeaderView* header_view() const {
    return non_client_frame_view_->header_view_;
  }

 private:
  // Not owned.
  NonClientFrameViewAsh* non_client_frame_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(NonClientFrameViewAshTestWidgetDelegate);
};

class TestWidgetConstraintsDelegate
    : public NonClientFrameViewAshTestWidgetDelegate {
 public:
  TestWidgetConstraintsDelegate() = default;
  ~TestWidgetConstraintsDelegate() override = default;

  // views::View:
  gfx::Size GetMinimumSize() const override { return minimum_size_; }

  gfx::Size GetMaximumSize() const override { return maximum_size_; }

  views::View* GetContentsView() override {
    // Set this instance as the contents view so that the maximum and minimum
    // size constraints will be used.
    return this;
  }

  // views::WidgetDelegate:
  bool CanMaximize() const override { return true; }

  bool CanMinimize() const override { return true; }

  void set_minimum_size(const gfx::Size& min_size) { minimum_size_ = min_size; }

  void set_maximum_size(const gfx::Size& max_size) { maximum_size_ = max_size; }

  const gfx::Rect& GetFrameCaptionButtonContainerViewBounds() {
    return non_client_frame_view()
        ->GetFrameCaptionButtonContainerViewForTest()
        ->bounds();
  }

  void EndFrameCaptionButtonContainerViewAnimations() {
    FrameCaptionButtonContainerView::TestApi test(
        non_client_frame_view()->GetFrameCaptionButtonContainerViewForTest());
    test.EndAnimations();
  }

  int GetTitleBarHeight() const {
    return non_client_frame_view()->NonClientTopBorderHeight();
  }

 private:
  gfx::Size minimum_size_;
  gfx::Size maximum_size_;

  DISALLOW_COPY_AND_ASSIGN(TestWidgetConstraintsDelegate);
};

using NonClientFrameViewAshTest = AshTestBase;

// Verifies the client view is not placed at a y location of 0.
TEST_F(NonClientFrameViewAshTest, ClientViewCorrectlyPlaced) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(new NonClientFrameViewAshTestWidgetDelegate);
  EXPECT_NE(0, widget->client_view()->bounds().y());
}

// Test that the height of the header is correct upon initially displaying
// the widget.
TEST_F(NonClientFrameViewAshTest, HeaderHeight) {
  NonClientFrameViewAshTestWidgetDelegate* delegate =
      new NonClientFrameViewAshTestWidgetDelegate;

  std::unique_ptr<views::Widget> widget = CreateTestWidget(delegate);

  // The header should have enough room for the window controls. The
  // header/content separator line overlays the window controls.
  EXPECT_EQ(views::GetCaptionButtonLayoutSize(
                views::CaptionButtonLayoutSize::kNonBrowserCaption)
                .height(),
            delegate->non_client_frame_view()->GetHeaderView()->height());
}

// Regression test for https://crbug.com/839955
TEST_F(NonClientFrameViewAshTest, ActiveStateOfButtonMatchesWidget) {
  NonClientFrameViewAshTestWidgetDelegate* delegate =
      new NonClientFrameViewAshTestWidgetDelegate;
  std::unique_ptr<views::Widget> widget = CreateTestWidget(delegate);
  FrameCaptionButtonContainerView::TestApi test_api(
      delegate->non_client_frame_view()
          ->GetHeaderView()
          ->caption_button_container());

  widget->Show();
  EXPECT_TRUE(widget->IsActive());
  // The paint state doesn't change till the next paint.
  ui::DrawWaiterForTest::WaitForCommit(widget->GetLayer()->GetCompositor());
  EXPECT_TRUE(test_api.size_button()->paint_as_active());

  // Activate a different widget so the original one loses activation.
  std::unique_ptr<views::Widget> widget2 =
      CreateTestWidget(new NonClientFrameViewAshTestWidgetDelegate);
  widget2->Show();
  ui::DrawWaiterForTest::WaitForCommit(widget->GetLayer()->GetCompositor());

  EXPECT_FALSE(widget->IsActive());
  EXPECT_FALSE(test_api.size_button()->paint_as_active());
}

// Verify that NonClientFrameViewAsh returns the correct minimum and maximum
// frame sizes when the client view does not specify any size constraints.
TEST_F(NonClientFrameViewAshTest, NoSizeConstraints) {
  TestWidgetConstraintsDelegate* delegate = new TestWidgetConstraintsDelegate;
  std::unique_ptr<views::Widget> widget = CreateTestWidget(delegate);

  NonClientFrameViewAsh* non_client_frame_view =
      delegate->non_client_frame_view();
  gfx::Size min_frame_size = non_client_frame_view->GetMinimumSize();
  gfx::Size max_frame_size = non_client_frame_view->GetMaximumSize();

  EXPECT_EQ(delegate->GetTitleBarHeight(), min_frame_size.height());

  // A width and height constraint of 0 denotes unbounded.
  EXPECT_EQ(0, max_frame_size.width());
  EXPECT_EQ(0, max_frame_size.height());
}

// Verify that NonClientFrameViewAsh returns the correct minimum and maximum
// frame sizes when the client view specifies size constraints.
TEST_F(NonClientFrameViewAshTest, MinimumAndMaximumSize) {
  gfx::Size min_client_size(500, 500);
  gfx::Size max_client_size(800, 800);
  TestWidgetConstraintsDelegate* delegate = new TestWidgetConstraintsDelegate;
  delegate->set_minimum_size(min_client_size);
  delegate->set_maximum_size(max_client_size);
  std::unique_ptr<views::Widget> widget = CreateTestWidget(delegate);

  NonClientFrameViewAsh* non_client_frame_view =
      delegate->non_client_frame_view();
  gfx::Size min_frame_size = non_client_frame_view->GetMinimumSize();
  gfx::Size max_frame_size = non_client_frame_view->GetMaximumSize();

  EXPECT_EQ(min_client_size.width(), min_frame_size.width());
  EXPECT_EQ(max_client_size.width(), max_frame_size.width());
  EXPECT_EQ(min_client_size.height() + delegate->GetTitleBarHeight(),
            min_frame_size.height());
  EXPECT_EQ(max_client_size.height() + delegate->GetTitleBarHeight(),
            max_frame_size.height());
}

// Verify that NonClientFrameViewAsh updates the avatar icon based on the
// avatar icon window property.
TEST_F(NonClientFrameViewAshTest, AvatarIcon) {
  TestWidgetConstraintsDelegate* delegate = new TestWidgetConstraintsDelegate;
  std::unique_ptr<views::Widget> widget = CreateTestWidget(delegate);

  NonClientFrameViewAsh* non_client_frame_view =
      delegate->non_client_frame_view();
  EXPECT_FALSE(non_client_frame_view->GetAvatarIconViewForTest());

  // Avatar image becomes available.
  widget->GetNativeWindow()->SetProperty(
      aura::client::kAvatarIconKey,
      new gfx::ImageSkia(gfx::test::CreateImage(27, 27).AsImageSkia()));
  EXPECT_TRUE(non_client_frame_view->GetAvatarIconViewForTest());

  // Avatar image is gone; the ImageView for the avatar icon should be
  // removed.
  widget->GetNativeWindow()->ClearProperty(aura::client::kAvatarIconKey);
  EXPECT_FALSE(non_client_frame_view->GetAvatarIconViewForTest());
}

// Tests that a window is minimized, toggling tablet mode doesn't trigger
// caption button update (https://crbug.com/822890).
TEST_F(NonClientFrameViewAshTest, ToggleTabletModeOnMinimizedWindow) {
  NonClientFrameViewAshTestWidgetDelegate* delegate =
      new NonClientFrameViewAshTestWidgetDelegate;
  std::unique_ptr<views::Widget> widget = CreateTestWidget(delegate);
  FrameCaptionButtonContainerView::TestApi test(
      delegate->non_client_frame_view()
          ->GetHeaderView()
          ->caption_button_container());
  widget->Maximize();

  // Restore icon for size button in maximized window state. Compare by name
  // because the address may not be the same for different build targets in the
  // component build.
  EXPECT_STREQ(views::kWindowControlRestoreIcon.name,
               test.size_button()->icon_definition_for_test()->name);
  widget->Minimize();

  // Enter and exit tablet mode while the window is minimized.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);

  // When unminimizing in non-tablet mode, size button should match with
  // maximized window state, which is restore icon.
  ::wm::Unminimize(widget->GetNativeWindow());
  EXPECT_TRUE(widget->IsMaximized());
  EXPECT_STREQ(views::kWindowControlRestoreIcon.name,
               test.size_button()->icon_definition_for_test()->name);
}

// Verify that when in tablet mode with a maximized window, the height of the
// header is zero.
TEST_F(NonClientFrameViewAshTest, FrameHiddenInTabletModeForMaximizedWindows) {
  NonClientFrameViewAshTestWidgetDelegate* delegate =
      new NonClientFrameViewAshTestWidgetDelegate;
  std::unique_ptr<views::Widget> widget = CreateTestWidget(delegate);
  widget->Maximize();

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(0, delegate->GetNonClientFrameViewTopBorderHeight());
}

// Verify that when in tablet mode with a non maximized window, the height of
// the header is non zero.
TEST_F(NonClientFrameViewAshTest,
       FrameShownInTabletModeForNonMaximizedWindows) {
  auto* delegate = new NonClientFrameViewAshTestWidgetDelegate();
  std::unique_ptr<views::Widget> widget = CreateTestWidget(delegate);

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(views::GetCaptionButtonLayoutSize(
                views::CaptionButtonLayoutSize::kNonBrowserCaption)
                .height(),
            delegate->GetNonClientFrameViewTopBorderHeight());
}

// Verify that if originally in fullscreen mode, and enter tablet mode, the
// height of the header remains zero.
TEST_F(NonClientFrameViewAshTest,
       FrameRemainsHiddenInTabletModeWhenTogglingFullscreen) {
  NonClientFrameViewAshTestWidgetDelegate* delegate =
      new NonClientFrameViewAshTestWidgetDelegate;
  std::unique_ptr<views::Widget> widget = CreateTestWidget(delegate);

  widget->SetFullscreen(true);
  EXPECT_EQ(0, delegate->GetNonClientFrameViewTopBorderHeight());
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(0, delegate->GetNonClientFrameViewTopBorderHeight());
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_EQ(0, delegate->GetNonClientFrameViewTopBorderHeight());
}

TEST_F(NonClientFrameViewAshTest, OpeningAppsInTabletMode) {
  auto* delegate = new TestWidgetConstraintsDelegate;
  std::unique_ptr<views::Widget> widget = CreateTestWidget(delegate);
  widget->Maximize();

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(0, delegate->GetNonClientFrameViewTopBorderHeight());

  // Verify that after minimizing and showing the widget, the height of the
  // header is zero.
  widget->Minimize();
  widget->Show();
  EXPECT_TRUE(widget->IsMaximized());
  EXPECT_EQ(0, delegate->GetNonClientFrameViewTopBorderHeight());

  // Verify that when we toggle maximize, the header is shown. For example,
  // maximized can be toggled in tablet mode by using the accessibility
  // keyboard.
  WMEvent event(WM_EVENT_TOGGLE_MAXIMIZE);
  WindowState::Get(widget->GetNativeWindow())->OnWMEvent(&event);
  EXPECT_EQ(0, delegate->GetNonClientFrameViewTopBorderHeight());

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_EQ(views::GetCaptionButtonLayoutSize(
                views::CaptionButtonLayoutSize::kNonBrowserCaption)
                .height(),
            delegate->GetNonClientFrameViewTopBorderHeight());
}

// Test if creating a new window in tablet mode uses maximzied state
// and immersive mode.
TEST_F(NonClientFrameViewAshTest, GetPreferredOnScreenHeightInTabletMaximzied) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  auto* delegate = new TestWidgetConstraintsDelegate;
  std::unique_ptr<views::Widget> widget = CreateTestWidget(delegate);
  auto* frame_view = static_cast<NonClientFrameViewAsh*>(
      widget->non_client_view()->frame_view());
  auto* header_view = frame_view->GetHeaderView();
  ASSERT_TRUE(widget->IsMaximized());
  EXPECT_TRUE(header_view->in_immersive_mode());
  static_cast<ImmersiveFullscreenControllerDelegate*>(header_view)
      ->SetVisibleFraction(0.5);
  // The height should be ~(33 *.5)
  EXPECT_NEAR(16, header_view->GetPreferredOnScreenHeight(), 1);
  static_cast<ImmersiveFullscreenControllerDelegate*>(header_view)
      ->SetVisibleFraction(0.0);
  EXPECT_EQ(0, header_view->GetPreferredOnScreenHeight());
}

// Verify windows that are minimized and then entered into tablet mode will have
// no header when unminimized in tablet mode.
TEST_F(NonClientFrameViewAshTest, MinimizedWindowsInTabletMode) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(new NonClientFrameViewAshTestWidgetDelegate);
  widget->GetNativeWindow()->SetProperty(
      aura::client::kResizeBehaviorKey,
      aura::client::kResizeBehaviorCanMaximize);
  widget->Maximize();
  widget->Minimize();
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  widget->Show();
  EXPECT_EQ(widget->non_client_view()->bounds(),
            widget->client_view()->bounds());
}

TEST_F(NonClientFrameViewAshTest, HeaderVisibilityInFullscreen) {
  auto* delegate = new NonClientFrameViewAshTestWidgetDelegate();
  std::unique_ptr<views::Widget> widget = CreateTestWidget(delegate);
  NonClientFrameViewAsh* non_client_frame_view =
      delegate->non_client_frame_view();
  HeaderView* header_view = non_client_frame_view->GetHeaderView();
  EXPECT_FALSE(header_view->in_immersive_mode());
  EXPECT_TRUE(header_view->GetVisible());
  widget->SetFullscreen(true);
  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(header_view->in_immersive_mode());
  EXPECT_TRUE(header_view->GetVisible());
  widget->SetFullscreen(false);
  widget->LayoutRootViewIfNecessary();
  EXPECT_FALSE(header_view->in_immersive_mode());
  EXPECT_TRUE(header_view->GetVisible());

  // Turn immersive off, and make sure that header view is invisible
  // in fullscreen.
  widget->SetFullscreen(true);
  ImmersiveFullscreenController::EnableForWidget(widget.get(), false);
  widget->LayoutRootViewIfNecessary();
  EXPECT_FALSE(header_view->in_immersive_mode());
  EXPECT_FALSE(header_view->GetVisible());
  widget->SetFullscreen(false);
  widget->LayoutRootViewIfNecessary();
  EXPECT_FALSE(header_view->in_immersive_mode());
  EXPECT_TRUE(header_view->GetVisible());
}

namespace {

class TestButtonModel : public CaptionButtonModel {
 public:
  TestButtonModel() = default;
  ~TestButtonModel() override = default;

  void set_zoom_mode(bool zoom_mode) { zoom_mode_ = zoom_mode; }

  void SetVisible(views::CaptionButtonIcon type, bool visible) {
    if (visible)
      visible_buttons_.insert(type);
    else
      visible_buttons_.erase(type);
  }

  void SetEnabled(views::CaptionButtonIcon type, bool enabled) {
    if (enabled)
      enabled_buttons_.insert(type);
    else
      enabled_buttons_.erase(type);
  }

  // CaptionButtonModel::
  bool IsVisible(views::CaptionButtonIcon type) const override {
    return visible_buttons_.count(type);
  }
  bool IsEnabled(views::CaptionButtonIcon type) const override {
    return enabled_buttons_.count(type);
  }
  bool InZoomMode() const override { return zoom_mode_; }

 private:
  base::flat_set<views::CaptionButtonIcon> visible_buttons_;
  base::flat_set<views::CaptionButtonIcon> enabled_buttons_;
  bool zoom_mode_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestButtonModel);
};

}  // namespace

TEST_F(NonClientFrameViewAshTest, BackButton) {
  AcceleratorControllerImpl* controller =
      Shell::Get()->accelerator_controller();
  std::unique_ptr<TestButtonModel> model = std::make_unique<TestButtonModel>();
  TestButtonModel* model_ptr = model.get();

  auto* delegate = new NonClientFrameViewAshTestWidgetDelegate();
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(delegate, desks_util::GetActiveDeskContainerId(),
                       gfx::Rect(0, 0, 400, 500));

  ui::Accelerator accelerator_back_press(ui::VKEY_BROWSER_BACK, ui::EF_NONE);
  accelerator_back_press.set_key_state(ui::Accelerator::KeyState::PRESSED);
  ui::TestAcceleratorTarget target_back_press;
  controller->Register({accelerator_back_press}, &target_back_press);

  ui::Accelerator accelerator_back_release(ui::VKEY_BROWSER_BACK, ui::EF_NONE);
  accelerator_back_release.set_key_state(ui::Accelerator::KeyState::RELEASED);
  ui::TestAcceleratorTarget target_back_release;
  controller->Register({accelerator_back_release}, &target_back_release);

  NonClientFrameViewAsh* non_client_frame_view =
      delegate->non_client_frame_view();
  non_client_frame_view->SetCaptionButtonModel(std::move(model));

  HeaderView* header_view = non_client_frame_view->GetHeaderView();
  EXPECT_FALSE(header_view->GetBackButton());
  model_ptr->SetVisible(views::CAPTION_BUTTON_ICON_BACK, true);
  non_client_frame_view->SizeConstraintsChanged();
  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(header_view->GetBackButton());
  EXPECT_FALSE(header_view->GetBackButton()->GetEnabled());

  // Back button is disabled, so clicking on it should not should
  // generate back key sequence.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(
      header_view->GetBackButton()->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();
  EXPECT_EQ(0, target_back_press.accelerator_count());
  EXPECT_EQ(0, target_back_release.accelerator_count());

  model_ptr->SetEnabled(views::CAPTION_BUTTON_ICON_BACK, true);
  non_client_frame_view->SizeConstraintsChanged();
  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(header_view->GetBackButton());
  EXPECT_TRUE(header_view->GetBackButton()->GetEnabled());

  // Back button is now enabled, so clicking on it should generate
  // back key sequence.
  generator->MoveMouseTo(
      header_view->GetBackButton()->GetBoundsInScreen().CenterPoint());

  generator->ClickLeftButton();
  EXPECT_EQ(1, target_back_press.accelerator_count());
  EXPECT_EQ(1, target_back_release.accelerator_count());

  model_ptr->SetVisible(views::CAPTION_BUTTON_ICON_BACK, false);
  non_client_frame_view->SizeConstraintsChanged();
  widget->LayoutRootViewIfNecessary();
  EXPECT_FALSE(header_view->GetBackButton());
}

// Make sure that client view occupies the entire window when the
// frame is hidden.
TEST_F(NonClientFrameViewAshTest, FrameVisibility) {
  NonClientFrameViewAshTestWidgetDelegate* delegate =
      new NonClientFrameViewAshTestWidgetDelegate;
  gfx::Rect window_bounds(10, 10, 200, 100);
  std::unique_ptr<views::Widget> widget = CreateTestWidget(
      delegate, desks_util::GetActiveDeskContainerId(), window_bounds);

  // The height is smaller by the top border height.
  gfx::Size client_bounds(200, 68);
  NonClientFrameViewAsh* non_client_frame_view =
      delegate->non_client_frame_view();
  EXPECT_EQ(client_bounds, widget->client_view()->GetLocalBounds().size());

  non_client_frame_view->SetVisible(false);
  widget->GetRootView()->Layout();
  EXPECT_EQ(gfx::Size(200, 100),
            widget->client_view()->GetLocalBounds().size());
  EXPECT_FALSE(widget->non_client_view()->frame_view()->GetVisible());
  EXPECT_EQ(
      window_bounds,
      non_client_frame_view->GetClientBoundsForWindowBounds(window_bounds));

  non_client_frame_view->SetVisible(true);
  widget->GetRootView()->Layout();
  EXPECT_EQ(client_bounds, widget->client_view()->GetLocalBounds().size());
  EXPECT_TRUE(widget->non_client_view()->frame_view()->GetVisible());
  EXPECT_EQ(32, delegate->GetNonClientFrameViewTopBorderHeight());
  EXPECT_EQ(
      gfx::Rect(gfx::Point(10, 42), client_bounds),
      non_client_frame_view->GetClientBoundsForWindowBounds(window_bounds));
}

TEST_F(NonClientFrameViewAshTest, CustomButtonModel) {
  std::unique_ptr<TestButtonModel> model = std::make_unique<TestButtonModel>();
  TestButtonModel* model_ptr = model.get();

  auto* delegate = new NonClientFrameViewAshTestWidgetDelegate();
  std::unique_ptr<views::Widget> widget = CreateTestWidget(delegate);

  NonClientFrameViewAsh* non_client_frame_view =
      delegate->non_client_frame_view();
  non_client_frame_view->SetCaptionButtonModel(std::move(model));

  HeaderView* header_view = non_client_frame_view->GetHeaderView();
  FrameCaptionButtonContainerView::TestApi test_api(
      header_view->caption_button_container());

  // CLOSE button is always enabled.
  EXPECT_TRUE(test_api.close_button());
  EXPECT_FALSE(test_api.close_button()->GetVisible());
  EXPECT_TRUE(test_api.close_button()->GetEnabled());

  model_ptr->SetVisible(views::CAPTION_BUTTON_ICON_CLOSE, true);
  non_client_frame_view->SizeConstraintsChanged();
  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(test_api.close_button()->GetVisible());

  EXPECT_FALSE(test_api.minimize_button()->GetVisible());
  EXPECT_FALSE(test_api.size_button()->GetVisible());
  EXPECT_FALSE(test_api.menu_button()->GetVisible());

  // Back button
  model_ptr->SetVisible(views::CAPTION_BUTTON_ICON_BACK, true);
  non_client_frame_view->SizeConstraintsChanged();
  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(header_view->GetBackButton()->GetVisible());
  EXPECT_FALSE(header_view->GetBackButton()->GetEnabled());

  model_ptr->SetEnabled(views::CAPTION_BUTTON_ICON_BACK, true);
  non_client_frame_view->SizeConstraintsChanged();
  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(header_view->GetBackButton()->GetEnabled());

  // size button
  model_ptr->SetVisible(views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE, true);
  non_client_frame_view->SizeConstraintsChanged();
  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(test_api.size_button()->GetVisible());
  EXPECT_FALSE(test_api.size_button()->GetEnabled());

  model_ptr->SetEnabled(views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE, true);
  non_client_frame_view->SizeConstraintsChanged();
  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(test_api.size_button()->GetEnabled());

  // minimize button
  model_ptr->SetVisible(views::CAPTION_BUTTON_ICON_MINIMIZE, true);
  non_client_frame_view->SizeConstraintsChanged();
  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(test_api.minimize_button()->GetVisible());
  EXPECT_FALSE(test_api.minimize_button()->GetEnabled());

  model_ptr->SetEnabled(views::CAPTION_BUTTON_ICON_MINIMIZE, true);
  non_client_frame_view->SizeConstraintsChanged();
  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(test_api.minimize_button()->GetEnabled());

  // menu button
  model_ptr->SetVisible(views::CAPTION_BUTTON_ICON_MENU, true);
  non_client_frame_view->SizeConstraintsChanged();
  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(test_api.menu_button()->GetVisible());
  EXPECT_FALSE(test_api.menu_button()->GetEnabled());

  model_ptr->SetEnabled(views::CAPTION_BUTTON_ICON_MENU, true);
  non_client_frame_view->SizeConstraintsChanged();
  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(test_api.menu_button()->GetEnabled());

  // zoom button
  EXPECT_STREQ(views::kWindowControlMaximizeIcon.name,
               test_api.size_button()->icon_definition_for_test()->name);
  model_ptr->set_zoom_mode(true);
  non_client_frame_view->SizeConstraintsChanged();
  widget->LayoutRootViewIfNecessary();
  EXPECT_STREQ(kWindowControlZoomIcon.name,
               test_api.size_button()->icon_definition_for_test()->name);
  widget->Maximize();
  EXPECT_STREQ(kWindowControlDezoomIcon.name,
               test_api.size_button()->icon_definition_for_test()->name);
}

TEST_F(NonClientFrameViewAshTest, WideFrame) {
  auto* delegate = new NonClientFrameViewAshTestWidgetDelegate();
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(delegate, desks_util::GetActiveDeskContainerId(),
                       gfx::Rect(100, 0, 400, 500));

  NonClientFrameViewAsh* non_client_frame_view =
      delegate->non_client_frame_view();
  HeaderView* header_view = non_client_frame_view->GetHeaderView();
  widget->Maximize();

  std::unique_ptr<WideFrameView> wide_frame_view =
      std::make_unique<WideFrameView>(widget.get());
  wide_frame_view->GetWidget()->Show();

  HeaderView* wide_header_view = wide_frame_view->header_view();
  display::Screen* screen = display::Screen::GetScreen();

  const gfx::Rect work_area = screen->GetPrimaryDisplay().work_area();
  gfx::Rect frame_bounds =
      wide_frame_view->GetWidget()->GetWindowBoundsInScreen();
  EXPECT_EQ(work_area.width(), frame_bounds.width());
  EXPECT_EQ(work_area.origin(), frame_bounds.origin());
  EXPECT_FALSE(header_view->should_paint());
  EXPECT_TRUE(wide_header_view->should_paint());

  // Test immersive.
  ImmersiveFullscreenController controller;
  wide_frame_view->Init(&controller);
  EXPECT_FALSE(wide_header_view->in_immersive_mode());
  EXPECT_FALSE(header_view->in_immersive_mode());
  EXPECT_TRUE(header_view->GetVisible());

  ImmersiveFullscreenController::EnableForWidget(widget.get(), true);
  EXPECT_TRUE(header_view->in_immersive_mode());
  EXPECT_TRUE(wide_header_view->in_immersive_mode());
  EXPECT_TRUE(header_view->GetVisible());
  // The height should be ~(33 *.5)
  wide_header_view->SetVisibleFraction(0.5);
  EXPECT_NEAR(16, wide_header_view->GetPreferredOnScreenHeight(), 1);

  // Make sure the frame can be revaled outside of the target window.
  EXPECT_FALSE(ImmersiveFullscreenControllerTestApi(&controller)
                   .IsTopEdgeHoverTimerRunning());
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(gfx::Point(10, 0));
  generator->MoveMouseBy(1, 0);
  EXPECT_TRUE(ImmersiveFullscreenControllerTestApi(&controller)
                  .IsTopEdgeHoverTimerRunning());

  generator->MoveMouseTo(gfx::Point(10, 10));
  generator->MoveMouseBy(1, 0);
  EXPECT_FALSE(ImmersiveFullscreenControllerTestApi(&controller)
                   .IsTopEdgeHoverTimerRunning());

  generator->MoveMouseTo(gfx::Point(600, 0));
  generator->MoveMouseBy(1, 0);
  EXPECT_TRUE(ImmersiveFullscreenControllerTestApi(&controller)
                  .IsTopEdgeHoverTimerRunning());

  ImmersiveFullscreenController::EnableForWidget(widget.get(), false);
  EXPECT_FALSE(header_view->in_immersive_mode());
  EXPECT_FALSE(wide_header_view->in_immersive_mode());
  // visible fraction should be ignored in non immersive.
  wide_header_view->SetVisibleFraction(0.5);
  EXPECT_EQ(32, wide_header_view->GetPreferredOnScreenHeight());

  UpdateDisplay("1234x800");
  EXPECT_EQ(1234,
            wide_frame_view->GetWidget()->GetWindowBoundsInScreen().width());

  // Double Click
  EXPECT_TRUE(widget->IsMaximized());
  generator->MoveMouseToCenterOf(
      wide_header_view->GetWidget()->GetNativeWindow());
  generator->DoubleClickLeftButton();
  EXPECT_FALSE(widget->IsMaximized());
}

TEST_F(NonClientFrameViewAshTest, WideFrameButton) {
  auto* delegate = new NonClientFrameViewAshTestWidgetDelegate();
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(delegate, desks_util::GetActiveDeskContainerId(),
                       gfx::Rect(100, 0, 400, 500));

  std::unique_ptr<WideFrameView> wide_frame_view =
      std::make_unique<WideFrameView>(widget.get());
  wide_frame_view->GetWidget()->Show();
  HeaderView* header_view = wide_frame_view->header_view();
  FrameCaptionButtonContainerView::TestApi test_api(
      header_view->caption_button_container());

  EXPECT_STREQ(views::kWindowControlMaximizeIcon.name,
               test_api.size_button()->icon_definition_for_test()->name);
  widget->Maximize();
  header_view->Layout();
  EXPECT_STREQ(views::kWindowControlRestoreIcon.name,
               test_api.size_button()->icon_definition_for_test()->name);

  widget->Restore();
  header_view->Layout();
  EXPECT_STREQ(views::kWindowControlMaximizeIcon.name,
               test_api.size_button()->icon_definition_for_test()->name);

  widget->SetFullscreen(true);
  header_view->Layout();
  EXPECT_STREQ(views::kWindowControlRestoreIcon.name,
               test_api.size_button()->icon_definition_for_test()->name);

  widget->SetFullscreen(false);
  header_view->Layout();
  EXPECT_STREQ(views::kWindowControlMaximizeIcon.name,
               test_api.size_button()->icon_definition_for_test()->name);
}

namespace {

class NonClientFrameViewAshFrameColorTest
    : public NonClientFrameViewAshTest,
      public testing::WithParamInterface<bool> {
 public:
  NonClientFrameViewAshFrameColorTest() = default;
  ~NonClientFrameViewAshFrameColorTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(NonClientFrameViewAshFrameColorTest);
};

class TestWidgetDelegate : public TestWidgetConstraintsDelegate {
 public:
  TestWidgetDelegate(bool custom) : custom_(custom) {}
  ~TestWidgetDelegate() override = default;

  // views::WidgetDelegate:
  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override {
    if (custom_) {
      WindowState* window_state = WindowState::Get(widget->GetNativeWindow());
      window_state->SetDelegate(std::make_unique<WindowStateDelegate>());
    }
    return TestWidgetConstraintsDelegate::CreateNonClientFrameView(widget);
  }

 private:
  bool custom_;

  DISALLOW_COPY_AND_ASSIGN(TestWidgetDelegate);
};

}  // namespace

// Verify that NonClientFrameViewAsh updates the active color based on the
// kFrameActiveColorKey window property.
TEST_P(NonClientFrameViewAshFrameColorTest, kFrameActiveColorKey) {
  TestWidgetDelegate* delegate = new TestWidgetDelegate(GetParam());
  std::unique_ptr<views::Widget> widget = CreateTestWidget(delegate);

  SkColor active_color =
      widget->GetNativeWindow()->GetProperty(kFrameActiveColorKey);
  constexpr SkColor new_color = SK_ColorWHITE;
  EXPECT_NE(active_color, new_color);

  widget->GetNativeWindow()->SetProperty(kFrameActiveColorKey, new_color);
  active_color = widget->GetNativeWindow()->GetProperty(kFrameActiveColorKey);
  EXPECT_EQ(active_color, new_color);
  EXPECT_EQ(new_color,
            delegate->non_client_frame_view()->GetActiveFrameColorForTest());

  // Test that changing the property updates the caption button images.
  FrameCaptionButtonContainerView::TestApi test_api(
      delegate->non_client_frame_view()
          ->GetHeaderView()
          ->caption_button_container());
  ui::DrawWaiterForTest::WaitForCommit(widget->GetLayer()->GetCompositor());
  gfx::ImageSkia original_icon_image = test_api.size_button()->icon_image();
  widget->GetNativeWindow()->SetProperty(kFrameActiveColorKey, SK_ColorBLACK);
  ui::DrawWaiterForTest::WaitForCommit(widget->GetLayer()->GetCompositor());
  EXPECT_FALSE(original_icon_image.BackedBySameObjectAs(
      test_api.size_button()->icon_image()));
}

// Verify that NonClientFrameViewAsh updates the inactive color based on the
// kFrameInactiveColorKey window property.
TEST_P(NonClientFrameViewAshFrameColorTest, KFrameInactiveColor) {
  TestWidgetDelegate* delegate = new TestWidgetDelegate(GetParam());
  std::unique_ptr<views::Widget> widget = CreateTestWidget(delegate);

  SkColor active_color =
      widget->GetNativeWindow()->GetProperty(kFrameInactiveColorKey);
  constexpr SkColor new_color = SK_ColorWHITE;
  EXPECT_NE(active_color, new_color);

  widget->GetNativeWindow()->SetProperty(kFrameInactiveColorKey, new_color);
  active_color = widget->GetNativeWindow()->GetProperty(kFrameInactiveColorKey);
  EXPECT_EQ(active_color, new_color);
  EXPECT_EQ(new_color,
            delegate->non_client_frame_view()->GetInactiveFrameColorForTest());
}

// Verify that NonClientFrameViewAsh updates the active color based on the
// kFrameActiveColorKey window property.
TEST_P(NonClientFrameViewAshFrameColorTest, WideFrameInitialColor) {
  TestWidgetDelegate* delegate = new TestWidgetDelegate(GetParam());
  std::unique_ptr<views::Widget> widget = CreateTestWidget(delegate);
  aura::Window* window = widget->GetNativeWindow();
  SkColor active_color = window->GetProperty(kFrameActiveColorKey);
  SkColor inactive_color = window->GetProperty(kFrameInactiveColorKey);
  constexpr SkColor new_active_color = SK_ColorWHITE;
  constexpr SkColor new_inactive_color = SK_ColorBLACK;
  EXPECT_NE(active_color, new_active_color);
  EXPECT_NE(inactive_color, new_inactive_color);
  window->SetProperty(kFrameActiveColorKey, new_active_color);
  window->SetProperty(kFrameInactiveColorKey, new_inactive_color);

  std::unique_ptr<WideFrameView> wide_frame_view =
      std::make_unique<WideFrameView>(widget.get());
  HeaderView* wide_header_view = wide_frame_view->header_view();
  DefaultFrameHeader* header = wide_header_view->GetFrameHeader();
  EXPECT_EQ(new_active_color, header->active_frame_color_for_testing());
  EXPECT_EQ(new_inactive_color, header->inactive_frame_color_for_testing());
}

// Run frame color tests with and without custom WindowStateDelegate.
INSTANTIATE_TEST_SUITE_P(All,
                         NonClientFrameViewAshFrameColorTest,
                         testing::Bool());

}  // namespace ash
