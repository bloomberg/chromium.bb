// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/desktop_window_tree_host_mus.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "services/ws/test_ws/test_ws.mojom-test-utils.h"
#include "services/ws/test_ws/test_ws.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/transient_window_client.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/capture_synchronizer.h"
#include "ui/aura/mus/focus_synchronizer.h"
#include "ui/aura/mus/in_flight_change.h"
#include "ui/aura/mus/window_mus.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/mus/window_tree_client_test_observer.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/test/mus/change_completion_waiter.h"
#include "ui/aura/test/mus/test_window_tree.h"
#include "ui/aura/test/mus/window_tree_client_test_api.h"
#include "ui/aura/window.h"
#include "ui/display/display_switches.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/gestures/gesture_recognizer.h"
#include "ui/events/gestures/gesture_recognizer_observer.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/views/mus/mus_client.h"
#include "ui/views/mus/mus_client_test_api.h"
#include "ui/views/mus/screen_mus.h"
#include "ui/views/mus/window_manager_frame_values.h"
#include "ui/views/test/native_widget_factory.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/public/activation_client.h"

namespace views {

class DesktopWindowTreeHostMusTest : public ViewsTestBase,
                                     public WidgetObserver {
 public:
  DesktopWindowTreeHostMusTest() = default;
  ~DesktopWindowTreeHostMusTest() override = default;

  // ViewsTestBase:
  void SetUp() override {
    set_native_widget_type(NativeWidgetType::kDesktop);
    ViewsTestBase::SetUp();
  }

  // Creates a test widget. Takes ownership of |delegate|.
  std::unique_ptr<Widget> CreateWidget(WidgetDelegate* delegate = nullptr,
                                       aura::Window* parent = nullptr) {
    std::unique_ptr<Widget> widget = std::make_unique<Widget>();
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    params.delegate = delegate;
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(0, 1, 111, 123);
    params.parent = parent;
    widget->Init(params);
    widget->AddObserver(this);
    return widget;
  }

  const Widget* widget_activated() const { return widget_activated_; }
  const Widget* widget_deactivated() const { return widget_deactivated_; }

 private:
  void OnWidgetActivationChanged(Widget* widget, bool active) override {
    if (active) {
      widget_activated_ = widget;
    } else {
      if (widget_activated_ == widget)
        widget_activated_ = nullptr;
      widget_deactivated_ = widget;
    }
  }

  Widget* widget_activated_ = nullptr;
  Widget* widget_deactivated_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(DesktopWindowTreeHostMusTest);
};

class ExpectsNullCursorClientDuringTearDown : public aura::WindowObserver {
 public:
  explicit ExpectsNullCursorClientDuringTearDown(aura::Window* window)
      : window_(window) {
    window_->AddObserver(this);
  }

  ~ExpectsNullCursorClientDuringTearDown() override { EXPECT_FALSE(window_); }

 private:
  // aura::WindowObserver:
  void OnWindowRemovingFromRootWindow(aura::Window* window,
                                      aura::Window* new_root) override {
    aura::client::CursorClient* cursor_client =
        aura::client::GetCursorClient(window->GetRootWindow());
    EXPECT_FALSE(cursor_client);
    window_->RemoveObserver(this);
    window_ = nullptr;
  }

