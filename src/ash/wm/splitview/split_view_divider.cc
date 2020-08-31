// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_divider.h"

#include <memory>

#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_divider_handler_view.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_util.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "ui/aura/scoped_window_targeter.h"
#include "ui/aura/window_targeter.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/view.h"
#include "ui/views/view_targeter_delegate.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

// The window targeter that is installed on the always on top container window
// when the split view mode is active.
class AlwaysOnTopWindowTargeter : public aura::WindowTargeter {
 public:
  explicit AlwaysOnTopWindowTargeter(aura::Window* divider_window)
      : divider_window_(divider_window) {}
  ~AlwaysOnTopWindowTargeter() override = default;

 private:
  bool GetHitTestRects(aura::Window* target,
                       gfx::Rect* hit_test_rect_mouse,
                       gfx::Rect* hit_test_rect_touch) const override {
    if (target == divider_window_) {
      *hit_test_rect_mouse = *hit_test_rect_touch = gfx::Rect(target->bounds());
      hit_test_rect_touch->Inset(
          gfx::Insets(-SplitViewDivider::kDividerEdgeInsetForTouch,
                      -SplitViewDivider::kDividerEdgeInsetForTouch));
      return true;
    }
    return aura::WindowTargeter::GetHitTestRects(target, hit_test_rect_mouse,
                                                 hit_test_rect_touch);
  }

  aura::Window* divider_window_;

  DISALLOW_COPY_AND_ASSIGN(AlwaysOnTopWindowTargeter);
};

// The divider view class. Passes the mouse/gesture events to the controller.
// Has two child views, one for the divider and one for its white handler. The
// bounds and transforms of these two child views can be affected by the
// spawning animation and by dragging, but regardless, the controller receives
// mouse/gesture events in the bounds of the |DividerView| object itself.
class DividerView : public views::View, public views::ViewTargeterDelegate {
 public:
  explicit DividerView(SplitViewController* controller,
                       SplitViewDivider* divider)
      : controller_(controller), divider_(divider) {
    divider_view_ = new views::View();
    divider_view_->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
    divider_view_->layer()->SetColor(kSplitviewDividerColor);

    divider_handler_view_ = new SplitViewDividerHandlerView();

    AddChildView(divider_view_);
    AddChildView(divider_handler_view_);

    SetEventTargeter(
        std::unique_ptr<views::ViewTargeter>(new views::ViewTargeter(this)));
  }
  ~DividerView() override = default;

