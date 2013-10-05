// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/tooltip_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/tooltip_client.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font.h"
#include "ui/gfx/point.h"
#include "ui/gfx/screen_type_delegate.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/corewm/tooltip_aura.h"
#include "ui/views/corewm/tooltip_controller_test_helper.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "ui/base/win/scoped_ole_initializer.h"
#endif
#if !defined(OS_CHROMEOS)
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#endif

namespace views {
namespace corewm {
namespace test {
namespace {

views::Widget* CreateWidget(aura::RootWindow* root) {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.accept_events = true;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
#if defined(OS_CHROMEOS)
  params.parent = root;
#else
  params.native_widget = new DesktopNativeWidgetAura(widget);
#endif
  params.bounds = gfx::Rect(0, 0, 200, 100);
  widget->Init(params);
  widget->Show();
  return widget;
}

TooltipController* GetController(Widget* widget) {
  return static_cast<TooltipController*>(
      aura::client::GetTooltipClient(
          widget->GetNativeWindow()->GetRootWindow()));
}

}  // namespace

class TooltipControllerTest : public aura::test::AuraTestBase {
 public:
  TooltipControllerTest() : view_(NULL) {}
  virtual ~TooltipControllerTest() {}

  virtual void SetUp() OVERRIDE {
    aura::test::AuraTestBase::SetUp();
#if defined(OS_CHROMEOS)
    controller_.reset(new TooltipController(
          scoped_ptr<views::corewm::Tooltip>(
              new views::corewm::TooltipAura(gfx::SCREEN_TYPE_ALTERNATE))));
    root_window()->AddPreTargetHandler(controller_.get());
    SetTooltipClient(root_window(), controller_.get());
#endif
    widget_.reset(CreateWidget(root_window()));
    widget_->SetContentsView(new View);
    view_ = new TooltipTestView;
    widget_->GetContentsView()->AddChildView(view_);
    view_->SetBoundsRect(widget_->GetContentsView()->GetLocalBounds());
    helper_.reset(new TooltipControllerTestHelper(
                      GetController(widget_.get())));
    generator_.reset(new aura::test::EventGenerator(GetRootWindow()));
  }

  virtual void TearDown() OVERRIDE {
#if defined(OS_CHROMEOS)
    root_window()->RemovePreTargetHandler(controller_.get());
    SetTooltipClient(root_window(), NULL);
    controller_.reset();
#endif
    generator_.reset();
    helper_.reset();
    widget_.reset();
    aura::test::AuraTestBase::TearDown();
  }

 protected:
  aura::Window* GetWindow() {
    return widget_->GetNativeWindow();
  }

  aura::RootWindow* GetRootWindow() {
    return GetWindow()->GetRootWindow();
  }

  TooltipTestView* PrepareSecondView() {
    TooltipTestView* view2 = new TooltipTestView;
    widget_->GetContentsView()->AddChildView(view2);
    view_->SetBounds(0, 0, 100, 100);
    view2->SetBounds(100, 0, 100, 100);
    return view2;
  }

  scoped_ptr<views::Widget> widget_;
  TooltipTestView* view_;
  scoped_ptr<TooltipControllerTestHelper> helper_;
  scoped_ptr<aura::test::EventGenerator> generator_;