  aura::Window* window_;
  DISALLOW_COPY_AND_ASSIGN(ExpectsNullCursorClientDuringTearDown);
};

// Tests that the window service can set the initial show state for a window.
// https://crbug.com/899055
TEST_F(DesktopWindowTreeHostMusTest, ShowStateFromWindowService) {
  // Wait for the window created by
  // aura::TestScreen::CreateHostForPrimaryDisplay() lest it be the one that is
  // impacted by MaximizeNextWindow().
  aura::test::WaitForAllChangesToComplete();

  // Configure the window service to maximize the next top-level window.
  test_ws::mojom::TestWsPtr test_ws_ptr;
  MusClient::Get()->window_tree_client()->connector()->BindInterface(
      test_ws::mojom::kServiceName, &test_ws_ptr);
  test_ws::mojom::TestWsAsyncWaiter wait_for(test_ws_ptr.get());
  wait_for.MaximizeNextWindow();

  // Create a widget with the default show state.
  Widget widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(0, 0, 100, 100);
  EXPECT_EQ(ui::SHOW_STATE_DEFAULT, params.show_state);
  widget.Init(params);
  aura::test::WaitForAllChangesToComplete();

  // Window service provided the show state.
  EXPECT_TRUE(widget.IsMaximized());
}

TEST_F(DesktopWindowTreeHostMusTest, Visibility) {
  std::unique_ptr<Widget> widget(CreateWidget());
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_FALSE(widget->GetNativeView()->IsVisible());
  // It's important the parent is also hidden as this value is sent to the
  // server.
  EXPECT_FALSE(widget->GetNativeView()->parent()->IsVisible());
  widget->Show();
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_TRUE(widget->GetNativeView()->IsVisible());
  EXPECT_TRUE(widget->GetNativeView()->parent()->IsVisible());
  widget->Hide();
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_FALSE(widget->GetNativeView()->IsVisible());
  EXPECT_FALSE(widget->GetNativeView()->parent()->IsVisible());
}

TEST_F(DesktopWindowTreeHostMusTest, Capture) {
  std::unique_ptr<Widget> widget1(CreateWidget());
  widget1->Show();
  EXPECT_FALSE(widget1->HasCapture());

  std::unique_ptr<Widget> widget2(CreateWidget());
  widget2->Show();
  EXPECT_FALSE(widget2->HasCapture());

  widget1->SetCapture(widget1->GetRootView());
  EXPECT_TRUE(widget1->HasCapture());
  EXPECT_FALSE(widget2->HasCapture());
  EXPECT_EQ(widget1->GetNativeWindow(), MusClient::Get()
                                            ->window_tree_client()
                                            ->capture_synchronizer()
                                            ->capture_window()
                                            ->GetWindow());

  widget2->SetCapture(widget2->GetRootView());
  EXPECT_TRUE(widget2->HasCapture());
  EXPECT_EQ(widget2->GetNativeWindow(), MusClient::Get()
                                            ->window_tree_client()
                                            ->capture_synchronizer()
                                            ->capture_window()
                                            ->GetWindow());

  widget1->ReleaseCapture();
  EXPECT_TRUE(widget2->HasCapture());
  EXPECT_FALSE(widget1->HasCapture());
  EXPECT_EQ(widget2->GetNativeWindow(), MusClient::Get()
                                            ->window_tree_client()
                                            ->capture_synchronizer()
                                            ->capture_window()
                                            ->GetWindow());

  widget2->ReleaseCapture();
  EXPECT_FALSE(widget2->HasCapture());
  EXPECT_FALSE(widget1->HasCapture());
  EXPECT_EQ(nullptr, MusClient::Get()
                         ->window_tree_client()
                         ->capture_synchronizer()
                         ->capture_window());
}

TEST_F(DesktopWindowTreeHostMusTest, Deactivate) {
  std::unique_ptr<Widget> widget1(CreateWidget());
  widget1->Show();

  std::unique_ptr<Widget> widget2(CreateWidget());
  widget2->Show();

  widget1->Activate();
  EXPECT_TRUE(widget1->GetNativeWindow()->HasFocus());

  RunPendingMessages();
  EXPECT_TRUE(widget1->IsActive());
  EXPECT_TRUE(widget1->GetNativeWindow()->HasFocus());
  EXPECT_EQ(widget_activated(), widget1.get());

  widget1->Deactivate();
  EXPECT_FALSE(widget1->IsActive());
}

TEST_F(DesktopWindowTreeHostMusTest, HideWindowTreeHostWindowChangesActive) {
  std::unique_ptr<Widget> widget1(CreateWidget());
  widget1->Show();
  widget1->Activate();
  EXPECT_TRUE(widget1->IsActive());
  EXPECT_TRUE(widget1->GetNativeWindow()->HasFocus());
  ASSERT_TRUE(widget1->GetNativeWindow()->parent());

  // This simulates what happens when a hide happens from the server.
  widget1->GetNativeWindow()->GetHost()->Hide();
  EXPECT_FALSE(widget1->GetNativeWindow()->HasFocus());
  EXPECT_FALSE(widget1->IsActive());
}

TEST_F(DesktopWindowTreeHostMusTest, BecomesActiveOnMousePress) {
  std::unique_ptr<Widget> widget(CreateWidget());
  widget->ShowInactive();
  aura::test::WaitForAllChangesToComplete();

  EXPECT_FALSE(widget->IsActive());
  EXPECT_FALSE(widget->GetNativeWindow()->HasFocus());

  ui::MouseEvent mouse_event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
  ignore_result(
      widget->GetNativeWindow()->GetHost()->event_sink()->OnEventFromSource(
          &mouse_event));
  EXPECT_TRUE(widget->IsActive());
  EXPECT_TRUE(widget->GetNativeWindow()->HasFocus());
  aura::client::FocusClient* widget_focus_client =
      aura::client::GetFocusClient(widget->GetNativeWindow());
  EXPECT_EQ(widget_focus_client, MusClient::Get()
                                     ->window_tree_client()
                                     ->focus_synchronizer()
                                     ->active_focus_client());

  // The mouse event should generate a focus request to the server.
  EXPECT_TRUE(
      aura::WindowTreeClientTestApi(MusClient::Get()->window_tree_client())
          .HasChangeInFlightOfType(aura::ChangeType::FOCUS));
}

TEST_F(DesktopWindowTreeHostMusTest, ActivateBeforeShow) {
  std::unique_ptr<Widget> widget1(CreateWidget());
  // Activation can be attempted before visible.
  widget1->Activate();
  widget1->Show();
  widget1->Activate();
  EXPECT_TRUE(widget1->IsActive());
  // The Widget's NativeWindow (|DesktopNativeWidgetAura::content_window_|)
  // should be active.
  EXPECT_TRUE(widget1->GetNativeWindow()->HasFocus());
  // Env's active FocusClient should match the active window.
  aura::client::FocusClient* widget_focus_client =
      aura::client::GetFocusClient(widget1->GetNativeWindow());
  ASSERT_TRUE(widget_focus_client);
  EXPECT_EQ(widget_focus_client, MusClient::Get()
                                     ->window_tree_client()
                                     ->focus_synchronizer()
                                     ->active_focus_client());
}

// Tests that changes to kTopViewInset will cause the client area to be updated.
TEST_F(DesktopWindowTreeHostMusTest, ServerTopInsetChangeUpdatesClientArea) {
  std::unique_ptr<Widget> widget(CreateWidget());
  widget->Show();

  auto set_top_inset = [&widget](int value) {
    widget->GetNativeWindow()->GetRootWindow()->SetProperty(
        aura::client::kTopViewInset, value);
  };

  EXPECT_EQ(widget->GetRootView()->bounds(), widget->client_view()->bounds());

  set_top_inset(3);
  gfx::Rect root_bounds = widget->GetRootView()->bounds();
  root_bounds.Inset(gfx::Insets(3, 0, 0, 0));

  set_top_inset(0);
  EXPECT_EQ(widget->GetRootView()->bounds(), widget->client_view()->bounds());
}

TEST_F(DesktopWindowTreeHostMusTest, CursorClientDuringTearDown) {
  std::unique_ptr<Widget> widget(CreateWidget());
  widget->Show();

  std::unique_ptr<aura::Window> window(new aura::Window(nullptr));
  window->Init(ui::LAYER_SOLID_COLOR);
  ExpectsNullCursorClientDuringTearDown observer(window.get());

  widget->GetNativeWindow()->AddChild(window.release());
  widget.reset();
}

TEST_F(DesktopWindowTreeHostMusTest, StackAtTop) {
  std::unique_ptr<Widget> widget1(CreateWidget());
  widget1->Show();

  std::unique_ptr<Widget> widget2(CreateWidget());
  widget2->Show();

  aura::test::ChangeCompletionWaiter waiter(aura::ChangeType::REORDER, true);
  widget1->StackAtTop();
  waiter.Wait();

  // Other than the signal that our StackAtTop() succeeded, we don't have any
  // pieces of public data that we can check. If we actually stopped waiting,
  // count that as success.
}

TEST_F(DesktopWindowTreeHostMusTest, StackAtTopAlreadyOnTop) {
  std::unique_ptr<Widget> widget1(CreateWidget());
  widget1->Show();

  std::unique_ptr<Widget> widget2(CreateWidget());
  widget2->Show();

  aura::test::ChangeCompletionWaiter waiter(aura::ChangeType::REORDER, true);
  widget2->StackAtTop();
  waiter.Wait();
}

TEST_F(DesktopWindowTreeHostMusTest, StackAbove) {
  std::unique_ptr<Widget> widget1(CreateWidget(nullptr));
  widget1->Show();

  std::unique_ptr<Widget> widget2(CreateWidget(nullptr));
  widget2->Show();

  aura::test::ChangeCompletionWaiter waiter(aura::ChangeType::REORDER, true);
  widget1->StackAboveWidget(widget2.get());
  waiter.Wait();
}

TEST_F(DesktopWindowTreeHostMusTest, SetOpacity) {
  std::unique_ptr<Widget> widget1(CreateWidget(nullptr));
  widget1->Show();

  aura::test::ChangeCompletionWaiter waiter(aura::ChangeType::OPACITY, true);
  widget1->SetOpacity(0.5f);
  waiter.Wait();
}

TEST_F(DesktopWindowTreeHostMusTest, TransientParentWiredToHostWindow) {
  std::unique_ptr<Widget> widget1(CreateWidget());
  widget1->Show();

  std::unique_ptr<Widget> widget2(
      CreateWidget(nullptr, widget1->GetNativeView()));
  widget2->Show();

  aura::client::TransientWindowClient* transient_window_client =
      aura::client::GetTransientWindowClient();
  // Even though the widget1->GetNativeView() was specified as the parent we
  // expect the transient parents to be marked at the host level.
  EXPECT_EQ(widget1->GetNativeView()->GetHost()->window(),
            transient_window_client->GetTransientParent(
                widget2->GetNativeView()->GetHost()->window()));
}

TEST_F(DesktopWindowTreeHostMusTest, ShadowDefaults) {
  Widget widget;
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.Init(params);
  // |DesktopNativeWidgetAura::content_window_| should have no shadow; the wm
  // should provide it if it so desires.
  EXPECT_EQ(wm::kShadowElevationNone,
            widget.GetNativeView()->GetProperty(wm::kShadowElevationKey));
  // The wm honors the shadow property from the WindowTreeHost's window.
  EXPECT_EQ(wm::kShadowElevationDefault,
            widget.GetNativeView()->GetHost()->window()->GetProperty(
                wm::kShadowElevationKey));
}

TEST_F(DesktopWindowTreeHostMusTest, NoShadow) {
  Widget widget;
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.shadow_type = Widget::InitParams::SHADOW_TYPE_NONE;
  widget.Init(params);
  EXPECT_EQ(wm::kShadowElevationNone,
            widget.GetNativeView()->GetProperty(wm::kShadowElevationKey));
  EXPECT_EQ(wm::kShadowElevationNone,
            widget.GetNativeView()->GetHost()->window()->GetProperty(
                wm::kShadowElevationKey));
}

TEST_F(DesktopWindowTreeHostMusTest, CreateFullscreenWidget) {
  const Widget::InitParams::Type kWidgetTypes[] = {
      Widget::InitParams::TYPE_WINDOW,
      Widget::InitParams::TYPE_WINDOW_FRAMELESS,
  };

  for (auto widget_type : kWidgetTypes) {
    Widget widget;
    Widget::InitParams params = CreateParams(widget_type);
    params.show_state = ui::SHOW_STATE_FULLSCREEN;
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget.Init(params);

    EXPECT_TRUE(widget.IsFullscreen())
        << "Fullscreen creation failed for type=" << widget_type;
  }
}

TEST_F(DesktopWindowTreeHostMusTest, SynchronousBoundsWhenTogglingFullscreen) {
  const gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();

  const Widget::InitParams::Type kWidgetTypes[] = {
      Widget::InitParams::TYPE_WINDOW,
      Widget::InitParams::TYPE_WINDOW_FRAMELESS,
  };

  for (auto widget_type : kWidgetTypes) {
    Widget widget;
    Widget::InitParams params = CreateParams(widget_type);
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget.Init(params);

    const gfx::Rect restore_bounds = widget.GetWindowBoundsInScreen();

    // Entering fullscreen synchronously set show state and bounds.
    widget.SetFullscreen(true);
    EXPECT_TRUE(widget.IsFullscreen())
        << "Enter fullscreen failed for type=" << widget_type;
    EXPECT_EQ(display_bounds, widget.GetWindowBoundsInScreen());

    // Leaving fullscreen synchronously set show state and bounds.
    widget.SetFullscreen(false);
    EXPECT_FALSE(widget.IsFullscreen())
        << "Leave fullscreen failed for type=" << widget_type;
    EXPECT_EQ(restore_bounds, widget.GetWindowBoundsInScreen());
  }
}

TEST_F(DesktopWindowTreeHostMusTest, ClientWindowLayerDrawnSet) {
  struct {
    ui::LayerType layer_type;
    bool expected_layer_drawn;
  } kTestCases[] = {
      {ui::LayerType::LAYER_TEXTURED, true},
      {ui::LayerType::LAYER_SOLID_COLOR, true},
      {ui::LayerType::LAYER_NINE_PATCH, true},
      {ui::LayerType::LAYER_NOT_DRAWN, false},
  };

  for (const auto& test : kTestCases) {
    Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.layer_type = test.layer_type;

    Widget widget;
    widget.Init(params);
    EXPECT_EQ(test.expected_layer_drawn, widget.GetNativeWindow()->GetProperty(
                                             aura::client::kWindowLayerDrawn));
  }
}

// DesktopWindowTreeHostMusTest with --force-device-scale-factor=2.
class DesktopWindowTreeHostMusTestHighDPI
    : public DesktopWindowTreeHostMusTest {
 public:
  DesktopWindowTreeHostMusTestHighDPI() = default;
  ~DesktopWindowTreeHostMusTestHighDPI() override = default;

  // DesktopWindowTreeHostMusTest:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kForceDeviceScaleFactor, "2");
    DesktopWindowTreeHostMusTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DesktopWindowTreeHostMusTestHighDPI);
};

