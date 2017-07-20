// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/keyboard_controller.h"

#include <set>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
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
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/path.h"
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

constexpr int kHideKeyboardDelayMs = 100;

// The virtual keyboard show/hide animation duration.
constexpr int kAnimationDurationMs = 100;

// Reports an error histogram if the keyboard state is lingering in an
// intermediate state for more than 5 seconds.
constexpr int kReportLingeringStateDelayMs = 5000;

// The opacity of virtual keyboard container when show animation starts or
// hide animation finishes. This cannot be zero because we call Show() on the
// keyboard window before setting the opacity back to 1.0. Since windows are not
// allowed to be shown with zero opacity, we always animate to 0.01 instead.
constexpr float kAnimationStartOrAfterHideOpacity = 0.01f;

// State transition diagram (document linked from crbug.com/719905)
bool isAllowedStateStansition(keyboard::KeyboardControllerState from,
                              keyboard::KeyboardControllerState to) {
  static const std::set<std::pair<keyboard::KeyboardControllerState,
                                  keyboard::KeyboardControllerState>>
      kAllowedStateTransition = {
          // The initial ShowKeyboard scenario
          // INITIAL -> LOADING_EXTENSION -> SHOWING -> SHOWN.
          {keyboard::KeyboardControllerState::UNKNOWN,
           keyboard::KeyboardControllerState::INITIAL},
          {keyboard::KeyboardControllerState::INITIAL,
           keyboard::KeyboardControllerState::LOADING_EXTENSION},
          {keyboard::KeyboardControllerState::LOADING_EXTENSION,
           keyboard::KeyboardControllerState::SHOWING},
          {keyboard::KeyboardControllerState::SHOWING,
           keyboard::KeyboardControllerState::SHOWN},

          // Hide scenario
          // SHOWN -> WILL_HIDE -> HIDING -> HIDDEN.
          {keyboard::KeyboardControllerState::SHOWN,
           keyboard::KeyboardControllerState::WILL_HIDE},
          {keyboard::KeyboardControllerState::WILL_HIDE,
           keyboard::KeyboardControllerState::HIDING},
          {keyboard::KeyboardControllerState::HIDING,
           keyboard::KeyboardControllerState::HIDDEN},

          // Focus transition scenario
          // SHOWN -> WILL_HIDE -> SHOWN.
          {keyboard::KeyboardControllerState::WILL_HIDE,
           keyboard::KeyboardControllerState::SHOWN},

          // The second and subsequent ShowKeyboard scenario
          // HIDDEN -> SHOWING -> SHOWN.
          {keyboard::KeyboardControllerState::HIDDEN,
           keyboard::KeyboardControllerState::SHOWING},

          // HideKeyboard can be called at anytime for example on shutdown.
          {keyboard::KeyboardControllerState::SHOWN,
           keyboard::KeyboardControllerState::HIDING},
          {keyboard::KeyboardControllerState::SHOWING,
           keyboard::KeyboardControllerState::HIDING},
          {keyboard::KeyboardControllerState::LOADING_EXTENSION,
           keyboard::KeyboardControllerState::HIDING},

          // SHOWN -> SHOWN is possible when keyboard mode is changed.
          // See KeyboardControllerTest.SwitchToFullWidthVirtualKeyboard.
          {keyboard::KeyboardControllerState::SHOWN,
           keyboard::KeyboardControllerState::SHOWN},

          // HIDING -> SHOWING is possible when ShowKeyboard is called during
          // hide
          // animation. KeyboardControllerAnimationTest.ContainerShowWhileHide.
          {keyboard::KeyboardControllerState::HIDING,
           keyboard::KeyboardControllerState::SHOWING},

          // SHOWING -> WILL_HIDE is possible when focus is removed while
          // keyboard
          // being shown.
          {keyboard::KeyboardControllerState::SHOWING,
           keyboard::KeyboardControllerState::WILL_HIDE},

          // LOADING_EXTENSION -> INITIAL is possible when |HideKeyboard| is
          // called while wating for resize.
          // See VirtualKeyboardRootWindowControllerTest.FollowInputFocus.
          {keyboard::KeyboardControllerState::LOADING_EXTENSION,
           keyboard::KeyboardControllerState::INITIAL},

          // LOADING_EXTENSION -> LOADING_EXTENSION is possible when focused
          // window is changed and |ShowKeyboard| is called while waiting for
          // resize.
          // See VirtualKeyboardRootWindowControllerTest
          //     .VirtualKeyboardOnTouchableDisplayOnly.
          {keyboard::KeyboardControllerState::LOADING_EXTENSION,
           keyboard::KeyboardControllerState::LOADING_EXTENSION},

          // LOADING_EXTENSION -> HIDDEN occurs when the extension is pre-loaded
          {keyboard::KeyboardControllerState::LOADING_EXTENSION,
           keyboard::KeyboardControllerState::HIDDEN},
      };
  return kAllowedStateTransition.count(std::make_pair(from, to)) == 1;
};

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

