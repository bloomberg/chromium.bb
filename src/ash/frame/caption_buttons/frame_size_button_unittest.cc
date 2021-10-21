// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/caption_buttons/frame_size_button.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "base/i18n/rtl.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/frame/caption_buttons/frame_caption_button_container_view.h"
#include "chromeos/ui/frame/default_frame_header.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "chromeos/ui/wm/features.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/caption_button_layout_constants.h"
#include "ui/views/window/frame_caption_button.h"
#include "ui/views/window/vector_icons/vector_icons.h"

namespace ash {

namespace {

using ::chromeos::FrameCaptionButtonContainerView;
using ::chromeos::FrameSizeButton;
using ::chromeos::WindowStateType;

class TestWidgetDelegate : public views::WidgetDelegateView {
 public:
  explicit TestWidgetDelegate(bool resizable) {
    SetCanMaximize(true);
    SetCanMinimize(true);
    SetCanResize(resizable);
  }

  TestWidgetDelegate(const TestWidgetDelegate&) = delete;
  TestWidgetDelegate& operator=(const TestWidgetDelegate&) = delete;

  ~TestWidgetDelegate() override = default;

  FrameCaptionButtonContainerView* caption_button_container() {
    return caption_button_container_;
  }

 private:
  // Overridden from views::View:
  void Layout() override {
    caption_button_container_->Layout();

    // Right align the caption button container.
    gfx::Size preferred_size = caption_button_container_->GetPreferredSize();
    caption_button_container_->SetBounds(width() - preferred_size.width(), 0,
                                         preferred_size.width(),
                                         preferred_size.height());
  }

  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override {
    if (details.is_add && details.child == this) {
      caption_button_container_ =
          new FrameCaptionButtonContainerView(GetWidget());

      // Set images for the button icons and assign the default caption button
      // size.
      caption_button_container_->SetButtonSize(
          views::GetCaptionButtonLayoutSize(
              views::CaptionButtonLayoutSize::kNonBrowserCaption));
      caption_button_container_->SetButtonImage(
          views::CAPTION_BUTTON_ICON_MINIMIZE,
          views::kWindowControlMinimizeIcon);
      caption_button_container_->SetButtonImage(
          views::CAPTION_BUTTON_ICON_MENU, chromeos::kWindowControlMenuIcon);
      caption_button_container_->SetButtonImage(
          views::CAPTION_BUTTON_ICON_CLOSE, views::kWindowControlCloseIcon);
      caption_button_container_->SetButtonImage(
          views::CAPTION_BUTTON_ICON_LEFT_TOP_SNAPPED,
          chromeos::kWindowControlLeftSnappedIcon);
      caption_button_container_->SetButtonImage(
          views::CAPTION_BUTTON_ICON_RIGHT_BOTTOM_SNAPPED,
          chromeos::kWindowControlRightSnappedIcon);
      caption_button_container()->SetButtonImage(
          views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE,
          views::kWindowControlMaximizeIcon);

      AddChildView(caption_button_container_);
    }
  }

  // Not owned.
  FrameCaptionButtonContainerView* caption_button_container_;
};

class FrameSizeButtonTest : public AshTestBase {
 public:
  FrameSizeButtonTest() = default;
  explicit FrameSizeButtonTest(bool resizable) : resizable_(resizable) {}

  FrameSizeButtonTest(const FrameSizeButtonTest&) = delete;
  FrameSizeButtonTest& operator=(const FrameSizeButtonTest&) = delete;

  ~FrameSizeButtonTest() override = default;

  // Returns the center point of |view| in screen coordinates.
  gfx::Point CenterPointInScreen(views::View* view) {
    return view->GetBoundsInScreen().CenterPoint();
  }

  // Returns true if the window has |state_type|.
  bool HasStateType(WindowStateType state_type) const {
    return window_state()->GetStateType() == state_type;
  }

  // Returns true if all three buttons are in the normal state.
  bool AllButtonsInNormalState() const {
    return minimize_button_->GetState() == views::Button::STATE_NORMAL &&
           size_button_->GetState() == views::Button::STATE_NORMAL &&
           close_button_->GetState() == views::Button::STATE_NORMAL;
  }

