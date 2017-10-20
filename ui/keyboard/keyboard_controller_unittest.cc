// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/keyboard_controller.h"

#include <memory>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/test/aura_test_helper.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_factory.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer_type.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/context_factories_for_test.h"
#include "ui/compositor/test/layer_animator_test_controller.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/keyboard/container_full_width_behavior.h"
#include "ui/keyboard/keyboard_controller_observer.h"
#include "ui/keyboard/keyboard_test_util.h"
#include "ui/keyboard/keyboard_ui.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/wm/core/default_activation_client.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace keyboard {
namespace {

const int kDefaultVirtualKeyboardHeight = 100;

// Verify if the |keyboard| window covers the |container| window completely.
void VerifyKeyboardWindowSize(aura::Window* container, aura::Window* keyboard) {
  ASSERT_EQ(gfx::Rect(0, 0, container->bounds().width(),
                      container->bounds().height()),
            keyboard->bounds());
}

// Steps a layer animation until it is completed. Animations must be enabled.
void RunAnimationForLayer(ui::Layer* layer) {
  // Animations must be enabled for stepping to work.
  ASSERT_NE(ui::ScopedAnimationDurationScaleMode::duration_scale_mode(),
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  ui::LayerAnimatorTestController controller(layer->GetAnimator());
  // Multiple steps are required to complete complex animations.
  // TODO(vollick): This should not be necessary. crbug.com/154017
  while (controller.animator()->is_animating()) {
    controller.StartThreadedAnimationsIfNeeded();
    base::TimeTicks step_time = controller.animator()->last_step_time();
    controller.animator()->Step(step_time +
                                base::TimeDelta::FromMilliseconds(1000));
  }
}

class ScopedTouchKeyboardEnabler {
 public:
  ScopedTouchKeyboardEnabler() : enabled_(keyboard::GetTouchKeyboardEnabled()) {
    keyboard::SetTouchKeyboardEnabled(true);
  }

  ~ScopedTouchKeyboardEnabler() { keyboard::SetTouchKeyboardEnabled(enabled_); }

 private:
  const bool enabled_;
};

class ScopedAccessibilityKeyboardEnabler {
 public:
  ScopedAccessibilityKeyboardEnabler()
      : enabled_(keyboard::GetAccessibilityKeyboardEnabled()) {
    keyboard::SetAccessibilityKeyboardEnabled(true);
  }

  ~ScopedAccessibilityKeyboardEnabler() {
    keyboard::SetAccessibilityKeyboardEnabled(enabled_);
  }

 private:
  const bool enabled_;
};

// An event handler that focuses a window when it is clicked/touched on. This is
// used to match the focus manger behaviour in ash and views.
class TestFocusController : public ui::EventHandler {
 public:
  explicit TestFocusController(aura::Window* root)
      : root_(root) {
    root_->AddPreTargetHandler(this);
  }

  ~TestFocusController() override { root_->RemovePreTargetHandler(this); }

 private:
  // Overridden from ui::EventHandler:
  void OnEvent(ui::Event* event) override {
    aura::Window* target = static_cast<aura::Window*>(event->target());
    if (event->type() == ui::ET_MOUSE_PRESSED ||
        event->type() == ui::ET_TOUCH_PRESSED) {
      aura::client::GetFocusClient(target)->FocusWindow(target);
    }
  }

  aura::Window* root_;
  DISALLOW_COPY_AND_ASSIGN(TestFocusController);
};

// Keeps a count of all the events a window receives.
class EventObserver : public ui::EventHandler {
 public:
  EventObserver() {}
  ~EventObserver() override {}

  int GetEventCount(ui::EventType type) {
    return event_counts_[type];
  }

 private:
  // Overridden from ui::EventHandler:
  void OnEvent(ui::Event* event) override {
    ui::EventHandler::OnEvent(event);
    event_counts_[event->type()]++;
  }

  std::map<ui::EventType, int> event_counts_;
  DISALLOW_COPY_AND_ASSIGN(EventObserver);
};

class KeyboardContainerObserver : public aura::WindowObserver {
 public:
  explicit KeyboardContainerObserver(aura::Window* window,
                                     base::RunLoop* run_loop)
      : window_(window), run_loop_(run_loop) {
    window_->AddObserver(this);
  }
  ~KeyboardContainerObserver() override { window_->RemoveObserver(this); }

 private:
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override {
    if (!visible)
      run_loop_->QuitWhenIdle();
  }

  aura::Window* window_;
  base::RunLoop* const run_loop_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardContainerObserver);
};

class TestKeyboardLayoutDelegate : public KeyboardLayoutDelegate {
 public:
  TestKeyboardLayoutDelegate() {}
  ~TestKeyboardLayoutDelegate() override {}