std::string StateToStr(keyboard::KeyboardControllerState state) {
  switch (state) {
    case keyboard::KeyboardControllerState::UNKNOWN:
      return "UNKNOWN";
    case keyboard::KeyboardControllerState::SHOWN:
      return "SHOWN";
    case keyboard::KeyboardControllerState::SHOWING:
      return "SHOWING";
    case keyboard::KeyboardControllerState::LOADING_EXTENSION:
      return "LOADING_EXTENSION";
    case keyboard::KeyboardControllerState::WILL_HIDE:
      return "WILL_HIDE";
    case keyboard::KeyboardControllerState::HIDING:
      return "HIDING";
    case keyboard::KeyboardControllerState::HIDDEN:
      return "HIDDEN";
    case keyboard::KeyboardControllerState::INITIAL:
      return "INITIAL";
    case keyboard::KeyboardControllerState::COUNT:
      NOTREACHED();
  }
  NOTREACHED() << "Unknownstate: " << static_cast<int>(state);
  // Needed for windows build.
  return "";
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

KeyboardController::KeyboardController(std::unique_ptr<KeyboardUI> ui,
                                       KeyboardLayoutDelegate* delegate)
    : ui_(std::move(ui)),
      layout_delegate_(delegate),
      keyboard_visible_(false),
      show_on_resize_(false),
      keyboard_locked_(false),
      keyboard_mode_(FULL_WIDTH),
      state_(KeyboardControllerState::UNKNOWN),
      weak_factory_report_lingering_state_(this),
      weak_factory_will_hide_(this) {
  ui_->GetInputMethod()->AddObserver(this);
  ui_->SetController(this);
  ChangeState(KeyboardControllerState::INITIAL);
}

KeyboardController::~KeyboardController() {
  if (container_) {
    if (container_->GetRootWindow())
      container_->GetRootWindow()->RemoveObserver(this);
    container_->RemoveObserver(this);
    container_->RemovePreTargetHandler(&event_filter_);
  }
  ui_->GetInputMethod()->RemoveObserver(this);
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

void KeyboardController::NotifyContentsBoundsChanging(
    const gfx::Rect& new_bounds) {
  current_keyboard_bounds_ = new_bounds;
  if (ui_->HasContentsWindow() && ui_->GetContentsWindow()->IsVisible()) {
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

void KeyboardController::SetContainerBounds(const gfx::Rect& new_bounds,
                                            const bool contents_loaded) {
  ui::LayerAnimator* animator = container_->layer()->GetAnimator();
  // Stops previous animation if a window resize is requested during animation.
  if (animator->is_animating())
    animator->StopAnimating();

  container_->SetBounds(new_bounds);

  if (contents_loaded && show_on_resize()) {
    // The window height is set to 0 initially or before switch to an IME in a
    // different extension. Virtual keyboard window may wait for this bounds
    // change to correctly animate in.
    if (keyboard_locked()) {
      // Do not move the keyboard to another display after switch to an IME in a
      // different extension.
      const int64_t display_id =
          display::Screen::GetScreen()
              ->GetDisplayNearestWindow(GetContainerWindow())
              .id();
      ShowKeyboardInDisplay(display_id);
    } else {
      ShowKeyboard(false /* lock */);
    }
  } else {
    if (contents_loaded && !keyboard_visible()) {
      // When the child is layed out, the controller is not shown, but showing
      // is not desired, this is indicative that the pre-load has completed.
      if (state_ == KeyboardControllerState::LOADING_EXTENSION) {
        ChangeState(KeyboardControllerState::HIDDEN);
      }
    }

    if (keyboard_mode() == FULL_WIDTH) {
      // We need to send out this notification only if keyboard is visible since
      // the contents window is resized even if keyboard is hidden.
      if (keyboard_visible())
        NotifyContentsBoundsChanging(new_bounds);
    } else if (keyboard_mode() == FLOATING) {
      NotifyContentsBoundsChanging(gfx::Rect());
    }
  }
}

void KeyboardController::HideKeyboard(HideReason reason) {
  TRACE_EVENT0("vk", "HideKeyboard");

  if (state_ == KeyboardControllerState::HIDING ||
      state_ == KeyboardControllerState::HIDDEN ||
      state_ == KeyboardControllerState::INITIAL) {
    // Do nothing if keyboard is already hidden or being hidden.
    return;
  }

  weak_factory_will_hide_.InvalidateWeakPtrs();
  keyboard_visible_ = false;
  ToggleTouchEventLogging(true);

  keyboard::LogKeyboardControlEvent(
      reason == HIDE_REASON_AUTOMATIC ?
          keyboard::KEYBOARD_CONTROL_HIDE_AUTO :
          keyboard::KEYBOARD_CONTROL_HIDE_USER);

  NotifyContentsBoundsChanging(gfx::Rect());

  set_keyboard_locked(false);

  if (state_ == KeyboardControllerState::LOADING_EXTENSION) {
    ChangeState(KeyboardControllerState::INITIAL);
    return;
  }

  // Change the state here instead of at the end of the function because
  // the animation immediately starts and may finish before the end of the
  // function is reached.
  ChangeState(KeyboardControllerState::HIDING);

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

bool KeyboardController::HasObserver(
    KeyboardControllerObserver* observer) const {
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
    NotifyContentsBoundsChanging(gfx::Rect());
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
  return keyboard_container_initialized() && ui_->HasContentsWindow();
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
  if (!keyboard_container_initialized() || !ui_->HasContentsWindow())
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
  if (ui_->HasContentsWindow()) {
    // A reload should never try to show virtual keyboard. If keyboard is not
    // visible before reload, it should keep invisible after reload.
    show_on_resize_ = false;
    ui_->ReloadKeyboardIfNeeded();
  }
}

void KeyboardController::OnTextInputStateChanged(
    const ui::TextInputClient* client) {
  if (!container_.get()) {
    DCHECK(state_ == KeyboardControllerState::HIDDEN ||
           state_ == KeyboardControllerState::INITIAL);
    return;
  }

  TRACE_EVENT0("vk", "OnTextInputStateChanged");

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
                     weak_factory_will_hide_.GetWeakPtr(),
                     HIDE_REASON_AUTOMATIC),
          base::TimeDelta::FromMilliseconds(kHideKeyboardDelayMs));
      if (state_ == KeyboardControllerState::LOADING_EXTENSION) {
        show_on_resize_ = false;
        ChangeState(KeyboardControllerState::INITIAL);
      } else {
        ChangeState(KeyboardControllerState::WILL_HIDE);
      }
    }
  } else {
    // Abort a pending keyboard hide.
    if (WillHideKeyboard()) {
      // TODO(oka): state_ is SHOWN in
      // VirtualKeyboardWebContentTest.EnableIMEInDifferentExtension.
      // Investigate why, and enable the DCHECK below.
      // DCHECK(state_ == KeyboardControllerState::WILL_HIDE) <<
      // StateToStr(state_);
      weak_factory_will_hide_.InvalidateWeakPtrs();
      keyboard_visible_ = true;
      ChangeState(KeyboardControllerState::SHOWN);
    }
    // Do not explicitly show the Virtual keyboard unless it is in the process
    // of hiding. Instead, the virtual keyboard is shown in response to a user
    // gesture (mouse or touch) that is received while an element has input
    // focus. Showing the keyboard requires an explicit call to
    // OnShowImeIfNeeded.
  }
}

void KeyboardController::OnShowImeIfNeeded() {
  // Calling |ShowKeyboardInternal| may move the keyboard to another display.
  if (IsKeyboardEnabled() && !keyboard_locked())
    ShowKeyboardInternal(display::kInvalidDisplayId);
}

void KeyboardController::LoadKeyboardUiInBackground() {
  // ShowKeyboardInternal may trigger RootControllerWindow::ActiveKeyboard which
  // will cause LoadKeyboardUiInBackground to potentially run even though the
  // keyboard has been initialized.
  if (state_ != KeyboardControllerState::INITIAL)
    return;

  // The container window should have been created already when
  // |Shell::CreateKeyboard| is called.
  DCHECK(container_.get());

  PopulateKeyboardContent(display::kInvalidDisplayId, false);
}

void KeyboardController::ShowKeyboardInternal(int64_t display_id) {
  DCHECK(container_.get());
  keyboard::MarkKeyboardLoadStarted();
  PopulateKeyboardContent(display_id, true);
}

void KeyboardController::PopulateKeyboardContent(int64_t display_id,
                                                 bool show_keyboard) {
  TRACE_EVENT0("vk", "ShowKeyboardInternal");

  if (container_->children().empty()) {
    aura::Window* keyboard = ui_->GetContentsWindow();
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
  } else if (!show_keyboard ||
             ui_->GetContentsWindow()->bounds().height() == 0) {
    if (show_keyboard) {
      // show the keyboard once loading is complete.
      show_on_resize_ = true;
    }
    ChangeState(KeyboardControllerState::LOADING_EXTENSION);
    return;
  }

  keyboard_visible_ = true;

  // If the controller is in the process of hiding the keyboard, do not log
  // the stat here since the keyboard will not actually be shown.
  if (!WillHideKeyboard()) {
    keyboard::LogKeyboardControlEvent(keyboard::KEYBOARD_CONTROL_SHOW);
  } else {
    weak_factory_will_hide_.InvalidateWeakPtrs();
  }

  // If |container_| has hide animation, its visibility is set to false when
  // hide animation finished. So even if the container is visible at this
  // point, it may in the process of hiding. We still need to show keyboard
  // container in this case.
  if (container_->IsVisible() &&
      !container_->layer()->GetAnimator()->is_animating()) {
    DCHECK(state_ == KeyboardControllerState::HIDING
           // TODO(oka): The state was LOADING_EXTENSION in
           // KeyboardControllerTest.CloseKeyboard. Check if it's expected and
           // resolve it if not.
           || state_ == KeyboardControllerState::LOADING_EXTENSION
           // TODO(oka): the state is HIDDEN in
           // VirtualKeyboardRootWindowControllerTest
           //     .EnsureCaretInWorkAreaWithMultipleDisplays.
           // Fix the test.
           || state_ == KeyboardControllerState::HIDDEN)
        << StateToStr(state_);
    return;
  }

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

  // Change the state here instead of at the end of the function because
  // the animation immediately starts and may finish before the end of the
  // function is reached.
  ChangeState(KeyboardControllerState::SHOWING);
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
  return weak_factory_will_hide_.HasWeakPtrs();
}

void KeyboardController::ShowAnimationFinished() {
  MarkKeyboardLoadFinished();

  // Notify observers after animation finished to prevent reveal desktop
  // background during animation.
  NotifyContentsBoundsChanging(container_->bounds());
  ui_->EnsureCaretInWorkArea();
  ChangeState(KeyboardControllerState::SHOWN);
}

void KeyboardController::HideAnimationFinished() {
  ui_->HideKeyboardContainer(container_.get());
  for (KeyboardControllerObserver& observer : observer_list_)
    observer.OnKeyboardHidden();
  ui_->EnsureCaretInWorkArea();
  ChangeState(KeyboardControllerState::HIDDEN);
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

void KeyboardController::CheckStateTransition(KeyboardControllerState prev,
                                              KeyboardControllerState next) {
  std::stringstream error_message;
  const bool valid_transition = isAllowedStateStansition(prev, next);
  if (!valid_transition)
    error_message << "Unexpected transition";
  // TODO(oka): Add more condition check.

  // Emit UMA
  const int transition_record =
      (valid_transition ? 1 : -1) *
      (static_cast<int>(prev) * 1000 + static_cast<int>(next));
  UMA_HISTOGRAM_SPARSE_SLOWLY("VirtualKeyboard.ControllerStateTransition",
                              transition_record);
  UMA_HISTOGRAM_BOOLEAN("VirtualKeyboard.ControllerStateTransitionIsValid",
                        transition_record > 0);

  // Crash on release build too for debug.
  // TODO(oka): Change this to DCHECK once enough data is collected.
  CHECK(error_message.str().empty())
      << "State: " << StateToStr(prev) << " -> " << StateToStr(next) << " "
      << error_message.str()
      << "  stack trace: " << base::debug::StackTrace(10).ToString();
}

void KeyboardController::ChangeState(KeyboardControllerState state) {
  CheckStateTransition(state_, state);
  if (state_ == state)
    return;

  state_ = state;
  for (KeyboardControllerObserver& observer : observer_list_)
    observer.OnStateChanged(state);

  weak_factory_report_lingering_state_.InvalidateWeakPtrs();

  switch (state_) {
    case KeyboardControllerState::LOADING_EXTENSION:
    case KeyboardControllerState::WILL_HIDE:
    case KeyboardControllerState::HIDING:
    case KeyboardControllerState::SHOWING:
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&KeyboardController::ReportLingeringState,
                     weak_factory_report_lingering_state_.GetWeakPtr()),
          base::TimeDelta::FromMilliseconds(kReportLingeringStateDelayMs));
      break;
    default:
      // Do nothing
      break;
  }
}

void KeyboardController::ReportLingeringState() {
  UMA_HISTOGRAM_ENUMERATION("VirtualKeyboard.LingeringIntermediateState",
                            state_, KeyboardControllerState::COUNT);
}

}  // namespace keyboard