  // Creates a widget with |delegate|. The returned widget takes ownership of
  // |delegate|.
  views::Widget* CreateWidget(views::WidgetDelegate* delegate) {
    views::Widget* widget = new views::Widget;
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
    params.delegate = delegate;
    params.bounds = gfx::Rect(10, 10, 100, 100);
    params.context = GetContext();
    widget->Init(std::move(params));
    widget->Show();

    return widget;
  }

  // AshTestBase overrides:
  void SetUp() override {
    AshTestBase::SetUp();

    widget_delegate_ = new TestWidgetDelegate(resizable_);
    widget_ = CreateWidget(widget_delegate_);
    window_state_ = WindowState::Get(widget_->GetNativeWindow());

    FrameCaptionButtonContainerView::TestApi test(
        widget_delegate_->caption_button_container());

    minimize_button_ = test.minimize_button();
    size_button_ = test.size_button();
    static_cast<FrameSizeButton*>(size_button_)
        ->set_delay_to_set_buttons_to_snap_mode(0);
    close_button_ = test.close_button();
  }

  WindowState* window_state() { return window_state_; }
  const WindowState* window_state() const { return window_state_; }
  views::Widget* GetWidget() const { return widget_; }

  views::FrameCaptionButton* minimize_button() { return minimize_button_; }
  views::FrameCaptionButton* size_button() { return size_button_; }
  views::FrameCaptionButton* close_button() { return close_button_; }
  TestWidgetDelegate* widget_delegate() { return widget_delegate_; }

 private:
  // Not owned.
  WindowState* window_state_;
  views::Widget* widget_;
  views::FrameCaptionButton* minimize_button_;
  views::FrameCaptionButton* size_button_;
  views::FrameCaptionButton* close_button_;
  TestWidgetDelegate* widget_delegate_;
  bool resizable_ = true;
};

}  // namespace

// Tests that pressing the left mouse button or tapping down on the size button
// puts the button into the pressed state.
TEST_F(FrameSizeButtonTest, PressedState) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressLeftButton();
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->GetState());
  generator->ReleaseLeftButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(views::Button::STATE_NORMAL, size_button()->GetState());

  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressTouchId(3);
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->GetState());
  generator->ReleaseTouchId(3);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(views::Button::STATE_NORMAL, size_button()->GetState());
}

// Tests that clicking on the size button toggles between the maximized and
// normal state.
TEST_F(FrameSizeButtonTest, ClickSizeButtonTogglesMaximize) {
  EXPECT_FALSE(window_state()->IsMaximized());

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->ClickLeftButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(window_state()->IsMaximized());

  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->ClickLeftButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(window_state()->IsMaximized());

  generator->GestureTapAt(CenterPointInScreen(size_button()));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(window_state()->IsMaximized());

  generator->GestureTapAt(CenterPointInScreen(size_button()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(window_state()->IsMaximized());
}

// Test that clicking + dragging to a button adjacent to the size button snaps
// the window left or right.
TEST_F(FrameSizeButtonTest, ButtonDrag) {
  EXPECT_TRUE(window_state()->IsNormalStateType());

  // 1) Test by dragging the mouse.
  // Snap right.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressLeftButton();
  generator->MoveMouseTo(CenterPointInScreen(close_button()));
  generator->ReleaseLeftButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasStateType(WindowStateType::kSecondarySnapped));

  // Snap left.
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressLeftButton();
  generator->MoveMouseTo(CenterPointInScreen(minimize_button()));
  generator->ReleaseLeftButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasStateType(WindowStateType::kPrimarySnapped));

  // 2) Test with scroll gestures.
  // Snap right.
  generator->GestureScrollSequence(CenterPointInScreen(size_button()),
                                   CenterPointInScreen(close_button()),
                                   base::Milliseconds(100), 3);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasStateType(WindowStateType::kSecondarySnapped));

  // Snap left.
  generator->GestureScrollSequence(CenterPointInScreen(size_button()),
                                   CenterPointInScreen(minimize_button()),
                                   base::Milliseconds(100), 3);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasStateType(WindowStateType::kPrimarySnapped));

  // 3) Test with tap gestures.
  const float touch_default_radius =
      ui::GestureConfiguration::GetInstance()->default_radius();
  ui::GestureConfiguration::GetInstance()->set_default_radius(0);
  // Snap right.
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressMoveAndReleaseTouchTo(CenterPointInScreen(close_button()));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasStateType(WindowStateType::kSecondarySnapped));
  // Snap left.
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressMoveAndReleaseTouchTo(CenterPointInScreen(minimize_button()));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasStateType(WindowStateType::kPrimarySnapped));
  ui::GestureConfiguration::GetInstance()->set_default_radius(
      touch_default_radius);
}