  // Overridden from keyboard::KeyboardLayoutDelegate
  void MoveKeyboardToDisplay(int64_t /* display_id */) override {}
  void MoveKeyboardToTouchableDisplay() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestKeyboardLayoutDelegate);
};

}  // namespace

class KeyboardControllerTest : public testing::Test,
                               public KeyboardControllerObserver {
 public:
  KeyboardControllerTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI),
        number_of_calls_(0),
        keyboard_closed_(false) {}
  ~KeyboardControllerTest() override {}

  void SetUp() override {
    // The ContextFactory must exist before any Compositors are created.
    bool enable_pixel_output = false;
    ui::ContextFactory* context_factory = nullptr;
    ui::ContextFactoryPrivate* context_factory_private = nullptr;

    ui::InitializeContextFactoryForTests(enable_pixel_output, &context_factory,
                                         &context_factory_private);

    ui::SetUpInputMethodFactoryForTesting();
    aura_test_helper_.reset(new aura::test::AuraTestHelper());
    aura_test_helper_->SetUp(context_factory, context_factory_private);
    new wm::DefaultActivationClient(aura_test_helper_->root_window());
    focus_controller_.reset(new TestFocusController(root_window()));
    layout_delegate_.reset(new TestKeyboardLayoutDelegate());
    controller_.reset(
        new KeyboardController(std::make_unique<TestKeyboardUI>(
                                   aura_test_helper_->host()->GetInputMethod()),
                               layout_delegate_.get()));
    controller()->AddObserver(this);
  }

  void TearDown() override {
    if (controller())
      controller()->RemoveObserver(this);
    controller_.reset();
    focus_controller_.reset();
    aura_test_helper_->TearDown();
    ui::TerminateContextFactoryForTests();
  }

  aura::Window* root_window() { return aura_test_helper_->root_window(); }
  KeyboardUI* ui() { return controller_->ui(); }
  KeyboardController* controller() { return controller_.get(); }

  void ShowKeyboard() {
    test_text_input_client_.reset(
        new ui::DummyTextInputClient(ui::TEXT_INPUT_TYPE_TEXT));
    SetFocus(test_text_input_client_.get());
  }

  void MockRotateScreen() {
    const gfx::Rect root_bounds = root_window()->bounds();
    root_window()->SetBounds(
        gfx::Rect(0, 0, root_bounds.height(), root_bounds.width()));
  }

 protected:
  // KeyboardControllerObserver overrides
  void OnKeyboardBoundsChanging(const gfx::Rect& new_bounds) override {
    notified_bounds_ = new_bounds;
    number_of_calls_++;
  }
  void OnKeyboardClosed() override { keyboard_closed_ = true; }

  int number_of_calls() const { return number_of_calls_; }

  const gfx::Rect& notified_bounds() { return notified_bounds_; }

  bool IsKeyboardClosed() { return keyboard_closed_; }

  void SetFocus(ui::TextInputClient* client) {
    ui::InputMethod* input_method = ui()->GetInputMethod();
    input_method->SetFocusedTextInputClient(client);
    if (client && client->GetTextInputType() != ui::TEXT_INPUT_TYPE_NONE) {
      input_method->ShowImeIfNeeded();
      if (controller_->ui()->GetContentsWindow()->bounds().height() == 0) {
        // Set initial bounds for test keyboard window.
        controller_->ui()->GetContentsWindow()->SetBounds(
            KeyboardBoundsFromRootBounds(root_window()->bounds(),
                                         kDefaultVirtualKeyboardHeight));
      }
    }
  }

  bool WillHideKeyboard() {
    return controller_->WillHideKeyboard();
  }

  bool ShouldEnableInsets(aura::Window* window) {
    aura::Window* contents_window = controller_->ui()->GetContentsWindow();
    return (contents_window->GetRootWindow() == window->GetRootWindow() &&
            keyboard::IsKeyboardOverscrollEnabled() &&
            contents_window->IsVisible() && controller_->keyboard_visible());
  }

  void ResetController() { controller_.reset(); }

  void RunLoop(base::RunLoop* run_loop) {
#if defined(USE_OZONE)
    // TODO(crbug/776357): Figure out why the initializer randomly doesn't run
    // for some tests. In the mean time, prevent flaky Ozone crash.
    ui::OzonePlatform::InitializeForGPU(ui::OzonePlatform::InitParams());
#endif
    run_loop->Run();
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<aura::test::AuraTestHelper> aura_test_helper_;
  std::unique_ptr<TestFocusController> focus_controller_;

 private:
  int number_of_calls_;
  gfx::Rect notified_bounds_;
  std::unique_ptr<KeyboardLayoutDelegate> layout_delegate_;
  std::unique_ptr<KeyboardController> controller_;
  std::unique_ptr<ui::TextInputClient> test_text_input_client_;
  bool keyboard_closed_;
  DISALLOW_COPY_AND_ASSIGN(KeyboardControllerTest);
};