// Ensure menu widgets correctly scale initial bounds: http://crbug.com/899084
TEST_F(DesktopWindowTreeHostMusTestHighDPI, InitializeMenuWithDIPBounds) {
  // Swap the WindowTree implementation to verify SetWindowBounds() is called
  // with the correct DIP bounds, using the host's cached device_scale_factor.
  aura::TestWindowTree test_window_tree;
  aura::WindowTreeClientTestApi test_api(
      MusClient::Get()->window_tree_client());
  ws::mojom::WindowTree* old_tree = test_api.SwapTree(&test_window_tree);

  Widget widget;
  Widget::InitParams params(Widget::InitParams::TYPE_MENU);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(2, 4, 60, 80);
  widget.Init(params);

  // Check the second-last set window bounds (for the frame, not the content).
  EXPECT_EQ(params.bounds, test_window_tree.second_last_set_window_bounds());
  EXPECT_EQ(params.bounds, widget.GetWindowBoundsInScreen());
  EXPECT_EQ(2.0f, widget.GetNativeWindow()->GetHost()->device_scale_factor());
  gfx::Rect pixels(gfx::ConvertRectToPixel(2.0f, params.bounds));
  EXPECT_EQ(pixels, widget.GetNativeWindow()->GetHost()->GetBoundsInPixels());

  widget.CloseNow();
  test_api.SwapTree(old_tree);
}