// Test that clicking, dragging, and overshooting the minimize button a bit
// horizontally still snaps the window left.
TEST_F(FrameSizeButtonTest, SnapLeftOvershootMinimize) {
  EXPECT_TRUE(window_state()->IsNormalStateType());

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(CenterPointInScreen(size_button()));

  generator->PressLeftButton();
  // Move to the minimize button.
  generator->MoveMouseTo(CenterPointInScreen(minimize_button()));
  // Overshoot the minimize button.
  generator->MoveMouseBy(-minimize_button()->width(), 0);
  generator->ReleaseLeftButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasStateType(WindowStateType::kPrimarySnapped));
}

// Test that right clicking the size button has no effect.
TEST_F(FrameSizeButtonTest, RightMouseButton) {
  EXPECT_TRUE(window_state()->IsNormalStateType());

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressRightButton();
  generator->ReleaseRightButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(window_state()->IsNormalStateType());
}

// Test that during the waiting to snap mode, if the window's state is changed,
// or the window is put in overview, we should cancel the waiting to snap mode.
TEST_F(FrameSizeButtonTest, CancelSnapTest) {
  EXPECT_EQ(views::Button::STATE_NORMAL, size_button()->GetState());

  // Press on the size button and drag toward to close buton to enter waiting-
  // for-snap mode.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressLeftButton();
  generator->MoveMouseTo(CenterPointInScreen(close_button()));
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->GetState());
  EXPECT_TRUE(
      static_cast<FrameSizeButton*>(size_button())->in_snap_mode_for_testing());
  // Maximize the window.
  window_state()->Maximize();
  EXPECT_EQ(views::Button::STATE_NORMAL, size_button()->GetState());
  EXPECT_FALSE(
      static_cast<FrameSizeButton*>(size_button())->in_snap_mode_for_testing());
  generator->ReleaseLeftButton();

  // Test that if window is put in overview, the waiting-to-snap is canceled.
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressLeftButton();
  generator->MoveMouseTo(CenterPointInScreen(close_button()));
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->GetState());
  EXPECT_TRUE(
      static_cast<FrameSizeButton*>(size_button())->in_snap_mode_for_testing());
  window_state()->window()->SetProperty(chromeos::kIsShowingInOverviewKey,
                                        true);
  EXPECT_EQ(views::Button::STATE_NORMAL, size_button()->GetState());
  EXPECT_FALSE(
      static_cast<FrameSizeButton*>(size_button())->in_snap_mode_for_testing());
  generator->ReleaseLeftButton();
}

