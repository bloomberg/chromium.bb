// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/keyboard_controller.h"

#include <set>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_observer.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/hit_test.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/path.h"
#include "ui/gfx/skia_util.h"
#include "ui/keyboard/keyboard_controller_observer.h"
#include "ui/keyboard/keyboard_layout_manager.h"
#include "ui/keyboard/keyboard_ui.h"
#include "ui/keyboard/keyboard_util.h"

#if defined(OS_CHROMEOS)
#include "base/process/launch.h"
#include "base/sys_info.h"
#if defined(USE_OZONE)
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"
#endif
#endif  // if defined(OS_CHROMEOS)

namespace {

const int kHideKeyboardDelayMs = 100;

// The virtual keyboard show/hide animation duration.
const int kAnimationDurationMs = 100;

// The opacity of virtual keyboard container when show animation starts or
// hide animation finishes. This cannot be zero because we call Show() on the
// keyboard window before setting the opacity back to 1.0. Since windows are not
// allowed to be shown with zero opacity, we always animate to 0.01 instead.
const float kAnimationStartOrAfterHideOpacity = 0.01f;

// The KeyboardWindowDelegate makes sure the keyboard-window does not get focus.
// This is necessary to make sure that the synthetic key-events reach the target
// window.
// The delegate deletes itself when the window is destroyed.
class KeyboardWindowDelegate : public aura::WindowDelegate {
 public:
  KeyboardWindowDelegate() {}
  ~KeyboardWindowDelegate() override {}

 private:
  // Overridden from aura::WindowDelegate:
  gfx::Size GetMinimumSize() const override { return gfx::Size(); }
  gfx::Size GetMaximumSize() const override { return gfx::Size(); }
  void OnBoundsChanged(const gfx::Rect& old_bounds,
                       const gfx::Rect& new_bounds) override {}
  gfx::NativeCursor GetCursor(const gfx::Point& point) override {
    return gfx::kNullCursor;
  }
  int GetNonClientComponent(const gfx::Point& point) const override {
    return HTNOWHERE;
  }
  bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) override {
    return true;
  }
  bool CanFocus() override { return false; }
  void OnCaptureLost() override {}
  void OnPaint(const ui::PaintContext& context) override {}
  void OnDeviceScaleFactorChanged(float device_scale_factor) override {}
  void OnWindowDestroying(aura::Window* window) override {}
  void OnWindowDestroyed(aura::Window* window) override { delete this; }
  void OnWindowTargetVisibilityChanged(bool visible) override {}
  bool HasHitTestMask() const override { return false; }
  void GetHitTestMask(gfx::Path* mask) const override {}

  DISALLOW_COPY_AND_ASSIGN(KeyboardWindowDelegate);
};

void ToggleTouchEventLogging(bool enable) {
#if defined(OS_CHROMEOS)
#if defined(USE_OZONE)
  // TODO(moshayedi): crbug.com/642863. Revisit when we have mojo interface for
  // InputController for processes that aren't mus-ws.
  if (aura::Env::GetInstance()->mode() == aura::Env::Mode::MUS)
    return;
  ui::InputController* controller =
      ui::OzonePlatform::GetInstance()->GetInputController();
  if (controller)
    controller->SetTouchEventLoggingEnabled(enable);
#elif defined(USE_X11)
  if (!base::SysInfo::IsRunningOnChromeOS())
    return;
  base::CommandLine command(
      base::FilePath("/opt/google/touchscreen/toggle_touch_event_logging"));
  if (enable)
    command.AppendArg("1");
  else
    command.AppendArg("0");
  VLOG(1) << "Running " << command.GetCommandLineString();
  base::LaunchOptions options;
  options.wait = true;
  base::LaunchProcess(command, options);
#endif
#endif  // defined(OS_CHROMEOS)
}

}  // namespace

namespace keyboard {

// Observer for both keyboard show and hide animations. It should be owned by
// KeyboardController.
class CallbackAnimationObserver : public ui::LayerAnimationObserver {
 public:
  CallbackAnimationObserver(const scoped_refptr<ui::LayerAnimator>& animator,
                            base::Callback<void(void)> callback);
  ~CallbackAnimationObserver() override;

 private:
  // Overridden from ui::LayerAnimationObserver:
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* seq) override;
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* seq) override;
  void OnLayerAnimationScheduled(ui::LayerAnimationSequence* seq) override {}

  scoped_refptr<ui::LayerAnimator> animator_;
  base::Callback<void(void)> callback_;

  DISALLOW_COPY_AND_ASSIGN(CallbackAnimationObserver);
};

CallbackAnimationObserver::CallbackAnimationObserver(
    const scoped_refptr<ui::LayerAnimator>& animator,
    base::Callback<void(void)> callback)
    : animator_(animator), callback_(callback) {
}