TEST_F(DesktopWindowTreeHostMusTest, GetWindowBoundsInScreen) {
  // No ScreenMus in single process Mash.
  if (::features::IsSingleProcessMash())
    return;

  ScreenMus* screen = MusClientTestApi::screen();

  // Add a second display to the right of the primary.
  const int64_t kSecondDisplayId = 222;
  screen->display_list().AddDisplay(
      display::Display(kSecondDisplayId, gfx::Rect(800, 0, 640, 480)),
      display::DisplayList::Type::NOT_PRIMARY);

  // Verify bounds for a widget on the first display.
  Widget widget1;
  Widget::InitParams params1(Widget::InitParams::TYPE_WINDOW);
  params1.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params1.bounds = gfx::Rect(0, 0, 100, 100);
  widget1.Init(params1);
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), widget1.GetWindowBoundsInScreen());

  // Verify bounds for a widget on the secondary display.
  Widget widget2;
  Widget::InitParams params2(Widget::InitParams::TYPE_WINDOW);
  params2.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params2.bounds = gfx::Rect(800, 0, 100, 100);
  widget2.Init(params2);
  EXPECT_EQ(gfx::Rect(800, 0, 100, 100), widget2.GetWindowBoundsInScreen());
  EXPECT_EQ(kSecondDisplayId,
            aura::WindowTreeHostMus::ForWindow(widget2.GetNativeWindow())
                ->display_id());
}

// WidgetDelegate implementation that allows setting window-title and whether
// the title should be shown.
class WindowTitleWidgetDelegate : public WidgetDelegateView {
 public:
  WindowTitleWidgetDelegate() = default;
  ~WindowTitleWidgetDelegate() override = default;

  void set_window_title(const base::string16& title) { window_title_ = title; }
  void set_should_show_window_title(bool value) {
    should_show_window_title_ = value;
  }

  // WidgetDelegateView:
  base::string16 GetWindowTitle() const override { return window_title_; }
  bool ShouldShowWindowTitle() const override {
    return should_show_window_title_;
  }

 private:
  base::string16 window_title_;
  bool should_show_window_title_ = true;

  DISALLOW_COPY_AND_ASSIGN(WindowTitleWidgetDelegate);
};

TEST_F(DesktopWindowTreeHostMusTest, WindowTitle) {
  // Owned by |widget|.
  WindowTitleWidgetDelegate* delegate = new WindowTitleWidgetDelegate();
  std::unique_ptr<Widget> widget(CreateWidget(delegate));
  aura::Window* window = widget->GetNativeWindow()->GetRootWindow();

  // Set the title in the delegate and verify it propagates.
  const base::string16 title1 = base::ASCIIToUTF16("X");
  delegate->set_window_title(title1);
  widget->UpdateWindowTitle();
  EXPECT_TRUE(window->GetProperty(aura::client::kTitleShownKey));
  EXPECT_EQ(title1, window->GetTitle());

  // Hiding the title should not change the title.
  delegate->set_should_show_window_title(false);
  widget->UpdateWindowTitle();
  EXPECT_FALSE(window->GetProperty(aura::client::kTitleShownKey));
  EXPECT_EQ(title1, window->GetTitle());

  // Show the title again with a different value.
  delegate->set_should_show_window_title(true);
  const base::string16 title2 = base::ASCIIToUTF16("Z");
  delegate->set_window_title(title2);
  widget->UpdateWindowTitle();
  EXPECT_TRUE(window->GetProperty(aura::client::kTitleShownKey));
  EXPECT_EQ(title2, window->GetTitle());
}

TEST_F(DesktopWindowTreeHostMusTest,
       ClientViewBoundsChangeUpdatesServerClientArea) {
  std::unique_ptr<Widget> widget = CreateWidget();
  views::NonClientView* non_client_view = widget->non_client_view();
  ASSERT_TRUE(non_client_view);
  ASSERT_TRUE(non_client_view->client_view());

  // Calculate a new bounds. It doesn't matter what the bounds are, just as long
  // as they differ.
  gfx::Rect bounds = non_client_view->client_view()->bounds();
  bounds.set_width(bounds.width() - 1);
  bounds.set_height(bounds.height() - 1);

  // Swap the WindowTree implementation to verify SetClientArea() is called when
  // the bounds change.
  aura::TestWindowTree test_window_tree;
  aura::WindowTreeClientTestApi window_tree_client_private(
      MusClient::Get()->window_tree_client());
  ws::mojom::WindowTree* old_tree =
      window_tree_client_private.SwapTree(&test_window_tree);
  non_client_view->client_view()->SetBoundsRect(bounds);
  EXPECT_FALSE(test_window_tree.last_client_area().IsEmpty());
  window_tree_client_private.SwapTree(old_tree);
}