TEST_F(KeyboardControllerTest, KeyboardSize) {
  aura::Window* container(controller()->GetContainerWindow());
  aura::Window* keyboard(ui()->GetContentsWindow());
  gfx::Rect screen_bounds = root_window()->bounds();
  root_window()->AddChild(container);
  container->AddChild(keyboard);
  const gfx::Rect& initial_bounds = container->bounds();
  // The container should be positioned at the bottom of screen and has 0
  // height.
  ASSERT_EQ(0, initial_bounds.height());
  ASSERT_EQ(screen_bounds.height(), initial_bounds.y());
  VerifyKeyboardWindowSize(container, keyboard);

  // Attempt to change window width or move window up from the bottom are
  // ignored. Changing window height is supported.
  gfx::Rect expected_bounds(0,
                            screen_bounds.height() - 50,
                            screen_bounds.width(),
                            50);

  // The x position of new bounds may not be 0 if shelf is on the left side of
  // screen. The virtual keyboard should always align with the left edge of
  // screen. See http://crbug.com/510595.
  gfx::Rect new_bounds(10, 0, 50, 50);
  keyboard->SetBounds(new_bounds);
  ASSERT_EQ(expected_bounds, container->bounds());
  VerifyKeyboardWindowSize(container, keyboard);

  MockRotateScreen();
  // The above call should resize keyboard to new width while keeping the old
  // height.
  ASSERT_EQ(gfx::Rect(0,
                      screen_bounds.width() - 50,
                      screen_bounds.height(),
                      50),
            container->bounds());
  VerifyKeyboardWindowSize(container, keyboard);
}

// Flaky on Windows. See http://crbug.com/757044
#if defined(OS_WIN)
#define MAYBE_KeyboardSizeMultiRootWindow DISABLED_KeyboardSizeMultiRootWindow
#else
#define MAYBE_KeyboardSizeMultiRootWindow KeyboardSizeMultiRootWindow
#endif

TEST_F(KeyboardControllerTest, MAYBE_KeyboardSizeMultiRootWindow) {
  aura::Window* container(controller()->GetContainerWindow());
  aura::Window* keyboard(ui()->GetContentsWindow());
  gfx::Rect screen_bounds = root_window()->bounds();
  root_window()->AddChild(container);
  container->AddChild(keyboard);
  const gfx::Rect& initial_bounds = container->bounds();
  // The container should be positioned at the bottom of screen and has 0
  // height.
  ASSERT_EQ(0, initial_bounds.height());
  ASSERT_EQ(screen_bounds.height(), initial_bounds.y());
  VerifyKeyboardWindowSize(container, keyboard);

  // Adding new root window.
  std::unique_ptr<aura::WindowTreeHost> secondary_tree_host =
      base::WrapUnique<aura::WindowTreeHost>(
          aura::WindowTreeHost::Create(gfx::Rect(0, 0, 1000, 500)));
  secondary_tree_host->InitHost();
  EXPECT_EQ(1000, secondary_tree_host->window()->bounds().width());
  EXPECT_EQ(500, secondary_tree_host->window()->bounds().height());

  // Move the keyboard into the secondary root window.
  controller()->HideKeyboard(
      KeyboardController::HideReason::HIDE_REASON_AUTOMATIC);
  root_window()->RemoveChild(container);
  secondary_tree_host->window()->AddChild(container);

  const gfx::Rect& new_bounds = container->bounds();
  EXPECT_EQ(500, new_bounds.y());
  VerifyKeyboardWindowSize(container, keyboard);
}