 private:
  scoped_ptr<TooltipController> controller_;
#if defined(OS_WIN)
  ui::ScopedOleInitializer ole_initializer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(TooltipControllerTest);
};

TEST_F(TooltipControllerTest, ViewTooltip) {
  view_->set_tooltip_text(ASCIIToUTF16("Tooltip Text"));
  EXPECT_EQ(string16(), helper_->GetTooltipText());
  EXPECT_EQ(NULL, helper_->GetTooltipWindow());
  generator_->MoveMouseToCenterOf(GetWindow());

  EXPECT_EQ(GetWindow(), GetRootWindow()->GetEventHandlerForPoint(
      generator_->current_location()));
  string16 expected_tooltip = ASCIIToUTF16("Tooltip Text");
  EXPECT_EQ(expected_tooltip, aura::client::GetTooltipText(GetWindow()));
  EXPECT_EQ(string16(), helper_->GetTooltipText());
  EXPECT_EQ(GetWindow(), helper_->GetTooltipWindow());

  // Fire tooltip timer so tooltip becomes visible.
  helper_->FireTooltipTimer();

  EXPECT_TRUE(helper_->IsTooltipVisible());
  generator_->MoveMouseBy(1, 0);

  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_EQ(expected_tooltip, aura::client::GetTooltipText(GetWindow()));
  EXPECT_EQ(expected_tooltip, helper_->GetTooltipText());
  EXPECT_EQ(GetWindow(), helper_->GetTooltipWindow());
}

TEST_F(TooltipControllerTest, TooltipsInMultipleViews) {
  view_->set_tooltip_text(ASCIIToUTF16("Tooltip Text"));
  EXPECT_EQ(string16(), helper_->GetTooltipText());
  EXPECT_EQ(NULL, helper_->GetTooltipWindow());

  PrepareSecondView();
  aura::Window* window = GetWindow();
  aura::RootWindow* root_window = GetRootWindow();

  // Fire tooltip timer so tooltip becomes visible.
  generator_->MoveMouseRelativeTo(window, view_->bounds().CenterPoint());
  helper_->FireTooltipTimer();
  EXPECT_TRUE(helper_->IsTooltipVisible());
  for (int i = 0; i < 49; ++i) {
    generator_->MoveMouseBy(1, 0);
    EXPECT_TRUE(helper_->IsTooltipVisible());
    EXPECT_EQ(window, root_window->GetEventHandlerForPoint(
            generator_->current_location()));
    string16 expected_tooltip = ASCIIToUTF16("Tooltip Text");
    EXPECT_EQ(expected_tooltip, aura::client::GetTooltipText(window));
    EXPECT_EQ(expected_tooltip, helper_->GetTooltipText());
    EXPECT_EQ(window, helper_->GetTooltipWindow());
  }
  for (int i = 0; i < 49; ++i) {
    generator_->MoveMouseBy(1, 0);
    EXPECT_FALSE(helper_->IsTooltipVisible());
    EXPECT_EQ(window, root_window->GetEventHandlerForPoint(
            generator_->current_location()));
    string16 expected_tooltip;  // = ""
    EXPECT_EQ(expected_tooltip, aura::client::GetTooltipText(window));
    EXPECT_EQ(expected_tooltip, helper_->GetTooltipText());
    EXPECT_EQ(window, helper_->GetTooltipWindow());
  }
}

TEST_F(TooltipControllerTest, EnableOrDisableTooltips) {
  view_->set_tooltip_text(ASCIIToUTF16("Tooltip Text"));
  EXPECT_EQ(string16(), helper_->GetTooltipText());
  EXPECT_EQ(NULL, helper_->GetTooltipWindow());

  generator_->MoveMouseRelativeTo(GetWindow(), view_->bounds().CenterPoint());
  string16 expected_tooltip = ASCIIToUTF16("Tooltip Text");

  // Fire tooltip timer so tooltip becomes visible.
  helper_->FireTooltipTimer();
  EXPECT_TRUE(helper_->IsTooltipVisible());

  // Diable tooltips and check again.
  helper_->controller()->SetTooltipsEnabled(false);
  EXPECT_FALSE(helper_->IsTooltipVisible());
  helper_->FireTooltipTimer();
  EXPECT_FALSE(helper_->IsTooltipVisible());

  // Enable tooltips back and check again.
  helper_->controller()->SetTooltipsEnabled(true);
  EXPECT_FALSE(helper_->IsTooltipVisible());
  helper_->FireTooltipTimer();
  EXPECT_TRUE(helper_->IsTooltipVisible());
}

TEST_F(TooltipControllerTest, TooltipHidesOnKeyPressAndStaysHiddenUntilChange) {
  view_->set_tooltip_text(ASCIIToUTF16("Tooltip Text for view 1"));
  EXPECT_EQ(string16(), helper_->GetTooltipText());
  EXPECT_EQ(NULL, helper_->GetTooltipWindow());

  TooltipTestView* view2 = PrepareSecondView();
  view2->set_tooltip_text(ASCIIToUTF16("Tooltip Text for view 2"));

  aura::Window* window = GetWindow();

  // Fire tooltip timer so tooltip becomes visible.
  generator_->MoveMouseRelativeTo(window, view_->bounds().CenterPoint());
  helper_->FireTooltipTimer();
  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_TRUE(helper_->IsTooltipShownTimerRunning());

  generator_->PressKey(ui::VKEY_1, 0);
  EXPECT_FALSE(helper_->IsTooltipVisible());
  EXPECT_FALSE(helper_->IsTooltipTimerRunning());
  EXPECT_FALSE(helper_->IsTooltipShownTimerRunning());

  // Moving the mouse inside |view1| should not change the state of the tooltip
  // or the timers.
  for (int i = 0; i < 49; i++) {
    generator_->MoveMouseBy(1, 0);
    EXPECT_FALSE(helper_->IsTooltipVisible());
    EXPECT_FALSE(helper_->IsTooltipTimerRunning());
    EXPECT_FALSE(helper_->IsTooltipShownTimerRunning());
    EXPECT_EQ(window,
              GetRootWindow()->GetEventHandlerForPoint(
                  generator_->current_location()));
    string16 expected_tooltip = ASCIIToUTF16("Tooltip Text for view 1");
    EXPECT_EQ(expected_tooltip, aura::client::GetTooltipText(window));
    EXPECT_EQ(expected_tooltip, helper_->GetTooltipText());
    EXPECT_EQ(window, helper_->GetTooltipWindow());
  }

  // Now we move the mouse on to |view2|. It should re-start the tooltip timer.
  generator_->MoveMouseBy(1, 0);
  EXPECT_TRUE(helper_->IsTooltipTimerRunning());
  helper_->FireTooltipTimer();
  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_TRUE(helper_->IsTooltipShownTimerRunning());
  string16 expected_tooltip = ASCIIToUTF16("Tooltip Text for view 2");
  EXPECT_EQ(expected_tooltip, aura::client::GetTooltipText(window));
  EXPECT_EQ(expected_tooltip, helper_->GetTooltipText());
  EXPECT_EQ(window, helper_->GetTooltipWindow());
}

TEST_F(TooltipControllerTest, TooltipHidesOnTimeoutAndStaysHiddenUntilChange) {
  view_->set_tooltip_text(ASCIIToUTF16("Tooltip Text for view 1"));
  EXPECT_EQ(string16(), helper_->GetTooltipText());
  EXPECT_EQ(NULL, helper_->GetTooltipWindow());

  TooltipTestView* view2 = PrepareSecondView();
  view2->set_tooltip_text(ASCIIToUTF16("Tooltip Text for view 2"));

  aura::Window* window = GetWindow();

  // Fire tooltip timer so tooltip becomes visible.
  generator_->MoveMouseRelativeTo(window, view_->bounds().CenterPoint());
  helper_->FireTooltipTimer();
  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_TRUE(helper_->IsTooltipShownTimerRunning());

  helper_->FireTooltipShownTimer();
  EXPECT_FALSE(helper_->IsTooltipVisible());
  EXPECT_FALSE(helper_->IsTooltipTimerRunning());
  EXPECT_FALSE(helper_->IsTooltipShownTimerRunning());

  // Moving the mouse inside |view1| should not change the state of the tooltip
  // or the timers.
  for (int i = 0; i < 49; ++i) {
    generator_->MoveMouseBy(1, 0);
    EXPECT_FALSE(helper_->IsTooltipVisible());
    EXPECT_FALSE(helper_->IsTooltipTimerRunning());
    EXPECT_FALSE(helper_->IsTooltipShownTimerRunning());
    EXPECT_EQ(window, GetRootWindow()->GetEventHandlerForPoint(
                  generator_->current_location()));
    string16 expected_tooltip = ASCIIToUTF16("Tooltip Text for view 1");
    EXPECT_EQ(expected_tooltip, aura::client::GetTooltipText(window));
    EXPECT_EQ(expected_tooltip, helper_->GetTooltipText());
    EXPECT_EQ(window, helper_->GetTooltipWindow());
  }

  // Now we move the mouse on to |view2|. It should re-start the tooltip timer.
  generator_->MoveMouseBy(1, 0);
  EXPECT_TRUE(helper_->IsTooltipTimerRunning());
  helper_->FireTooltipTimer();
  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_TRUE(helper_->IsTooltipShownTimerRunning());
  string16 expected_tooltip = ASCIIToUTF16("Tooltip Text for view 2");
  EXPECT_EQ(expected_tooltip, aura::client::GetTooltipText(window));
  EXPECT_EQ(expected_tooltip, helper_->GetTooltipText());
  EXPECT_EQ(window, helper_->GetTooltipWindow());
}

// Verifies a mouse exit event hides the tooltips.
TEST_F(TooltipControllerTest, HideOnExit) {
  view_->set_tooltip_text(ASCIIToUTF16("Tooltip Text"));
  generator_->MoveMouseToCenterOf(GetWindow());
  string16 expected_tooltip = ASCIIToUTF16("Tooltip Text");
  EXPECT_EQ(expected_tooltip, aura::client::GetTooltipText(GetWindow()));
  EXPECT_EQ(string16(), helper_->GetTooltipText());
  EXPECT_EQ(GetWindow(), helper_->GetTooltipWindow());

  // Fire tooltip timer so tooltip becomes visible.
  helper_->FireTooltipTimer();

  EXPECT_TRUE(helper_->IsTooltipVisible());
  generator_->SendMouseExit();
  EXPECT_FALSE(helper_->IsTooltipVisible());
}

#if !defined(OS_CHROMEOS)
// This test creates two top level windows and verifies that the tooltip
// displays correctly when mouse moves are dispatched to these windows.
// Additionally it also verifies that the tooltip is reparented to the new
// window when mouse moves are dispatched to it.
TEST_F(TooltipControllerTest, TooltipsInMultipleRootWindows) {
  view_->set_tooltip_text(ASCIIToUTF16("Tooltip Text For RootWindow1"));
  EXPECT_EQ(string16(), helper_->GetTooltipText());
  EXPECT_EQ(NULL, helper_->GetTooltipWindow());

  aura::Window* window = GetWindow();
  aura::RootWindow* root_window = GetRootWindow();

  // Fire tooltip timer so tooltip becomes visible.
  generator_->MoveMouseRelativeTo(window, view_->bounds().CenterPoint());
  helper_->FireTooltipTimer();
  EXPECT_TRUE(helper_->IsTooltipVisible());
  for (int i = 0; i < 49; ++i) {
    generator_->MoveMouseBy(1, 0);
    EXPECT_TRUE(helper_->IsTooltipVisible());
    EXPECT_EQ(window, root_window->GetEventHandlerForPoint(
            generator_->current_location()));
    string16 expected_tooltip =
        ASCIIToUTF16("Tooltip Text For RootWindow1");
    EXPECT_EQ(expected_tooltip, aura::client::GetTooltipText(window));
    EXPECT_EQ(expected_tooltip, helper_->GetTooltipText());
    EXPECT_EQ(window, helper_->GetTooltipWindow());
  }

  views::Widget* widget2 = CreateWidget(NULL);
  widget2->SetContentsView(new View);
  TooltipTestView* view2 = new TooltipTestView;
  widget2->GetContentsView()->AddChildView(view2);
  view2->SetBoundsRect(widget2->GetContentsView()->GetLocalBounds());
  helper_.reset(new TooltipControllerTestHelper(
                    GetController(widget2)));
  generator_.reset(new aura::test::EventGenerator(
      widget2->GetNativeWindow()->GetRootWindow()));
    view2->set_tooltip_text(ASCIIToUTF16("Tooltip Text For RootWindow2"));

  aura::Window* window2 = widget2->GetNativeWindow();
  aura::RootWindow* root_window2 =
      widget2->GetNativeWindow()->GetRootWindow();
  // Fire tooltip timer so tooltip becomes visible.
  generator_->MoveMouseRelativeTo(window2, view2->bounds().CenterPoint());
  helper_->FireTooltipTimer();

  EXPECT_NE(root_window, root_window2);
  EXPECT_NE(window, window2);

  for (int i = 0; i < 49; ++i) {
    generator_->MoveMouseBy(1, 0);
    EXPECT_TRUE(helper_->IsTooltipVisible());
    EXPECT_EQ(window2, root_window2->GetEventHandlerForPoint(
              generator_->current_location()));
    string16 expected_tooltip = ASCIIToUTF16("Tooltip Text For RootWindow2");
    EXPECT_EQ(expected_tooltip, aura::client::GetTooltipText(window2));
    EXPECT_EQ(expected_tooltip, helper_->GetTooltipText());
    EXPECT_EQ(window2, helper_->GetTooltipWindow());
  }

  bool tooltip_reparented = false;
  for (size_t i = 0; i < root_window2->children().size(); ++i) {
    if (root_window2->children()[i]->type() ==
        aura::client::WINDOW_TYPE_TOOLTIP) {
      tooltip_reparented = true;
      break;
    }
  }
  EXPECT_TRUE(tooltip_reparented);
  widget2->Close();
}

// This test validates whether the tooltip after becoming visible stays at the
// top of the ZOrder in its root window after activation changes.
TEST_F(TooltipControllerTest, TooltipAtTopOfZOrderAfterActivation) {
  view_->set_tooltip_text(ASCIIToUTF16("Tooltip Text"));
  EXPECT_EQ(string16(), helper_->GetTooltipText());
  EXPECT_EQ(NULL, helper_->GetTooltipWindow());
  generator_->MoveMouseToCenterOf(GetWindow());

  EXPECT_EQ(GetWindow(), GetRootWindow()->GetEventHandlerForPoint(
      generator_->current_location()));
  string16 expected_tooltip = ASCIIToUTF16("Tooltip Text");
  EXPECT_EQ(expected_tooltip, aura::client::GetTooltipText(GetWindow()));
  EXPECT_EQ(string16(), helper_->GetTooltipText());
  EXPECT_EQ(GetWindow(), helper_->GetTooltipWindow());

  // Fire tooltip timer so tooltip becomes visible.
  helper_->FireTooltipTimer();

  EXPECT_TRUE(helper_->IsTooltipVisible());
  generator_->MoveMouseBy(1, 0);

  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_EQ(expected_tooltip, aura::client::GetTooltipText(GetWindow()));
  EXPECT_EQ(expected_tooltip, helper_->GetTooltipText());
  EXPECT_EQ(GetWindow(), helper_->GetTooltipWindow());

  // Fake activation loss and gain in the native widget. This should cause a
  // ZOrder change which should not affect the position of the tooltip.
  DesktopNativeWidgetAura* native_widget =
      static_cast<DesktopNativeWidgetAura*>(widget_->native_widget());
  EXPECT_TRUE(native_widget != NULL);

  native_widget->HandleActivationChanged(false);
  native_widget->HandleActivationChanged(true);

  EXPECT_EQ(
      widget_->GetNativeWindow()->GetRootWindow()->children().back()->type(),
      aura::client::WINDOW_TYPE_TOOLTIP);
}

#endif

}  // namespace test
}  // namespace corewm
}  // namespace views