// Used to ensure the visibility of the root window is changed before that of
// the content window. This is necessary else close/hide animations end up
// animating a hidden (black) window.
class WidgetWindowVisibilityObserver : public aura::WindowObserver {
 public:
  explicit WidgetWindowVisibilityObserver(Widget* widget)
      : content_window_(widget->GetNativeWindow()),
        root_window_(content_window_->GetRootWindow()) {
    EXPECT_NE(content_window_, root_window_);
    content_window_->AddObserver(this);
    root_window_->AddObserver(this);
    EXPECT_TRUE(content_window_->IsVisible());
    EXPECT_TRUE(root_window_->IsVisible());
  }

  ~WidgetWindowVisibilityObserver() override {
    content_window_->RemoveObserver(this);
    root_window_->RemoveObserver(this);
  }

  bool got_content_window_hidden() const { return got_content_window_hidden_; }

  bool got_root_window_hidden() const { return got_root_window_hidden_; }

 private:
  // aura::WindowObserver:
  void OnWindowVisibilityChanging(aura::Window* window, bool visible) override {
    if (visible)
      return;

    if (!got_root_window_hidden_) {
      EXPECT_EQ(window, root_window_);
      got_root_window_hidden_ = true;
    } else if (!got_content_window_hidden_) {
      EXPECT_EQ(window, content_window_);
      got_content_window_hidden_ = true;
    }
  }

  aura::Window* content_window_;
  aura::Window* root_window_;

  // Set to true when |content_window_| is hidden. This is only checked after
  // the |root_window_| is hidden.
  bool got_content_window_hidden_ = false;

  // Set to true when |root_window_| is hidden.
  bool got_root_window_hidden_ = false;

  DISALLOW_COPY_AND_ASSIGN(WidgetWindowVisibilityObserver);
};

// See comments above WidgetWindowVisibilityObserver for details on what this
// verifies.
TEST_F(DesktopWindowTreeHostMusTest,
       HideChangesRootWindowVisibilityBeforeContentWindowVisibility) {
  std::unique_ptr<Widget> widget(CreateWidget());
  widget->Show();
  WidgetWindowVisibilityObserver observer(widget.get());
  widget->Close();
  EXPECT_TRUE(observer.got_content_window_hidden());
  EXPECT_TRUE(observer.got_root_window_hidden());
}

TEST_F(DesktopWindowTreeHostMusTest, MinimizeActivate) {
  std::unique_ptr<Widget> widget(CreateWidget());
  widget->Show();
  EXPECT_TRUE(widget->IsActive());

  widget->Minimize();
  aura::test::WaitForAllChangesToComplete();
  EXPECT_FALSE(widget->IsActive());
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_TRUE(widget->IsMinimized());

  // Activate() should restore the window.
  widget->Activate();
  EXPECT_TRUE(widget->IsActive());
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_FALSE(widget->IsMinimized());
  EXPECT_TRUE(widget->GetNativeWindow()->GetHost()->compositor()->IsVisible());
}

TEST_F(DesktopWindowTreeHostMusTest, MaximizeMinimizeRestore) {
  std::unique_ptr<Widget> widget(CreateWidget());
  widget->Show();
  EXPECT_TRUE(widget->IsActive());

  widget->Maximize();
  widget->Minimize();
  EXPECT_FALSE(widget->IsActive());
  EXPECT_TRUE(widget->IsMinimized());
  EXPECT_FALSE(widget->IsMaximized());

  widget->Restore();
  // Restore() *always* sets the state to normal, not the pre-minimized state.
  // This mirrors the logic in NativeWidgetAura. See
  // DesktopWindowTreeHostMus::RestoreToPreminimizedState() for details.
  EXPECT_FALSE(widget->IsMinimized());
  EXPECT_FALSE(widget->IsMaximized());
}

// Tests that toggling fullscreen synchronously updates kTopViewInset (via its
// effect on client bounds).
TEST_F(DesktopWindowTreeHostMusTest, FullscreenTopViewInset) {
  WindowManagerFrameValues frame_values;
  frame_values.normal_insets = gfx::Insets(7, 0, 0, 0);
  WindowManagerFrameValues::SetInstance(frame_values);

  std::unique_ptr<Widget> widget(CreateWidget());
  widget->Show();
  EXPECT_TRUE(widget->IsActive());

  gfx::Rect before_fullscreen_client_bounds = widget->client_view()->bounds();
  gfx::Rect frame_bounds = widget->non_client_view()->bounds();
  EXPECT_EQ(frame_values.normal_insets,
            frame_bounds.InsetsFrom(before_fullscreen_client_bounds));

  widget->SetFullscreen(true);

  gfx::Rect fullscreen_client_bounds = widget->client_view()->bounds();
  frame_bounds = widget->non_client_view()->bounds();
  EXPECT_EQ(fullscreen_client_bounds, frame_bounds);

  widget->SetFullscreen(false);

  gfx::Rect after_fullscreen_client_bounds = widget->client_view()->bounds();
  frame_bounds = widget->non_client_view()->bounds();
  EXPECT_EQ(frame_values.normal_insets,
            frame_bounds.InsetsFrom(after_fullscreen_client_bounds));

  EXPECT_EQ(before_fullscreen_client_bounds, after_fullscreen_client_bounds);
  EXPECT_NE(fullscreen_client_bounds, after_fullscreen_client_bounds);
}

// TransferTouchEventsCounter observes the GestureRecognizer and counts how many
// times TransferEventsTo() is invoked for testing.
class TransferTouchEventsCounter : public ui::GestureRecognizerObserver {
 public:
  TransferTouchEventsCounter() {
    aura::Env::GetInstance()->gesture_recognizer()->AddObserver(this);
  }
  ~TransferTouchEventsCounter() override {
    aura::Env::GetInstance()->gesture_recognizer()->RemoveObserver(this);
  }