CallbackAnimationObserver::~CallbackAnimationObserver() {
  animator_->RemoveObserver(this);
}

void CallbackAnimationObserver::OnLayerAnimationEnded(
    ui::LayerAnimationSequence* seq) {
  if (animator_->is_animating())
    return;
  animator_->RemoveObserver(this);
  callback_.Run();
}

void CallbackAnimationObserver::OnLayerAnimationAborted(
    ui::LayerAnimationSequence* seq) {
  animator_->RemoveObserver(this);
}

// static
KeyboardController* KeyboardController::instance_ = NULL;

KeyboardController::KeyboardController(KeyboardUI* ui,
                                       KeyboardLayoutDelegate* delegate)
    : ui_(ui),
      layout_delegate_(delegate),
      input_method_(NULL),
      keyboard_visible_(false),
      show_on_resize_(false),
      keyboard_locked_(false),
      keyboard_mode_(FULL_WIDTH),
      weak_factory_(this) {
  CHECK(ui);
  input_method_ = ui_->GetInputMethod();
  input_method_->AddObserver(this);
  ui_->SetController(this);
}

KeyboardController::~KeyboardController() {
  if (container_) {
    if (container_->GetRootWindow())
      container_->GetRootWindow()->RemoveObserver(this);
    container_->RemoveObserver(this);
    container_->RemovePreTargetHandler(&event_filter_);
  }
  if (input_method_)
    input_method_->RemoveObserver(this);
  for (KeyboardControllerObserver& observer : observer_list_)
    observer.OnKeyboardClosed();
  ui_->SetController(nullptr);
}

// static
void KeyboardController::ResetInstance(KeyboardController* controller) {
  if (instance_ && instance_ != controller)
    delete instance_;
  instance_ = controller;
}

// static
KeyboardController* KeyboardController::GetInstance() {
  return instance_;
}

aura::Window* KeyboardController::GetContainerWindow() {
  if (!container_.get()) {
    container_.reset(new aura::Window(new KeyboardWindowDelegate()));
    container_->SetName("KeyboardContainer");
    container_->set_owned_by_parent(false);
    container_->Init(ui::LAYER_NOT_DRAWN);
    container_->AddObserver(this);
    container_->SetLayoutManager(new KeyboardLayoutManager(this));
    container_->AddPreTargetHandler(&event_filter_);
  }
  return container_.get();
}

aura::Window* KeyboardController::GetContainerWindowWithoutCreationForTest() {
  return container_.get();
}

void KeyboardController::NotifyKeyboardBoundsChanging(
    const gfx::Rect& new_bounds) {
  current_keyboard_bounds_ = new_bounds;
  if (ui_->HasKeyboardWindow() && ui_->GetKeyboardWindow()->IsVisible()) {
    for (KeyboardControllerObserver& observer : observer_list_)
      observer.OnKeyboardBoundsChanging(new_bounds);
    if (keyboard::IsKeyboardOverscrollEnabled())
      ui_->InitInsets(new_bounds);
    else
      ui_->ResetInsets();
  } else {
    current_keyboard_bounds_ = gfx::Rect();
  }
}

void KeyboardController::HideKeyboard(HideReason reason) {
  keyboard_visible_ = false;
  ToggleTouchEventLogging(true);

  keyboard::LogKeyboardControlEvent(
      reason == HIDE_REASON_AUTOMATIC ?
          keyboard::KEYBOARD_CONTROL_HIDE_AUTO :
          keyboard::KEYBOARD_CONTROL_HIDE_USER);

  NotifyKeyboardBoundsChanging(gfx::Rect());

  set_keyboard_locked(false);

  ui::LayerAnimator* container_animator = container_->layer()->GetAnimator();
  animation_observer_.reset(new CallbackAnimationObserver(
      container_animator,
      base::Bind(&KeyboardController::HideAnimationFinished,
                 base::Unretained(this))));
  container_animator->AddObserver(animation_observer_.get());

  ui::ScopedLayerAnimationSettings settings(container_animator);
  settings.SetTweenType(gfx::Tween::FAST_OUT_LINEAR_IN);
  settings.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kAnimationDurationMs));
  gfx::Transform transform;
  transform.Translate(0, kAnimationDistance);
  container_->SetTransform(transform);
  container_->layer()->SetOpacity(kAnimationStartOrAfterHideOpacity);
}

void KeyboardController::AddObserver(KeyboardControllerObserver* observer) {
  observer_list_.AddObserver(observer);
}

bool KeyboardController::HasObserver(KeyboardControllerObserver* observer) {
  return observer_list_.HasObserver(observer);
}