// Tests that tapping/clicking inside the keyboard does not give it focus.
TEST_F(KeyboardControllerTest, ClickDoesNotFocusKeyboard) {
  ScopedAccessibilityKeyboardEnabler scoped_keyboard_enabler;
  const gfx::Rect& root_bounds = root_window()->bounds();
  aura::test::EventCountDelegate delegate;
  std::unique_ptr<aura::Window> window(new aura::Window(&delegate));
  window->Init(ui::LAYER_NOT_DRAWN);
  window->SetBounds(root_bounds);
  root_window()->AddChild(window.get());
  window->Show();
  window->Focus();

  aura::Window* keyboard_container(controller()->GetContainerWindow());

  root_window()->AddChild(keyboard_container);

  ShowKeyboard();

  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(keyboard_container->IsVisible());
  EXPECT_TRUE(window->HasFocus());
  EXPECT_FALSE(keyboard_container->HasFocus());

  // Click on the keyboard. Make sure the keyboard receives the event, but does
  // not get focus.
  EventObserver observer;
  keyboard_container->AddPreTargetHandler(&observer);

  ui::test::EventGenerator generator(root_window());
  generator.MoveMouseTo(keyboard_container->bounds().CenterPoint());
  generator.ClickLeftButton();
  EXPECT_TRUE(window->HasFocus());
  EXPECT_FALSE(keyboard_container->HasFocus());
  EXPECT_EQ("0 0", delegate.GetMouseButtonCountsAndReset());
  EXPECT_EQ(1, observer.GetEventCount(ui::ET_MOUSE_PRESSED));
  EXPECT_EQ(1, observer.GetEventCount(ui::ET_MOUSE_RELEASED));

  // Click outside of the keyboard. It should reach the window behind.
  generator.MoveMouseTo(gfx::Point());
  generator.ClickLeftButton();
  EXPECT_EQ("1 1", delegate.GetMouseButtonCountsAndReset());
  keyboard_container->RemovePreTargetHandler(&observer);
}

TEST_F(KeyboardControllerTest, VisibilityChangeWithTextInputTypeChange) {
  ScopedAccessibilityKeyboardEnabler scoped_keyboard_enabler;
  ui::DummyTextInputClient input_client_0(ui::TEXT_INPUT_TYPE_TEXT);
  ui::DummyTextInputClient input_client_1(ui::TEXT_INPUT_TYPE_TEXT);
  ui::DummyTextInputClient input_client_2(ui::TEXT_INPUT_TYPE_TEXT);
  ui::DummyTextInputClient no_input_client_0(ui::TEXT_INPUT_TYPE_NONE);
  ui::DummyTextInputClient no_input_client_1(ui::TEXT_INPUT_TYPE_NONE);

  base::RunLoop run_loop;
  aura::Window* keyboard_container(controller()->GetContainerWindow());
  std::unique_ptr<KeyboardContainerObserver> keyboard_container_observer(
      new KeyboardContainerObserver(keyboard_container, &run_loop));
  root_window()->AddChild(keyboard_container);

  SetFocus(&input_client_0);

  EXPECT_TRUE(keyboard_container->IsVisible());

  SetFocus(&no_input_client_0);
  // Keyboard should not immediately hide itself. It is delayed to avoid layout
  // flicker when the focus of input field quickly change.
  EXPECT_TRUE(keyboard_container->IsVisible());
  EXPECT_TRUE(WillHideKeyboard());
  // Wait for hide keyboard to finish.

  RunLoop(&run_loop);
  EXPECT_FALSE(keyboard_container->IsVisible());

  SetFocus(&input_client_1);
  EXPECT_TRUE(keyboard_container->IsVisible());

  // Schedule to hide keyboard.
  SetFocus(&no_input_client_1);
  EXPECT_TRUE(WillHideKeyboard());
  // Cancel keyboard hide.
  SetFocus(&input_client_2);

  EXPECT_FALSE(WillHideKeyboard());
  EXPECT_TRUE(keyboard_container->IsVisible());
}