  void DoSpawningAnimation(int spawn_position) {
    const gfx::Rect bounds = GetBoundsInScreen();
    int divider_signed_offset;
    // To animate the divider scaling up from nothing, animate its bounds rather
    // than its transform, mostly because a transform that scales by zero would
    // be singular. For that bounds animation, express |spawn_position| in local
    // coordinates by subtracting a coordinate of the origin. Compute
    // |divider_signed_offset| as described in the comment for
    // |SplitViewDividerHandlerView::DoSpawningAnimation|.
    if (IsCurrentScreenOrientationLandscape()) {
      divider_view_->SetBounds(spawn_position - bounds.x(), 0, 0,
                               bounds.height());
      divider_signed_offset = spawn_position - bounds.CenterPoint().x();
    } else {
      divider_view_->SetBounds(0, spawn_position - bounds.y(), bounds.width(),
                               0);
      divider_signed_offset = spawn_position - bounds.CenterPoint().y();
    }
    ui::LayerAnimator* divider_animator = divider_view_->layer()->GetAnimator();
    ui::ScopedLayerAnimationSettings settings(divider_animator);
    settings.SetTransitionDuration(kSplitviewDividerSpawnDuration);
    settings.SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);
    settings.SetPreemptionStrategy(ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);
    divider_animator->SchedulePauseForProperties(
        kSplitviewDividerSpawnDelay, ui::LayerAnimationElement::BOUNDS);
    divider_view_->SetBounds(0, 0, bounds.width(), bounds.height());
    divider_handler_view_->DoSpawningAnimation(divider_signed_offset);
  }

  // views::View:
  void Layout() override {
    // There is no divider in clamshell split view. If we are in clamshell mode,
    // then we must be transitioning from tablet mode, and the divider will be
    // destroyed, meaning there is no need to update it.
    if (!Shell::Get()->tablet_mode_controller()->InTabletMode())
      return;

    divider_view_->SetBoundsRect(GetLocalBounds());
    divider_handler_view_->Refresh(controller_->is_resizing());
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    gfx::Point location(event.location());
    views::View::ConvertPointToScreen(this, &location);
    controller_->StartResize(location);
    OnResizeStatusChanged();
    return true;
  }

  bool OnMouseDragged(const ui::MouseEvent& event) override {
    gfx::Point location(event.location());
    views::View::ConvertPointToScreen(this, &location);
    controller_->Resize(location);
    return true;
  }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    gfx::Point location(event.location());
    views::View::ConvertPointToScreen(this, &location);
    controller_->EndResize(location);
    OnResizeStatusChanged();
    if (event.GetClickCount() == 2)
      controller_->SwapWindows();
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    gfx::Point location(event->location());
    views::View::ConvertPointToScreen(this, &location);
    switch (event->type()) {
      case ui::ET_GESTURE_TAP:
        if (event->details().tap_count() == 2)
          controller_->SwapWindows();
        break;
      case ui::ET_GESTURE_TAP_DOWN:
      case ui::ET_GESTURE_SCROLL_BEGIN:
        controller_->StartResize(location);
        OnResizeStatusChanged();
        break;
      case ui::ET_GESTURE_SCROLL_UPDATE:
        controller_->Resize(location);
        break;
      case ui::ET_GESTURE_END:
        controller_->EndResize(location);
        OnResizeStatusChanged();
        break;
      default:
        break;
    }
    event->SetHandled();
  }

  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override {
    DCHECK_EQ(target, this);
    return true;
  }

 private:
  void OnResizeStatusChanged() {
    // It's possible that when this function is called, split view mode has
    // been ended, and the divider widget is to be deleted soon. In this case
    // no need to update the divider layout and do the animation.
    if (!controller_->InSplitViewMode())
      return;

    // If |divider_view_|'s bounds are animating, it is for the divider spawning
    // animation. Stop that before animating |divider_view_|'s transform.
    ui::LayerAnimator* divider_animator = divider_view_->layer()->GetAnimator();
    divider_animator->StopAnimatingProperty(ui::LayerAnimationElement::BOUNDS);

    // Do the divider enlarge/shrink animation when starting/ending dragging.
    divider_view_->SetBoundsRect(GetLocalBounds());
    const gfx::Rect old_bounds =
        divider_->GetDividerBoundsInScreen(/*is_dragging=*/false);
    const gfx::Rect new_bounds =
        divider_->GetDividerBoundsInScreen(controller_->is_resizing());
    gfx::Transform transform;
    transform.Translate(new_bounds.x() - old_bounds.x(),
                        new_bounds.y() - old_bounds.y());
    transform.Scale(
        static_cast<float>(new_bounds.width()) / old_bounds.width(),
        static_cast<float>(new_bounds.height()) / old_bounds.height());
    ui::ScopedLayerAnimationSettings settings(divider_animator);
    settings.SetTransitionDuration(
        kSplitviewDividerSelectionStatusChangeDuration);
    settings.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    divider_view_->SetTransform(transform);

    divider_handler_view_->Refresh(controller_->is_resizing());
  }

  views::View* divider_view_ = nullptr;
  SplitViewDividerHandlerView* divider_handler_view_ = nullptr;
  SplitViewController* controller_;
  SplitViewDivider* divider_;

  DISALLOW_COPY_AND_ASSIGN(DividerView);
};

}  // namespace

SplitViewDivider::SplitViewDivider(SplitViewController* controller)
    : controller_(controller) {
  Shell::Get()->activation_client()->AddObserver(this);
  CreateDividerWidget(controller);

  aura::Window* always_on_top_container = Shell::GetContainer(
      controller->root_window(), kShellWindowId_AlwaysOnTopContainer);
  split_view_window_targeter_ = std::make_unique<aura::ScopedWindowTargeter>(
      always_on_top_container, std::make_unique<AlwaysOnTopWindowTargeter>(
                                   divider_widget_->GetNativeWindow()));
}