void KeyboardController::RemoveObserver(KeyboardControllerObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void KeyboardController::SetKeyboardMode(KeyboardMode mode) {
  if (keyboard_mode_ == mode)
    return;

  keyboard_mode_ = mode;
  // When keyboard is floating, no overscroll or resize is necessary. Sets
  // keyboard bounds to zero so overscroll or resize is disabled.
  if (keyboard_mode_ == FLOATING) {
    NotifyKeyboardBoundsChanging(gfx::Rect());
  } else if (keyboard_mode_ == FULL_WIDTH) {
    AdjustKeyboardBounds();
    // No animation added, so call ShowAnimationFinished immediately.
    ShowAnimationFinished();
  }
}

void KeyboardController::ShowKeyboard(bool lock) {
  set_keyboard_locked(lock);
  ShowKeyboardInternal(display::kInvalidDisplayId);
}

void KeyboardController::ShowKeyboardInDisplay(int64_t display_id) {
  set_keyboard_locked(true);
  ShowKeyboardInternal(display_id);
}

bool KeyboardController::IsKeyboardWindowCreated() {
  return keyboard_container_initialized() && ui_->HasKeyboardWindow();
}

void KeyboardController::OnWindowHierarchyChanged(
    const HierarchyChangeParams& params) {
  if (params.new_parent && params.target == container_.get())
    OnTextInputStateChanged(ui_->GetInputMethod()->GetTextInputClient());
}

void KeyboardController::OnWindowAddedToRootWindow(aura::Window* window) {
  if (!window->GetRootWindow()->HasObserver(this))
    window->GetRootWindow()->AddObserver(this);
  AdjustKeyboardBounds();
}

void KeyboardController::OnWindowRemovingFromRootWindow(aura::Window* window,
    aura::Window* new_root) {
  if (window->GetRootWindow()->HasObserver(this))
    window->GetRootWindow()->RemoveObserver(this);
}

void KeyboardController::OnWindowBoundsChanged(aura::Window* window,
                                               const gfx::Rect& old_bounds,
                                               const gfx::Rect& new_bounds) {
  if (!window->IsRootWindow())
    return;
  // Keep the same height when window resize. It gets called when screen
  // rotate.
  if (!keyboard_container_initialized() || !ui_->HasKeyboardWindow())
    return;

  int container_height = container_->bounds().height();
  if (keyboard_mode_ == FULL_WIDTH) {
    container_->SetBounds(gfx::Rect(new_bounds.x(),
                                    new_bounds.bottom() - container_height,
                                    new_bounds.width(),
                                    container_height));
  } else if (keyboard_mode_ == FLOATING) {
    // When screen rotate, horizontally center floating virtual keyboard
    // window and vertically align it to the bottom.
    int container_width = container_->bounds().width();
    container_->SetBounds(gfx::Rect(
        new_bounds.x() + (new_bounds.width() - container_width) / 2,
        new_bounds.bottom() - container_height,
        container_width,
        container_height));
  }
}

void KeyboardController::Reload() {
  if (ui_->HasKeyboardWindow()) {
    // A reload should never try to show virtual keyboard. If keyboard is not
    // visible before reload, it should keep invisible after reload.
    show_on_resize_ = false;
    ui_->ReloadKeyboardIfNeeded();
  }
}

void KeyboardController::OnTextInputStateChanged(
    const ui::TextInputClient* client) {
  if (!container_.get())
    return;

  ui::TextInputType type =
      client ? client->GetTextInputType() : ui::TEXT_INPUT_TYPE_NONE;

  if (type == ui::TEXT_INPUT_TYPE_NONE && !keyboard_locked_) {
    if (keyboard_visible_) {
      // Set the visibility state here so that any queries for visibility
      // before the timer fires returns the correct future value.
      keyboard_visible_ = false;
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&KeyboardController::HideKeyboard,
                     weak_factory_.GetWeakPtr(), HIDE_REASON_AUTOMATIC),
          base::TimeDelta::FromMilliseconds(kHideKeyboardDelayMs));
    }
  } else {
    // Abort a pending keyboard hide.
    if (WillHideKeyboard()) {
      weak_factory_.InvalidateWeakPtrs();
      keyboard_visible_ = true;
    }
    ui_->SetUpdateInputType(type);
    // Do not explicitly show the Virtual keyboard unless it is in the process
    // of hiding. Instead, the virtual keyboard is shown in response to a user
    // gesture (mouse or touch) that is received while an element has input
    // focus. Showing the keyboard requires an explicit call to
    // OnShowImeIfNeeded.
  }
}

void KeyboardController::OnInputMethodDestroyed(
    const ui::InputMethod* input_method) {
  DCHECK_EQ(input_method_, input_method);
  input_method_ = NULL;
}