// Test to prevent spurious overscroll boxes when changing tabs during keyboard
// hide. Refer to crbug.com/401670 for more context.
TEST_F(KeyboardControllerTest, CheckOverscrollInsetDuringVisibilityChange) {
  ui::DummyTextInputClient input_client(ui::TEXT_INPUT_TYPE_TEXT);
  ui::DummyTextInputClient no_input_client(ui::TEXT_INPUT_TYPE_NONE);

  aura::Window* keyboard_container(controller()->GetContainerWindow());
  root_window()->AddChild(keyboard_container);

  // Enable touch keyboard / overscroll mode to test insets.
  ScopedTouchKeyboardEnabler scoped_keyboard_enabler;
  EXPECT_TRUE(keyboard::IsKeyboardOverscrollEnabled());

  SetFocus(&input_client);
  SetFocus(&no_input_client);
  // Insets should not be enabled for new windows while keyboard is in the
  // process of hiding when overscroll is enabled.
  EXPECT_FALSE(ShouldEnableInsets(ui()->GetContentsWindow()));
  // Cancel keyboard hide.
  SetFocus(&input_client);
  // Insets should be enabled for new windows as hide was cancelled.
  EXPECT_TRUE(ShouldEnableInsets(ui()->GetContentsWindow()));
}

TEST_F(KeyboardControllerTest, AlwaysVisibleWhenLocked) {
  ScopedAccessibilityKeyboardEnabler scoped_keyboard_enabler;
  ui::DummyTextInputClient input_client_0(ui::TEXT_INPUT_TYPE_TEXT);
  ui::DummyTextInputClient input_client_1(ui::TEXT_INPUT_TYPE_TEXT);
  ui::DummyTextInputClient no_input_client_0(ui::TEXT_INPUT_TYPE_NONE);
  ui::DummyTextInputClient no_input_client_1(ui::TEXT_INPUT_TYPE_NONE);

  base::RunLoop run_loop;
  aura::Window* keyboard_container(controller()->GetContainerWindow());
  std::unique_ptr<KeyboardContainerObserver> keyboard_container_observer(
      new KeyboardContainerObserver(keyboard_container, &run_loop));
  root_window()->AddChild(keyboard_container);

  SetFocus(&input_client_0);

  EXPECT_TRUE(keyboard_container->IsVisible());

  // Lock keyboard.
  controller()->set_keyboard_locked(true);

  SetFocus(&no_input_client_0);
  // Keyboard should not try to hide itself as it is locked.
  EXPECT_TRUE(keyboard_container->IsVisible());
  EXPECT_FALSE(WillHideKeyboard());

  SetFocus(&input_client_1);
  EXPECT_TRUE(keyboard_container->IsVisible());

  // Unlock keyboard.
  controller()->set_keyboard_locked(false);

  // Keyboard should hide when focus on no input client.
  SetFocus(&no_input_client_1);
  EXPECT_TRUE(WillHideKeyboard());

  // Wait for hide keyboard to finish.
  RunLoop(&run_loop);
  EXPECT_FALSE(keyboard_container->IsVisible());
}

// Tests that deactivates keyboard will get closed event.
TEST_F(KeyboardControllerTest, CloseKeyboard) {
  ScopedAccessibilityKeyboardEnabler scoped_keyboard_enabler;
  aura::Window* keyboard_container(controller()->GetContainerWindow());
  root_window()->AddChild(keyboard_container);

  ShowKeyboard();
  EXPECT_TRUE(keyboard_container->IsVisible());
  EXPECT_FALSE(IsKeyboardClosed());

  root_window()->RemoveChild(keyboard_container);
  ResetController();
  EXPECT_TRUE(IsKeyboardClosed());
}

class KeyboardControllerAnimationTest : public KeyboardControllerTest {
 public:
  KeyboardControllerAnimationTest() {}
  ~KeyboardControllerAnimationTest() override {}