  int GetTransferCount(ui::GestureConsumer* source,
                       ui::GestureConsumer* dest) const {
    return std::count(transfers_.begin(), transfers_.end(),
                      std::make_pair(source, dest));
  }

  int GetTotalCount() const { return transfers_.size(); }

 private:
  // ui::GestureRecognizerObserver:
  void OnActiveTouchesCanceledExcept(
      ui::GestureConsumer* not_cancelled) override {}
  void OnEventsTransferred(
      ui::GestureConsumer* current_consumer,
      ui::GestureConsumer* new_consumer,
      ui::TransferTouchesBehavior transfer_touches_behavior) override {
    transfers_.push_back(std::make_pair(current_consumer, new_consumer));
  }
  void OnActiveTouchesCanceled(ui::GestureConsumer* consumer) override {}

  std::vector<std::pair<ui::GestureConsumer*, ui::GestureConsumer*>> transfers_;

  DISALLOW_COPY_AND_ASSIGN(TransferTouchEventsCounter);
};

TEST_F(DesktopWindowTreeHostMusTest, WindowMoveTransfersTouchEvent) {
  std::unique_ptr<Widget> widget(CreateWidget());
  widget->Show();

  TransferTouchEventsCounter counter;
  aura::Window* window = widget->GetNativeWindow();
  aura::Window* root = window->GetRootWindow();

  auto runner = base::ThreadTaskRunnerHandle::Get();
  runner->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                     EXPECT_EQ(1, counter.GetTransferCount(window, root));
                     EXPECT_EQ(1, counter.GetTotalCount());
                   }));
  runner->PostTask(FROM_HERE, base::BindOnce(&Widget::EndMoveLoop,
                                             base::Unretained(widget.get())));

  widget->RunMoveLoop(gfx::Vector2d(), Widget::MOVE_LOOP_SOURCE_TOUCH,
                      Widget::MOVE_LOOP_ESCAPE_BEHAVIOR_DONT_HIDE);

  EXPECT_EQ(1, counter.GetTransferCount(root, window));
  EXPECT_EQ(2, counter.GetTotalCount());
}

TEST_F(DesktopWindowTreeHostMusTest, WindowMoveShouldNotTransfersBack) {
  std::unique_ptr<Widget> widget(CreateWidget());
  widget->Show();
  std::unique_ptr<Widget> widget2(CreateWidget());
  widget2->Show();

  TransferTouchEventsCounter counter;
  aura::Window* window = widget->GetNativeWindow();
  aura::Window* root = window->GetRootWindow();
  aura::Window* window2 = widget2->GetNativeWindow();

  auto runner = base::ThreadTaskRunnerHandle::Get();
  runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ui::GestureRecognizer::TransferEventsTo,
          base::Unretained(aura::Env::GetInstance()->gesture_recognizer()),
          root, window2, ui::TransferTouchesBehavior::kDontCancel));
  runner->PostTask(FROM_HERE, base::BindOnce(&Widget::EndMoveLoop,
                                             base::Unretained(widget.get())));

  widget->RunMoveLoop(gfx::Vector2d(), Widget::MOVE_LOOP_SOURCE_TOUCH,
                      Widget::MOVE_LOOP_ESCAPE_BEHAVIOR_DONT_HIDE);

  EXPECT_EQ(0, counter.GetTransferCount(root, window));
  EXPECT_EQ(1, counter.GetTransferCount(root, window2));
  EXPECT_EQ(2, counter.GetTotalCount());
}

TEST_F(DesktopWindowTreeHostMusTest, ShowWindowFromServerDoesntActivate) {
  std::unique_ptr<Widget> widget(CreateWidget());

  // This simulates what happens when a show happens from the server.
  widget->GetNativeWindow()->GetHost()->Show();
  EXPECT_TRUE(widget->IsVisible());
  // The window should not be active yet.
  EXPECT_FALSE(widget->GetNativeWindow()->HasFocus());
  EXPECT_FALSE(widget->IsActive());
}

// Used to track the number of times OnWidgetVisibilityChanged() is called.
class WidgetVisibilityObserver : public WidgetObserver {
 public:
  WidgetVisibilityObserver() = default;
  ~WidgetVisibilityObserver() override = default;

  int get_and_clear_change_count() {
    int result = change_count_;
    change_count_ = 0;
    return result;
  }

  // WidgetObserver:
  void OnWidgetVisibilityChanged(Widget* widget, bool visible) override {
    change_count_++;
  }

 private:
  int change_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(WidgetVisibilityObserver);
};

TEST_F(DesktopWindowTreeHostMusTest,
       TogglingVisibilityOfWindowTreeWindowTriggersWidgetNotification) {
  std::unique_ptr<Widget> widget(CreateWidget());
  widget->Show();

  WidgetVisibilityObserver observer;
  widget->AddObserver(&observer);

  widget->Show();
  EXPECT_EQ(0, observer.get_and_clear_change_count());
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_TRUE(widget->GetNativeWindow()->GetHost()->compositor()->IsVisible());
  EXPECT_TRUE(widget->GetNativeWindow()->GetRootWindow()->IsVisible());

  widget->Hide();
  EXPECT_EQ(1, observer.get_and_clear_change_count());
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_FALSE(widget->GetNativeWindow()->GetHost()->compositor()->IsVisible());
  EXPECT_FALSE(widget->GetNativeWindow()->GetRootWindow()->IsVisible());

  // Changing the visibility of the WindowTreeHost Window should notify the
  // observer.
  widget->GetNativeWindow()->GetHost()->window()->Show();
  EXPECT_EQ(1, observer.get_and_clear_change_count());
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_TRUE(widget->GetNativeWindow()->GetHost()->compositor()->IsVisible());
  EXPECT_TRUE(widget->GetNativeWindow()->GetRootWindow()->IsVisible());

  widget->GetNativeWindow()->GetHost()->window()->Hide();
  EXPECT_EQ(1, observer.get_and_clear_change_count());
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_FALSE(widget->GetNativeWindow()->GetHost()->compositor()->IsVisible());
  EXPECT_FALSE(widget->GetNativeWindow()->GetRootWindow()->IsVisible());

  widget->RemoveObserver(&observer);
}