SplitViewDivider::~SplitViewDivider() {
  Shell::Get()->activation_client()->RemoveObserver(this);
  divider_widget_->Close();
  split_view_window_targeter_.reset();
  for (auto* window : observed_windows_) {
    window->RemoveObserver(this);
    ::wm::TransientWindowManager::GetOrCreate(window)->RemoveObserver(this);
  }
  observed_windows_.clear();
}

// static
gfx::Rect SplitViewDivider::GetDividerBoundsInScreen(
    const gfx::Rect& work_area_bounds_in_screen,
    bool landscape,
    int divider_position,
    bool is_dragging) {
  const int dragging_diff = (kSplitviewDividerEnlargedShortSideLength -
                             kSplitviewDividerShortSideLength) /
                            2;
  if (landscape) {
    return is_dragging
               ? gfx::Rect(work_area_bounds_in_screen.x() + divider_position -
                               dragging_diff,
                           work_area_bounds_in_screen.y(),
                           kSplitviewDividerEnlargedShortSideLength,
                           work_area_bounds_in_screen.height())
               : gfx::Rect(work_area_bounds_in_screen.x() + divider_position,
                           work_area_bounds_in_screen.y(),
                           kSplitviewDividerShortSideLength,
                           work_area_bounds_in_screen.height());
  } else {
    return is_dragging
               ? gfx::Rect(work_area_bounds_in_screen.x(),
                           work_area_bounds_in_screen.y() + divider_position -
                               dragging_diff,
                           work_area_bounds_in_screen.width(),
                           kSplitviewDividerEnlargedShortSideLength)
               : gfx::Rect(work_area_bounds_in_screen.x(),
                           work_area_bounds_in_screen.y() + divider_position,
                           work_area_bounds_in_screen.width(),
                           kSplitviewDividerShortSideLength);
  }
}

void SplitViewDivider::DoSpawningAnimation(int spawning_position) {
  static_cast<DividerView*>(divider_widget_->GetContentsView())
      ->DoSpawningAnimation(spawning_position);
}

void SplitViewDivider::UpdateDividerBounds() {
  divider_widget_->SetBounds(GetDividerBoundsInScreen(/*is_dragging=*/false));
}

gfx::Rect SplitViewDivider::GetDividerBoundsInScreen(bool is_dragging) {
  const gfx::Rect work_area_bounds_in_screen =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          Shell::GetPrimaryRootWindow()->GetChildById(
              desks_util::GetActiveDeskContainerId()));
  const int divider_position = controller_->divider_position();
  const bool landscape = IsCurrentScreenOrientationLandscape();
  return GetDividerBoundsInScreen(work_area_bounds_in_screen, landscape,
                                  divider_position, is_dragging);
}

void SplitViewDivider::SetAlwaysOnTop(bool on_top) {
  if (on_top) {
    divider_widget_->SetZOrderLevel(ui::ZOrderLevel::kFloatingUIElement);

    // Special handling when put divider into always_on_top container. We want
    // to put it at the bottom so it won't block other always_on_top windows.
    aura::Window* always_on_top_container =
        Shell::GetContainer(divider_widget_->GetNativeWindow()->GetRootWindow(),
                            kShellWindowId_AlwaysOnTopContainer);
    always_on_top_container->StackChildAtBottom(
        divider_widget_->GetNativeWindow());
  } else {
    divider_widget_->SetZOrderLevel(ui::ZOrderLevel::kNormal);
  }
}

void SplitViewDivider::AddObservedWindow(aura::Window* window) {
  if (!base::Contains(observed_windows_, window)) {
    window->AddObserver(this);
    ::wm::TransientWindowManager::GetOrCreate(window)->AddObserver(this);
    observed_windows_.push_back(window);
  }
}