// Test that upon releasing the mouse button after having pressed the size
// button
// - The state of all the caption buttons is reset.
// - The icon displayed by all of the caption buttons is reset.
TEST_F(FrameSizeButtonTest, ResetButtonsAfterClick) {
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_MINIMIZE, minimize_button()->GetIcon());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_CLOSE, close_button()->GetIcon());
  EXPECT_TRUE(AllButtonsInNormalState());

  // Pressing the size button should result in the size button being pressed and
  // the minimize and close button icons changing.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressLeftButton();
  EXPECT_EQ(views::Button::STATE_NORMAL, minimize_button()->GetState());
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->GetState());
  EXPECT_EQ(views::Button::STATE_NORMAL, close_button()->GetState());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_LEFT_TOP_SNAPPED,
            minimize_button()->GetIcon());
  EXPECT_TRUE(chromeos::kWindowControlLeftSnappedIcon.name ==
              minimize_button()->icon_definition_for_test()->name);
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_RIGHT_BOTTOM_SNAPPED,
            close_button()->GetIcon());
  EXPECT_TRUE(chromeos::kWindowControlRightSnappedIcon.name ==
              close_button()->icon_definition_for_test()->name);

  // Dragging the mouse over the minimize button should hover the minimize
  // button and the minimize and close button icons should stay changed.
  generator->MoveMouseTo(CenterPointInScreen(minimize_button()));
  EXPECT_EQ(views::Button::STATE_HOVERED, minimize_button()->GetState());
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->GetState());
  EXPECT_EQ(views::Button::STATE_NORMAL, close_button()->GetState());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_LEFT_TOP_SNAPPED,
            minimize_button()->GetIcon());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_RIGHT_BOTTOM_SNAPPED,
            close_button()->GetIcon());

  // Release the mouse, snapping the window left.
  generator->ReleaseLeftButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasStateType(WindowStateType::kPrimarySnapped));

  // None of the buttons should stay pressed and the buttons should have their
  // regular icons.
  EXPECT_TRUE(AllButtonsInNormalState());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_MINIMIZE, minimize_button()->GetIcon());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_CLOSE, close_button()->GetIcon());

  // Repeat test but release button where it does not affect the window's state
  // because the code path is different.
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressLeftButton();
  EXPECT_EQ(views::Button::STATE_NORMAL, minimize_button()->GetState());
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->GetState());
  EXPECT_EQ(views::Button::STATE_NORMAL, close_button()->GetState());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_LEFT_TOP_SNAPPED,
            minimize_button()->GetIcon());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_RIGHT_BOTTOM_SNAPPED,
            close_button()->GetIcon());

  const gfx::Rect work_area_bounds_in_screen =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  generator->MoveMouseTo(work_area_bounds_in_screen.bottom_left());

  // None of the buttons should be pressed because we are really far away from
  // any of the caption buttons. The minimize and close button icons should
  // be changed because the mouse is pressed.
  EXPECT_TRUE(AllButtonsInNormalState());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_LEFT_TOP_SNAPPED,
            minimize_button()->GetIcon());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_RIGHT_BOTTOM_SNAPPED,
            close_button()->GetIcon());

  // Release the mouse. The window should stay snapped left.
  generator->ReleaseLeftButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasStateType(WindowStateType::kPrimarySnapped));

  // The buttons should stay unpressed and the buttons should now have their
  // regular icons.
  EXPECT_TRUE(AllButtonsInNormalState());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_MINIMIZE, minimize_button()->GetIcon());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_CLOSE, close_button()->GetIcon());
}

// Test that the size button is pressed whenever the snap left/right buttons
// are hovered.
TEST_F(FrameSizeButtonTest, SizeButtonPressedWhenSnapButtonHovered) {
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_MINIMIZE, minimize_button()->GetIcon());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_CLOSE, close_button()->GetIcon());
  EXPECT_TRUE(AllButtonsInNormalState());

  // Pressing the size button should result in the size button being pressed and
  // the minimize and close button icons changing.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressLeftButton();
  EXPECT_EQ(views::Button::STATE_NORMAL, minimize_button()->GetState());
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->GetState());
  EXPECT_EQ(views::Button::STATE_NORMAL, close_button()->GetState());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_LEFT_TOP_SNAPPED,
            minimize_button()->GetIcon());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_RIGHT_BOTTOM_SNAPPED,
            close_button()->GetIcon());

  // Dragging the mouse over the minimize button (snap left button) should hover
  // the minimize button and keep the size button pressed.
  generator->MoveMouseTo(CenterPointInScreen(minimize_button()));
  EXPECT_EQ(views::Button::STATE_HOVERED, minimize_button()->GetState());
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->GetState());
  EXPECT_EQ(views::Button::STATE_NORMAL, close_button()->GetState());

  // Moving the mouse far away from the caption buttons and then moving it over
  // the close button (snap right button) should hover the close button and
  // keep the size button pressed.
  const gfx::Rect work_area_bounds_in_screen =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  generator->MoveMouseTo(work_area_bounds_in_screen.bottom_left());
  EXPECT_TRUE(AllButtonsInNormalState());
  generator->MoveMouseTo(CenterPointInScreen(close_button()));
  EXPECT_EQ(views::Button::STATE_NORMAL, minimize_button()->GetState());
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->GetState());
  EXPECT_EQ(views::Button::STATE_HOVERED, close_button()->GetState());
}