TEST_F(DesktopWindowTreeHostMusTest, TransientChildMatchesParentVisibility) {
  std::unique_ptr<Widget> widget(CreateWidget());
  widget->Show();

  std::unique_ptr<Widget> transient_child =
      CreateWidget(nullptr, widget->GetNativeWindow());
  transient_child->Show();

  WidgetVisibilityObserver observer;
  transient_child->AddObserver(&observer);

  // Hiding the parent should also hide the transient child.
  widget->Hide();
  EXPECT_FALSE(transient_child->IsVisible());
  EXPECT_EQ(1, observer.get_and_clear_change_count());
  EXPECT_FALSE(
      transient_child->GetNativeWindow()->GetHost()->compositor()->IsVisible());
  EXPECT_FALSE(
      transient_child->GetNativeWindow()->GetRootWindow()->IsVisible());

  // set_parent_controls_visibility(true) makes it so showing the parent also
  // shows the child.
  wm::TransientWindowManager::GetOrCreate(
      transient_child->GetNativeWindow()->GetRootWindow())
      ->set_parent_controls_visibility(true);
  // With set_parent_controls_visibility() true, showing the parent should also
  // show the transient child.
  widget->Show();
  EXPECT_TRUE(transient_child->IsVisible());
  EXPECT_EQ(1, observer.get_and_clear_change_count());
  EXPECT_TRUE(
      transient_child->GetNativeWindow()->GetHost()->compositor()->IsVisible());
  EXPECT_TRUE(transient_child->GetNativeWindow()->GetRootWindow()->IsVisible());

  transient_child->RemoveObserver(&observer);
}

class StaticSizedWidgetDelegate : public WidgetDelegateView {
 public:
  StaticSizedWidgetDelegate(const gfx::Size& min_size,
                            const gfx::Size& max_size)
      : min_size_(min_size), max_size_(max_size) {}
  ~StaticSizedWidgetDelegate() override = default;

  void SetMinMaxSize(const gfx::Size& min_size, const gfx::Size& max_size) {
    min_size_ = min_size;
    max_size_ = max_size;
  }

 private:
  // View:
  gfx::Size GetMinimumSize() const override { return min_size_; }
  gfx::Size GetMaximumSize() const override { return max_size_; }

  gfx::Size min_size_;
  gfx::Size max_size_;

  DISALLOW_COPY_AND_ASSIGN(StaticSizedWidgetDelegate);
};

TEST_F(DesktopWindowTreeHostMusTest, MinMaxSize) {
  gfx::Size min_size(100, 100);
  gfx::Size max_size(200, 200);
  auto* delegate = new StaticSizedWidgetDelegate(min_size, max_size);
  std::unique_ptr<Widget> widget = CreateWidget(delegate);
  aura::Window* window = widget->GetNativeWindow()->GetRootWindow();

  // min/max sizes are not yet set.
  EXPECT_FALSE(window->GetProperty(aura::client::kMinimumSize));
  EXPECT_FALSE(window->GetProperty(aura::client::kMaximumSize));

  widget->Show();
  EXPECT_EQ(min_size, *window->GetProperty(aura::client::kMinimumSize));
  EXPECT_EQ(max_size, *window->GetProperty(aura::client::kMaximumSize));

  // Changing the min/max size isn't propagated immediately.
  gfx::Size min_size2(120, 130);
  gfx::Size max_size2(190, 180);
  delegate->SetMinMaxSize(min_size2, max_size2);
  EXPECT_EQ(min_size, *window->GetProperty(aura::client::kMinimumSize));
  EXPECT_EQ(max_size, *window->GetProperty(aura::client::kMaximumSize));

  // Propagated when the widget gets resized.
  widget->SetBounds(gfx::Rect(0, 0, 150, 150));
  EXPECT_EQ(min_size2, *window->GetProperty(aura::client::kMinimumSize));
  EXPECT_EQ(max_size2, *window->GetProperty(aura::client::kMaximumSize));

  delegate->SetMinMaxSize(min_size, max_size);
  // SizeConstraintsChanged should cause the update of min/max size.
  widget->OnSizeConstraintsChanged();
  EXPECT_EQ(min_size, *window->GetProperty(aura::client::kMinimumSize));
  EXPECT_EQ(max_size, *window->GetProperty(aura::client::kMaximumSize));

  // Re-show should propagate the information.
  delegate->SetMinMaxSize(min_size2, max_size2);
  widget->Hide();
  widget->Show();
  EXPECT_EQ(min_size2, *window->GetProperty(aura::client::kMinimumSize));
  EXPECT_EQ(max_size2, *window->GetProperty(aura::client::kMaximumSize));

  // If not changed, properties shouldn't be updated.
  gfx::Size* min_ptr = window->GetProperty(aura::client::kMinimumSize);
  gfx::Size* max_ptr = window->GetProperty(aura::client::kMaximumSize);
  widget->OnSizeConstraintsChanged();
  EXPECT_EQ(min_ptr, window->GetProperty(aura::client::kMinimumSize));
  EXPECT_EQ(max_ptr, window->GetProperty(aura::client::kMaximumSize));

  // If there are no limits, properties should be cleared.
  delegate->SetMinMaxSize(gfx::Size(), gfx::Size());
  widget->OnSizeConstraintsChanged();
  EXPECT_FALSE(window->GetProperty(aura::client::kMinimumSize));
  EXPECT_FALSE(window->GetProperty(aura::client::kMaximumSize));
}