void SplitViewDivider::RemoveObservedWindow(aura::Window* window) {
  auto iter =
      std::find(observed_windows_.begin(), observed_windows_.end(), window);
  if (iter != observed_windows_.end()) {
    window->RemoveObserver(this);
    ::wm::TransientWindowManager::GetOrCreate(window)->RemoveObserver(this);
    observed_windows_.erase(iter);
  }
}

void SplitViewDivider::OnWindowDragStarted() {
  is_dragging_window_ = true;
  SetAlwaysOnTop(false);

  aura::Window* divider_window = divider_widget_->GetNativeWindow();
  divider_window->parent()->StackChildAtBottom(divider_window);
}

void SplitViewDivider::OnWindowDragEnded() {
  is_dragging_window_ = false;
  SetAlwaysOnTop(true);
}

void SplitViewDivider::OnWindowDestroying(aura::Window* window) {
  RemoveObservedWindow(window);
}

void SplitViewDivider::OnWindowBoundsChanged(aura::Window* window,
                                             const gfx::Rect& old_bounds,
                                             const gfx::Rect& new_bounds,
                                             ui::PropertyChangeReason reason) {
  if (!controller_->InSplitViewMode())
    return;

  // We only care about the bounds change of windows in
  // |transient_windows_observer_|.
  if (!transient_windows_observer_.IsObserving(window))
    return;

  // |window|'s transient parent must be one of the windows in
  // |observed_windows_|.
  aura::Window* transient_parent = nullptr;
  for (auto* observed_window : observed_windows_) {
    if (::wm::HasTransientAncestor(window, observed_window)) {
      transient_parent = observed_window;
      break;
    }
  }
  DCHECK(transient_parent);

  gfx::Rect transient_bounds = window->GetBoundsInScreen();
  transient_bounds.AdjustToFit(transient_parent->GetBoundsInScreen());
  window->SetBoundsInScreen(
      transient_bounds,
      display::Screen::GetScreen()->GetDisplayNearestWindow(window));
}

void SplitViewDivider::OnWindowActivated(ActivationReason reason,
                                         aura::Window* gained_active,
                                         aura::Window* lost_active) {
  if (!is_dragging_window_ &&
      (!gained_active || base::Contains(observed_windows_, gained_active))) {
    SetAlwaysOnTop(true);
  } else {
    // If |gained_active| is not one of the observed windows, or there is one
    // window that is currently being dragged, |divider_widget_| should not
    // be placed on top.
    SetAlwaysOnTop(false);
  }
}

void SplitViewDivider::OnTransientChildAdded(aura::Window* window,
                                             aura::Window* transient) {
  // For now, we only care about dialog bubbles type transient child. We may
  // observe other types transient child window as well if need arises in the
  // future.
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(transient);
  if (!widget || !widget->widget_delegate()->AsBubbleDialogDelegate())
    return;

  // At this moment, the transient window may not have the valid bounds yet.
  // Start observe the transient window.
  transient_windows_observer_.Add(transient);
}

void SplitViewDivider::OnTransientChildRemoved(aura::Window* window,
                                               aura::Window* transient) {
  if (transient_windows_observer_.IsObserving(transient))
    transient_windows_observer_.Remove(transient);
}

void SplitViewDivider::CreateDividerWidget(SplitViewController* controller) {
  DCHECK(!divider_widget_);
  // Native widget owns this widget.
  divider_widget_ = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kOpaque;
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  params.parent = Shell::GetContainer(controller->root_window(),
                                      kShellWindowId_AlwaysOnTopContainer);
  params.init_properties_container.SetProperty(kHideInDeskMiniViewKey, true);
  DividerView* divider_view = new DividerView(controller, this);
  divider_widget_->set_focus_on_creation(false);
  divider_widget_->Init(std::move(params));
  divider_widget_->SetVisibilityAnimationTransition(
      views::Widget::ANIMATE_NONE);
  divider_widget_->SetContentsView(divider_view);
  divider_widget_->SetBounds(GetDividerBoundsInScreen(false /* is_dragging */));
  divider_widget_->Show();
}

}  // namespace ash