class FrameSizeButtonTestRTL : public FrameSizeButtonTest {
 public:
  FrameSizeButtonTestRTL() = default;

  FrameSizeButtonTestRTL(const FrameSizeButtonTestRTL&) = delete;
  FrameSizeButtonTestRTL& operator=(const FrameSizeButtonTestRTL&) = delete;

  ~FrameSizeButtonTestRTL() override = default;

  void SetUp() override {
    original_locale_ = base::i18n::GetConfiguredLocale();
    base::i18n::SetICUDefaultLocale("he");

    FrameSizeButtonTest::SetUp();
  }

  void TearDown() override {
    FrameSizeButtonTest::TearDown();
    base::i18n::SetICUDefaultLocale(original_locale_);
  }

 private:
  std::string original_locale_;
};

// Test that clicking + dragging to a button adjacent to the size button presses
// the correct button and snaps the window to the correct side.
TEST_F(FrameSizeButtonTestRTL, ButtonDrag) {
  // In RTL the close button should be left of the size button and the minimize
  // button should be right of the size button.
  ASSERT_LT(close_button()->GetBoundsInScreen().x(),
            size_button()->GetBoundsInScreen().x());
  ASSERT_LT(size_button()->GetBoundsInScreen().x(),
            minimize_button()->GetBoundsInScreen().x());

  // Test initial state.
  EXPECT_TRUE(window_state()->IsNormalStateType());
  EXPECT_TRUE(AllButtonsInNormalState());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_MINIMIZE, minimize_button()->GetIcon());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_CLOSE, close_button()->GetIcon());

  // Pressing the size button should swap the icons of the minimize and close
  // buttons to icons for snapping right and for snapping left respectively.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressLeftButton();
  EXPECT_EQ(views::Button::STATE_NORMAL, minimize_button()->GetState());
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->GetState());
  EXPECT_EQ(views::Button::STATE_NORMAL, close_button()->GetState());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_RIGHT_BOTTOM_SNAPPED,
            minimize_button()->GetIcon());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_LEFT_TOP_SNAPPED,
            close_button()->GetIcon());

  // Dragging over to the minimize button should press it.
  generator->MoveMouseTo(CenterPointInScreen(minimize_button()));
  EXPECT_EQ(views::Button::STATE_HOVERED, minimize_button()->GetState());
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->GetState());
  EXPECT_EQ(views::Button::STATE_NORMAL, close_button()->GetState());

  // Releasing should snap the window right.
  generator->ReleaseLeftButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasStateType(WindowStateType::kSecondarySnapped));

  // None of the buttons should stay pressed and the buttons should have their
  // regular icons.
  EXPECT_TRUE(AllButtonsInNormalState());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_MINIMIZE, minimize_button()->GetIcon());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_CLOSE, close_button()->GetIcon());
}

namespace {

class FrameSizeButtonNonResizableTest : public FrameSizeButtonTest {
 public:
  FrameSizeButtonNonResizableTest() : FrameSizeButtonTest(false) {}

  FrameSizeButtonNonResizableTest(const FrameSizeButtonNonResizableTest&) =
      delete;
  FrameSizeButtonNonResizableTest& operator=(
      const FrameSizeButtonNonResizableTest&) = delete;

  ~FrameSizeButtonNonResizableTest() override {}
};

}  // namespace

TEST_F(FrameSizeButtonNonResizableTest, NoSnap) {
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_MINIMIZE, minimize_button()->GetIcon());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_CLOSE, close_button()->GetIcon());
  EXPECT_TRUE(AllButtonsInNormalState());

  // Pressing the size button should result in the size button being pressed and
  // the minimize and close button icons changing.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressLeftButton();
  EXPECT_EQ(views::Button::STATE_NORMAL, minimize_button()->GetState());
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->GetState());
  EXPECT_EQ(views::Button::STATE_NORMAL, close_button()->GetState());

  EXPECT_EQ(views::CAPTION_BUTTON_ICON_MINIMIZE, minimize_button()->GetIcon());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_CLOSE, close_button()->GetIcon());
}