TEST_F(DesktopWindowTreeHostMusTest, SetCanFocus) {
  auto* delegate = new WidgetDelegateView;
  std::unique_ptr<Widget> widget = CreateWidget(delegate);
  // Swap the WindowTree implementation to verify SetCanFocus() is called when
  // the active state changes.
  aura::TestWindowTree test_window_tree;
  aura::WindowTreeClientTestApi window_tree_client_private(
      MusClient::Get()->window_tree_client());
  ws::mojom::WindowTree* old_tree =
      window_tree_client_private.SwapTree(&test_window_tree);

  delegate->SetCanActivate(false);
  EXPECT_FALSE(delegate->CanActivate());
  EXPECT_FALSE(test_window_tree.last_can_focus());
  EXPECT_EQ(1u, test_window_tree.get_and_clear_can_focus_count());

  delegate->SetCanActivate(true);
  EXPECT_TRUE(delegate->CanActivate());
  EXPECT_TRUE(test_window_tree.last_can_focus());
  EXPECT_EQ(1u, test_window_tree.get_and_clear_can_focus_count());

  window_tree_client_private.SwapTree(old_tree);
}

// DesktopWindowTreeHostMusTest with --force-device-scale-factor=1.25.
class DesktopWindowTreeHostMusTestFractionalDPI
    : public DesktopWindowTreeHostMusTest {
 public:
  DesktopWindowTreeHostMusTestFractionalDPI() = default;
  ~DesktopWindowTreeHostMusTestFractionalDPI() override = default;

  // DesktopWindowTreeHostMusTest:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kForceDeviceScaleFactor, "1.25");
    DesktopWindowTreeHostMusTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DesktopWindowTreeHostMusTestFractionalDPI);
};

TEST_F(DesktopWindowTreeHostMusTestFractionalDPI,
       SetBoundsInDipWithFractionalScale) {
  std::unique_ptr<Widget> widget(CreateWidget());
  // These numbers are carefully chosen such that if enclosing rect is used
  // the pixel values differ between the two. The WindowServcie assumes ceiling
  // is used on the size, which is not impacted by the location.
  const gfx::Rect bounds1(408, 48, 339, 296);
  const int expected_pixel_height =
      gfx::ScaleToCeiledSize(bounds1.size(), 1.25f).height();
  widget->SetBounds(bounds1);
  EXPECT_EQ(expected_pixel_height,
            widget->GetNativeWindow()->GetHost()->GetBoundsInPixels().height());

  const gfx::Rect bounds2(gfx::Point(408, 49), bounds1.size());
  widget->SetBounds(bounds2);
  EXPECT_EQ(expected_pixel_height,
            widget->GetNativeWindow()->GetHost()->GetBoundsInPixels().height());
}

// DesktopWindowTreeHostMusTest with --force-device-scale-factor=1.6.
class DesktopWindowTreeHostMusTestFractionalDPI2
    : public DesktopWindowTreeHostMusTest {
 public:
  DesktopWindowTreeHostMusTestFractionalDPI2() = default;
  ~DesktopWindowTreeHostMusTestFractionalDPI2() override = default;

  // DesktopWindowTreeHostMusTest:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kForceDeviceScaleFactor, "1.6");
    DesktopWindowTreeHostMusTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DesktopWindowTreeHostMusTestFractionalDPI2);
};

TEST_F(DesktopWindowTreeHostMusTestFractionalDPI2,
       SetBoundsInDipWithFractionalScale) {
  std::unique_ptr<Widget> widget(CreateWidget());
  // These values have proven problematic at this scale.
  const gfx::Rect bounds(64, 34, 600, 372);
  widget->SetBounds(bounds);
  EXPECT_EQ(bounds, widget->GetWindowBoundsInScreen());
  EXPECT_EQ(bounds, widget->GetNativeWindow()->GetBoundsInScreen());
}

TEST_F(DesktopWindowTreeHostMusTest, ServerBoundsChangeIngoresMinMax) {
  gfx::Size min_size(100, 100);
  gfx::Size max_size(200, 200);
  auto* delegate = new StaticSizedWidgetDelegate(min_size, max_size);
  std::unique_ptr<Widget> widget = CreateWidget(delegate);
  widget->Show();

  // Setting the bounds to a size bigger than max should result in going to
  // max.
  widget->SetBounds(gfx::Rect(0, 0, 250, 250));
  EXPECT_EQ(gfx::Size(200, 200), widget->GetWindowBoundsInScreen().size());

  // Changes to the bounds from the server should not consider the min/max.
  const gfx::Rect server_bounds(1, 2, 250, 251);
  static_cast<aura::WindowTreeHostMus*>(widget->GetNativeWindow()->GetHost())
      ->SetBoundsFromServer(server_bounds, ui::SHOW_STATE_DEFAULT,
                            viz::LocalSurfaceIdAllocation());
  EXPECT_EQ(server_bounds, widget->GetWindowBoundsInScreen());
}

// Verify that focusing a child window makes the toplevel window active.
TEST_F(DesktopWindowTreeHostMusTest, DontActivateNonToplevelWindow) {
  std::unique_ptr<Widget> toplevel(CreateWidget());
  toplevel->Show();

  aura::Window* child = new aura::Window(nullptr);
  child->Init(ui::LAYER_SOLID_COLOR);
  toplevel->GetNativeView()->AddChild(child);
  ASSERT_TRUE(child->CanFocus());

  wm::ActivationClient* activation_client =
      wm::GetActivationClient(toplevel->GetNativeView()->GetRootWindow());

  // Focus |child| in normal state. |toplevel| is the active window and |child|
  // is focused.
  child->Focus();
  EXPECT_EQ(toplevel->GetNativeView(), activation_client->GetActiveWindow());
  EXPECT_TRUE(toplevel->IsActive());
  EXPECT_TRUE(child->HasFocus());

  // Minimize |toplevel|.
  toplevel->Minimize();
  ASSERT_TRUE(toplevel->IsMinimized());
  EXPECT_EQ(nullptr, activation_client->GetActiveWindow());
  EXPECT_FALSE(toplevel->IsActive());
  EXPECT_FALSE(child->HasFocus());

  // Focus |child| again. |toplevel| is the active window and |child| is
  // focused.
  child->Focus();
  EXPECT_EQ(toplevel->GetNativeView(), activation_client->GetActiveWindow());
  EXPECT_TRUE(toplevel->IsActive());
  EXPECT_TRUE(child->HasFocus());
}

}  // namespace views