  void SetUp() override {
    // We cannot short-circuit animations for this test.
    ui::ScopedAnimationDurationScaleMode test_duration_mode(
        ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

    KeyboardControllerTest::SetUp();

    const gfx::Rect& root_bounds = root_window()->bounds();
    keyboard_container()->SetBounds(root_bounds);
    root_window()->AddChild(keyboard_container());
  }

  void TearDown() override {
    KeyboardControllerTest::TearDown();
  }

 protected:
  aura::Window* keyboard_container() {
    return controller()->GetContainerWindow();
  }

  aura::Window* contents_window() { return ui()->GetContentsWindow(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyboardControllerAnimationTest);
};

TEST_F(KeyboardControllerAnimationTest, ContainerAnimation) {
  ScopedAccessibilityKeyboardEnabler scoped_keyboard_enabler;
  ui::Layer* layer = keyboard_container()->layer();
  EXPECT_EQ(gfx::Rect(), notified_bounds());
  ShowKeyboard();

  // Keyboard container and window should immediately become visible before
  // animation starts.
  EXPECT_TRUE(keyboard_container()->IsVisible());
  EXPECT_TRUE(contents_window()->IsVisible());
  float show_start_opacity = layer->opacity();
  gfx::Transform transform;
  transform.Translate(0, keyboard::kFullWidthKeyboardAnimationDistance);
  EXPECT_EQ(transform, layer->transform());
  // animation occurs in a cloned layer, so the actual final bounds should
  // already be applied to the container.
  EXPECT_EQ(keyboard_container()->bounds(), notified_bounds());

  RunAnimationForLayer(layer);
  EXPECT_TRUE(keyboard_container()->IsVisible());
  EXPECT_TRUE(contents_window()->IsVisible());
  float show_end_opacity = layer->opacity();
  EXPECT_LT(show_start_opacity, show_end_opacity);
  EXPECT_EQ(gfx::Transform(), layer->transform());
  // KeyboardController should notify the bounds of container window to its
  // observers after show animation finished.
  EXPECT_EQ(keyboard_container()->bounds(), notified_bounds());

  // Directly hide keyboard without delay.
  float hide_start_opacity = layer->opacity();
  controller()->HideKeyboard(KeyboardController::HIDE_REASON_AUTOMATIC);
  EXPECT_FALSE(keyboard_container()->IsVisible());
  EXPECT_FALSE(keyboard_container()->layer()->visible());
  EXPECT_FALSE(contents_window()->IsVisible());
  layer = keyboard_container()->layer();
  // KeyboardController should notify the bounds of keyboard window to its
  // observers before hide animation starts.
  EXPECT_EQ(gfx::Rect(), notified_bounds());

  RunAnimationForLayer(layer);
  EXPECT_FALSE(keyboard_container()->IsVisible());
  EXPECT_FALSE(keyboard_container()->layer()->visible());
  EXPECT_FALSE(contents_window()->IsVisible());
  float hide_end_opacity = layer->opacity();
  EXPECT_GT(hide_start_opacity, hide_end_opacity);
  EXPECT_EQ(gfx::Rect(), notified_bounds());
}

// Show keyboard during keyboard hide animation should abort the hide animation
// and the keyboard should animate in.
// Test for crbug.com/333284.
TEST_F(KeyboardControllerAnimationTest, ContainerShowWhileHide) {
  ScopedAccessibilityKeyboardEnabler scoped_keyboard_enabler;
  ui::Layer* layer = keyboard_container()->layer();
  ShowKeyboard();
  RunAnimationForLayer(layer);

  controller()->HideKeyboard(KeyboardController::HIDE_REASON_AUTOMATIC);
  // Before hide animation finishes, show keyboard again.
  ShowKeyboard();
  layer = keyboard_container()->layer();
  RunAnimationForLayer(layer);
  EXPECT_TRUE(keyboard_container()->IsVisible());
  EXPECT_TRUE(contents_window()->IsVisible());
  EXPECT_EQ(1.0, layer->opacity());
}

TEST_F(KeyboardControllerAnimationTest, SetBoundsOnOldKeyboardUiReference) {
  ScopedAccessibilityKeyboardEnabler scoped_keyboard_enabler;

  // Ensure keyboard ui is populated
  ui::Layer* layer = keyboard_container()->layer();
  ShowKeyboard();
  RunAnimationForLayer(layer);

  ASSERT_TRUE(controller()->ui());

  aura::Window* container_window = controller()->GetContainerWindow();

  // Simulate removal of keyboard controller from root window as done by
  // RootWindowController::DeactivateKeyboard()
  container_window->parent()->RemoveChild(container_window);

  // lingering handle to the contents window is adjusted.
  // container_window's LayoutManager should abort silently and not crash.
  controller()->ui()->GetContentsWindow()->SetBounds(gfx::Rect());
}

TEST_F(KeyboardControllerTest, DisplayChangeShouldNotifyBoundsChange) {
  ScopedTouchKeyboardEnabler scoped_keyboard_enabler;
  ui::DummyTextInputClient input_client(ui::TEXT_INPUT_TYPE_TEXT);

  aura::Window* container(controller()->GetContainerWindow());
  root_window()->AddChild(container);

  SetFocus(&input_client);
  gfx::Rect new_bounds(0, 0, 1280, 800);
  ASSERT_NE(new_bounds, root_window()->bounds());
  EXPECT_EQ(1, number_of_calls());
  root_window()->SetBounds(new_bounds);
  EXPECT_EQ(2, number_of_calls());
  MockRotateScreen();
  EXPECT_EQ(3, number_of_calls());
}

}  // namespace keyboard