void KeyboardController::OnShowImeIfNeeded() {
  // Calling |ShowKeyboardInternal| may move the keyboard to another display.
  if (IsKeyboardEnabled() && !keyboard_locked())
    ShowKeyboardInternal(display::kInvalidDisplayId);
}

void KeyboardController::ShowKeyboardInternal(int64_t display_id) {
  // The container window should have been created already when
  // |Shell::CreateKeyboard| is called.
  DCHECK(container_.get());

  if (container_->children().empty()) {
    keyboard::MarkKeyboardLoadStarted();
    aura::Window* keyboard = ui_->GetKeyboardWindow();
    keyboard->Show();
    container_->AddChild(keyboard);
    keyboard->set_owned_by_parent(false);
  }

  ui_->ReloadKeyboardIfNeeded();

  if (layout_delegate_ != nullptr) {
    if (display_id != display::kInvalidDisplayId)
      layout_delegate_->MoveKeyboardToDisplay(display_id);
    else
      layout_delegate_->MoveKeyboardToTouchableDisplay();
  }

  if (keyboard_visible_) {
    return;
  } else if (ui_->GetKeyboardWindow()->bounds().height() == 0) {
    show_on_resize_ = true;
    return;
  }

  keyboard_visible_ = true;

  // If the controller is in the process of hiding the keyboard, do not log
  // the stat here since the keyboard will not actually be shown.
  if (!WillHideKeyboard())
    keyboard::LogKeyboardControlEvent(keyboard::KEYBOARD_CONTROL_SHOW);

  weak_factory_.InvalidateWeakPtrs();

  // If |container_| has hide animation, its visibility is set to false when
  // hide animation finished. So even if the container is visible at this
  // point, it may in the process of hiding. We still need to show keyboard
  // container in this case.
  if (container_->IsVisible() &&
      !container_->layer()->GetAnimator()->is_animating())
    return;

  ToggleTouchEventLogging(false);
  ui::LayerAnimator* container_animator = container_->layer()->GetAnimator();

  // If the container is not animating, makes sure the position and opacity
  // are at begin states for animation.
  if (!container_animator->is_animating()) {
    gfx::Transform transform;
    transform.Translate(0, kAnimationDistance);
    container_->SetTransform(transform);
    container_->layer()->SetOpacity(kAnimationStartOrAfterHideOpacity);
  }

  container_animator->set_preemption_strategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  if (keyboard_mode_ == FLOATING) {
    animation_observer_.reset();
  } else {
    animation_observer_.reset(new CallbackAnimationObserver(
        container_animator,
        base::Bind(&KeyboardController::ShowAnimationFinished,
                   base::Unretained(this))));
    container_animator->AddObserver(animation_observer_.get());
  }

  ui_->ShowKeyboardContainer(container_.get());

  {
    // Scope the following animation settings as we don't want to animate
    // visibility change that triggered by a call to the base class function
    // ShowKeyboardContainer with these settings. The container should become
    // visible immediately.
    ui::ScopedLayerAnimationSettings settings(container_animator);
    settings.SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kAnimationDurationMs));
    container_->SetTransform(gfx::Transform());
    container_->layer()->SetOpacity(1.0);
  }
}

bool KeyboardController::WillHideKeyboard() const {
  return weak_factory_.HasWeakPtrs();
}

void KeyboardController::ShowAnimationFinished() {
  MarkKeyboardLoadFinished();

  // Notify observers after animation finished to prevent reveal desktop
  // background during animation.
  NotifyKeyboardBoundsChanging(container_->bounds());
  ui_->EnsureCaretInWorkArea();
}

void KeyboardController::HideAnimationFinished() {
  ui_->HideKeyboardContainer(container_.get());
  for (KeyboardControllerObserver& observer : observer_list_)
    observer.OnKeyboardHidden();
  ui_->EnsureCaretInWorkArea();
}

void KeyboardController::AdjustKeyboardBounds() {
  // When keyboard is floating, no resize is necessary.
  if (keyboard_mode_ == FLOATING)
    return;

  if (keyboard_mode_ == FULL_WIDTH) {
    // TODO(bshe): revisit this logic after we decide to support resize virtual
    // keyboard.
    int keyboard_height = GetContainerWindow()->bounds().height();
    const gfx::Rect& root_bounds = container_->GetRootWindow()->bounds();
    gfx::Rect new_bounds = root_bounds;
    new_bounds.set_y(root_bounds.height() - keyboard_height);
    new_bounds.set_height(keyboard_height);
    GetContainerWindow()->SetBounds(new_bounds);
  }
}

}  // namespace keyboard