// FrameSizeButtonPortraitDisplayTest is parameterized to run with and without
// the feature |features::kVerticalSnap|, which allows users to snap top and
// bottom in portrait layout, affecting snap icons.
class FrameSizeButtonPortraitDisplayTest
    : public FrameSizeButtonTest,
      public ::testing::WithParamInterface<bool> {
 public:
  FrameSizeButtonPortraitDisplayTest() = default;

  FrameSizeButtonPortraitDisplayTest(
      const FrameSizeButtonPortraitDisplayTest&) = delete;
  FrameSizeButtonPortraitDisplayTest& operator=(
      const FrameSizeButtonPortraitDisplayTest&) = delete;

  ~FrameSizeButtonPortraitDisplayTest() override = default;

  // FrameSizeButtonTest:
  void SetUp() override {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          chromeos::wm::features::kVerticalSnap);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          chromeos::wm::features::kVerticalSnap);
    }
    FrameSizeButtonTest::SetUp();
    UpdateDisplay("600x800");
  }

 protected:
  bool IsVerticalSnapEnabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that upon pressed the size button should show left and right arrows for
// horizontal snap and upward and downward arrow for vertical snap.
TEST_P(FrameSizeButtonPortraitDisplayTest, SnapButtons) {
  FrameCaptionButtonContainerView* container =
      widget_delegate()->caption_button_container();
  views::Widget* widget = widget_delegate()->GetWidget();
  chromeos::DefaultFrameHeader frame_header(
      widget, widget->non_client_view()->frame_view(), container);
  frame_header.LayoutHeader();

  EXPECT_EQ(views::CAPTION_BUTTON_ICON_MINIMIZE, minimize_button()->GetIcon());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_CLOSE, close_button()->GetIcon());
  EXPECT_TRUE(AllButtonsInNormalState());

  // Pressing the size button should result in the size button being pressed and
  // the minimize and close button icons changing.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressLeftButton();
  EXPECT_EQ(views::Button::STATE_NORMAL, minimize_button()->GetState());
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->GetState());
  EXPECT_EQ(views::Button::STATE_NORMAL, close_button()->GetState());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_LEFT_TOP_SNAPPED,
            minimize_button()->GetIcon());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_RIGHT_BOTTOM_SNAPPED,
            close_button()->GetIcon());

  const gfx::VectorIcon* left_icon =
      IsVerticalSnapEnabled() ? &chromeos::kWindowControlTopSnappedIcon
                              : &chromeos::kWindowControlLeftSnappedIcon;
  const gfx::VectorIcon* right_icon =
      IsVerticalSnapEnabled() ? &chromeos::kWindowControlBottomSnappedIcon
                              : &chromeos::kWindowControlRightSnappedIcon;

  EXPECT_TRUE(left_icon->name ==
              minimize_button()->icon_definition_for_test()->name);
  EXPECT_TRUE(right_icon->name ==
              close_button()->icon_definition_for_test()->name);

  // Dragging the mouse over the minimize button should hover the minimize
  // button (snap top/left). The minimize and close button icons should stay
  // changed.
  generator->MoveMouseTo(CenterPointInScreen(minimize_button()));
  EXPECT_EQ(views::Button::STATE_HOVERED, minimize_button()->GetState());
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->GetState());
  EXPECT_EQ(views::Button::STATE_NORMAL, close_button()->GetState());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_LEFT_TOP_SNAPPED,
            minimize_button()->GetIcon());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_RIGHT_BOTTOM_SNAPPED,
            close_button()->GetIcon());

  // Release the mouse, snapping the window to the primary position.
  generator->ReleaseLeftButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasStateType(WindowStateType::kPrimarySnapped));
}

INSTANTIATE_TEST_SUITE_P(All,
                         FrameSizeButtonPortraitDisplayTest,
                         ::testing::Bool());

}  // namespace ash
