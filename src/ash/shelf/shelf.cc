// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf.h"

#include <memory>

#include "ash/animation/animation_change_type.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf_controller.h"
#include "ash/shelf/shelf_focus_cycler.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_layout_manager_observer.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_observer.h"
#include "ash/shelf/shelf_tooltip_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/work_area_insets.h"
#include "base/bind_helpers.h"
#include "base/check.h"
#include "base/notreached.h"
#include "ui/compositor/animation_metrics_reporter.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/rect.h"

namespace {

bool IsAppListBackground(ash::ShelfBackgroundType background_type) {
  switch (background_type) {
    case ash::ShelfBackgroundType::kAppList:
    case ash::ShelfBackgroundType::kHomeLauncher:
    case ash::ShelfBackgroundType::kMaximizedWithAppList:
      return true;
    case ash::ShelfBackgroundType::kDefaultBg:
    case ash::ShelfBackgroundType::kMaximized:
    case ash::ShelfBackgroundType::kOobe:
    case ash::ShelfBackgroundType::kLogin:
    case ash::ShelfBackgroundType::kLoginNonBlurredWallpaper:
    case ash::ShelfBackgroundType::kOverview:
    case ash::ShelfBackgroundType::kInApp:
      return false;
  }
}

}  // namespace

namespace ash {

// Records smoothness of bounds animations for the HotseatWidget.
class HotseatWidgetAnimationMetricsReporter
    : public ui::AnimationMetricsReporter {
 public:
  // The different kinds of hotseat elements.
  enum class HotseatElementType {
    // The Hotseat Widget.
    kWidget,
    // The Hotseat Widget's translucent background.
    kTranslucentBackground
  };
  explicit HotseatWidgetAnimationMetricsReporter(
      HotseatElementType hotseat_element)
      : hotseat_element_(hotseat_element) {}
  ~HotseatWidgetAnimationMetricsReporter() override = default;

  void SetTargetHotseatState(HotseatState target_state) {
    target_state_ = target_state;
  }

  // ui::AnimationMetricsReporter:
  void Report(int value) override {
    switch (target_state_) {
      case HotseatState::kShownClamshell:
      case HotseatState::kShownHomeLauncher:
        if (hotseat_element_ == HotseatElementType::kWidget) {
          UMA_HISTOGRAM_PERCENTAGE(
              "Ash.HotseatWidgetAnimation.Widget.AnimationSmoothness."
              "TransitionToShownHotseat",
              value);
        } else {
          UMA_HISTOGRAM_PERCENTAGE(
              "Ash.HotseatWidgetAnimation.TranslucentBackground."
              "AnimationSmoothness.TransitionToShownHotseat",
              value);
        }
        break;
      case HotseatState::kExtended:
        if (hotseat_element_ == HotseatElementType::kWidget) {
          UMA_HISTOGRAM_PERCENTAGE(
              "Ash.HotseatWidgetAnimation.Widget.AnimationSmoothness."
              "TransitionToExtendedHotseat",
              value);
        } else {
          UMA_HISTOGRAM_PERCENTAGE(
              "Ash.HotseatWidgetAnimation.TranslucentBackground."
              "AnimationSmoothness.TransitionToExtendedHotseat",
              value);
        }
        break;
      case HotseatState::kHidden:
        if (hotseat_element_ == HotseatElementType::kWidget) {
          UMA_HISTOGRAM_PERCENTAGE(
              "Ash.HotseatWidgetAnimation.Widget.AnimationSmoothness."
              "TransitionToHiddenHotseat",
              value);
        } else {
          UMA_HISTOGRAM_PERCENTAGE(
              "Ash.HotseatWidgetAnimation.TranslucentBackground."
              "AnimationSmoothness.TransitionToHiddenHotseat",
              value);
        }
        break;
      default:
        NOTREACHED();
    }
  }

 private:
  // The element that is reporting an animation.
  HotseatElementType hotseat_element_;
  // The state to which the animation is transitioning.
  HotseatState target_state_ = HotseatState::kHidden;
};

// An animation metrics reporter for the shelf navigation widget.
class ASH_EXPORT NavigationWidgetAnimationMetricsReporter
    : public ui::AnimationMetricsReporter,
      public ShelfLayoutManagerObserver {
 public:
  explicit NavigationWidgetAnimationMetricsReporter(Shelf* shelf)
      : shelf_(shelf) {
    shelf_->shelf_layout_manager()->AddObserver(this);
  }

  ~NavigationWidgetAnimationMetricsReporter() override {
    shelf_->shelf_layout_manager()->RemoveObserver(this);
  }

  NavigationWidgetAnimationMetricsReporter(
      const NavigationWidgetAnimationMetricsReporter&) = delete;
  NavigationWidgetAnimationMetricsReporter& operator=(
      const NavigationWidgetAnimationMetricsReporter&) = delete;

  // ui::AnimationMetricsReporter:
  void Report(int value) override {
    switch (target_state_) {
      case HotseatState::kShownClamshell:
      case HotseatState::kShownHomeLauncher:
        UMA_HISTOGRAM_PERCENTAGE(
            "Ash.NavigationWidget.Widget.AnimationSmoothness."
            "TransitionToShownHotseat",
            value);
        break;
      case HotseatState::kExtended:
        UMA_HISTOGRAM_PERCENTAGE(
            "Ash.NavigationWidget.Widget.AnimationSmoothness."
            "TransitionToExtendedHotseat",
            value);
        break;
      case HotseatState::kHidden:
        UMA_HISTOGRAM_PERCENTAGE(
            "Ash.NavigationWidget.Widget.AnimationSmoothness."
            "TransitionToHiddenHotseat",
            value);
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  // ShelfLayoutManagerObserver:
  void OnHotseatStateChanged(HotseatState old_state,
                             HotseatState new_state) override {
    target_state_ = new_state;
  }

 private:
  Shelf* shelf_;
  // The state to which the animation is transitioning.
  HotseatState target_state_ = HotseatState::kShownHomeLauncher;
};

// Shelf::AutoHideEventHandler -----------------------------------------------

// Forwards mouse and gesture events to ShelfLayoutManager for auto-hide.
class Shelf::AutoHideEventHandler : public ui::EventHandler {
 public:
  explicit AutoHideEventHandler(Shelf* shelf) : shelf_(shelf) {
    Shell::Get()->AddPreTargetHandler(this);
  }

  ~AutoHideEventHandler() override {
    Shell::Get()->RemovePreTargetHandler(this);
  }

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override {
    shelf_->shelf_layout_manager()->UpdateAutoHideForMouseEvent(
        event, static_cast<aura::Window*>(event->target()));
  }
  void OnGestureEvent(ui::GestureEvent* event) override {
    shelf_->shelf_layout_manager()->ProcessGestureEventOfAutoHideShelf(
        event, static_cast<aura::Window*>(event->target()));
  }
  void OnTouchEvent(ui::TouchEvent* event) override {
    if (shelf_->auto_hide_behavior() != ShelfAutoHideBehavior::kAlways)
      return;

    // The event target should be the shelf widget or the hotseat widget.
    if (!shelf_->shelf_layout_manager()->IsShelfWindow(
            static_cast<aura::Window*>(event->target()))) {
      return;
    }

    // The touch-pressing event may hide the shelf. Lock the shelf's auto hide
    // state to give the shelf a chance to handle the touch event before it
    // being hidden.
    ShelfLayoutManager* shelf_layout_manager = shelf_->shelf_layout_manager();
    if (event->type() == ui::ET_TOUCH_PRESSED && shelf_->IsVisible()) {
      shelf_layout_manager->LockAutoHideState(true);
    } else if (event->type() == ui::ET_TOUCH_RELEASED ||
               event->type() == ui::ET_TOUCH_CANCELLED) {
      shelf_layout_manager->LockAutoHideState(false);
    }
  }

 private:
  Shelf* shelf_;
  DISALLOW_COPY_AND_ASSIGN(AutoHideEventHandler);
};

// Shelf::AutoDimEventHandler -----------------------------------------------

// Handles mouse and touch events and determines whether ShelfLayoutManager
// should update shelf opacity for auto-dimming.
class Shelf::AutoDimEventHandler : public ui::EventHandler,
                                   public ShelfObserver {
 public:
  explicit AutoDimEventHandler(Shelf* shelf) : shelf_(shelf) {
    Shell::Get()->AddPreTargetHandler(this);
    shelf_observer_.Add(shelf_);
    UndimShelf();
  }

  ~AutoDimEventHandler() override {
    Shell::Get()->RemovePreTargetHandler(this);
  }

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override {
    if (shelf_->shelf_layout_manager()->IsShelfWindow(
            static_cast<aura::Window*>(event->target()))) {
      UndimShelf();
    }
  }

  void OnTouchEvent(ui::TouchEvent* event) override {
    if (shelf_->shelf_layout_manager()->IsShelfWindow(
            static_cast<aura::Window*>(event->target()))) {
      UndimShelf();
    }
  }

  void StartDimShelfTimer() {
    dim_shelf_timer_.Start(
        FROM_HERE, kDimDelay,
        base::BindOnce(&AutoDimEventHandler::DimShelf, base::Unretained(this)));
  }

  void DimShelf() {
    // Attempt to dim the shelf. Stop the |dim_shelf_timer_| if successful.
    if (shelf_->shelf_layout_manager()->SetDimmed(true))
      dim_shelf_timer_.Stop();
  }

  // Sets shelf as active and sets timer to mark shelf as inactive.
  void UndimShelf() {
    shelf_->shelf_layout_manager()->SetDimmed(false);
    StartDimShelfTimer();
  }

  bool HasDimShelfTimer() { return dim_shelf_timer_.IsRunning(); }

  // ShelfObserver:
  void OnAutoHideStateChanged(ShelfAutoHideState new_state) override {
    // Shelf should be undimmed when it is shown.
    if (new_state == ShelfAutoHideState::SHELF_AUTO_HIDE_SHOWN)
      UndimShelf();
  }

  // ShelfObserver:
  void WillChangeVisibilityState(ShelfVisibilityState new_state) override {
    // Shelf should be undimmed when it is shown.
    if (new_state != ShelfVisibilityState::SHELF_HIDDEN)
      UndimShelf();
  }

 private:
  // Unowned pointer to the shelf that owns this event handler.
  Shelf* shelf_;
  // OneShotTimer that dims shelf due to inactivity.
  base::OneShotTimer dim_shelf_timer_;
  // An observer that notifies the AutoDimHandler that shelf visibility has
  // changed.
  ScopedObserver<Shelf, ShelfObserver> shelf_observer_{this};

  // Delay before dimming the shelf.
  const base::TimeDelta kDimDelay = base::TimeDelta::FromSeconds(5);

  DISALLOW_COPY_AND_ASSIGN(AutoDimEventHandler);
};

// Shelf ---------------------------------------------------------------------

Shelf::Shelf()
    : shelf_locking_manager_(this),
      shelf_focus_cycler_(std::make_unique<ShelfFocusCycler>(this)),
      tooltip_(std::make_unique<ShelfTooltipManager>(this)) {}

Shelf::~Shelf() = default;

// static
Shelf* Shelf::ForWindow(aura::Window* window) {
  return RootWindowController::ForWindow(window)->shelf();
}

// static
void Shelf::LaunchShelfItem(int item_index) {
  const int item_count = ShelfModel::Get()->item_count();

  // A negative argument will launch the last app. A positive argument will
  // launch the app at the corresponding index, unless it's higher than the
  // total number of apps, in which case we do nothing.
  if (item_index >= item_count)
    return;

  const int found_index = item_index >= 0 ? item_index : item_count - 1;

  // Set this one as active (or advance to the next item of its kind).
  ActivateShelfItem(found_index);
}

// static
void Shelf::ActivateShelfItem(int item_index) {
  ActivateShelfItemOnDisplay(item_index, display::kInvalidDisplayId);
}

// static
void Shelf::ActivateShelfItemOnDisplay(int item_index, int64_t display_id) {
  const ShelfModel* shelf_model = ShelfModel::Get();
  const ShelfItem& item = shelf_model->items()[item_index];
  ShelfItemDelegate* item_delegate = shelf_model->GetShelfItemDelegate(item.id);
  std::unique_ptr<ui::Event> event = std::make_unique<ui::KeyEvent>(
      ui::ET_KEY_RELEASED, ui::VKEY_UNKNOWN, ui::EF_NONE);
  item_delegate->ItemSelected(std::move(event), display_id, LAUNCH_FROM_SHELF,
                              base::DoNothing());
}

void Shelf::CreateNavigationWidget(aura::Window* container) {
  DCHECK(container);
  DCHECK(!navigation_widget_);
  navigation_widget_ = std::make_unique<ShelfNavigationWidget>(
      this, hotseat_widget()->GetShelfView());
  navigation_widget_->Initialize(container);
  navigation_widget_metrics_reporter_ =
      std::make_unique<NavigationWidgetAnimationMetricsReporter>(this);
}

void Shelf::CreateHotseatWidget(aura::Window* container) {
  DCHECK(container);
  DCHECK(!hotseat_widget_);
  hotseat_widget_ = std::make_unique<HotseatWidget>();
  translucent_background_metrics_reporter_ =
      std::make_unique<HotseatWidgetAnimationMetricsReporter>(
          HotseatWidgetAnimationMetricsReporter::HotseatElementType::
              kTranslucentBackground);
  hotseat_widget_->Initialize(container, this);
  shelf_widget_->RegisterHotseatWidget(hotseat_widget());
  hotseat_transition_metrics_reporter_ =
      std::make_unique<HotseatWidgetAnimationMetricsReporter>(
          HotseatWidgetAnimationMetricsReporter::HotseatElementType::kWidget);
}

void Shelf::CreateStatusAreaWidget(aura::Window* status_container) {
  DCHECK(status_container);
  DCHECK(!status_area_widget_);
  status_area_widget_ =
      std::make_unique<StatusAreaWidget>(status_container, this);
  status_area_widget_->Initialize();
}

void Shelf::CreateShelfWidget(aura::Window* root) {
  DCHECK(!shelf_widget_);
  aura::Window* shelf_container =
      root->GetChildById(kShellWindowId_ShelfContainer);
  shelf_widget_.reset(new ShelfWidget(this));

  DCHECK(!shelf_layout_manager_);
  shelf_layout_manager_ = shelf_widget_->shelf_layout_manager();
  shelf_layout_manager_->AddObserver(this);

  // Create the various shelf components.
  CreateHotseatWidget(shelf_container);
  CreateNavigationWidget(shelf_container);

  // Must occur after |shelf_widget_| is constructed because the system tray
  // constructors call back into Shelf::shelf_widget().
  CreateStatusAreaWidget(shelf_container);
  shelf_widget_->Initialize(shelf_container);
  shelf_widget_->GetNativeWindow()->parent()->StackChildAtBottom(
      shelf_widget_->GetNativeWindow());

  // The Hotseat should be above everything in the shelf.
  hotseat_widget()->StackAtTop();
}

void Shelf::ShutdownShelfWidget() {
  // The contents view of the hotseat widget may rely on the status area widget.
  // So do explicit destruction here.
  hotseat_widget_.reset();
  status_area_widget_.reset();
  navigation_widget_.reset();

  shelf_widget_->Shutdown();
}

void Shelf::DestroyShelfWidget() {
  DCHECK(shelf_widget_);
  shelf_widget_.reset();
}

bool Shelf::IsVisible() const {
  return shelf_layout_manager_->IsVisible();
}

const aura::Window* Shelf::GetWindow() const {
  return shelf_widget_ ? shelf_widget_->GetNativeWindow() : nullptr;
}

aura::Window* Shelf::GetWindow() {
  return const_cast<aura::Window*>(const_cast<const Shelf*>(this)->GetWindow());
}

void Shelf::SetAlignment(ShelfAlignment alignment) {
  if (!shelf_widget_)
    return;

  if (alignment_ == alignment)
    return;

  if (shelf_locking_manager_.is_locked() &&
      alignment != ShelfAlignment::kBottomLocked) {
    shelf_locking_manager_.set_stored_alignment(alignment);
    return;
  }

  ShelfAlignment old_alignment = alignment_;
  alignment_ = alignment;
  tooltip_->Close();
  shelf_layout_manager_->LayoutShelf();
  Shell::Get()->NotifyShelfAlignmentChanged(GetWindow()->GetRootWindow(),
                                            old_alignment);
}

bool Shelf::IsHorizontalAlignment() const {
  switch (alignment_) {
    case ShelfAlignment::kBottom:
    case ShelfAlignment::kBottomLocked:
      return true;
    case ShelfAlignment::kLeft:
    case ShelfAlignment::kRight:
      return false;
  }
  NOTREACHED();
  return true;
}

void Shelf::SetAutoHideBehavior(ShelfAutoHideBehavior auto_hide_behavior) {
  DCHECK(shelf_layout_manager_);

  if (auto_hide_behavior_ == auto_hide_behavior)
    return;

  auto_hide_behavior_ = auto_hide_behavior;
  Shell::Get()->NotifyShelfAutoHideBehaviorChanged(
      GetWindow()->GetRootWindow());
}

ShelfAutoHideState Shelf::GetAutoHideState() const {
  return shelf_layout_manager_->auto_hide_state();
}

void Shelf::UpdateAutoHideState() {
  shelf_layout_manager_->UpdateAutoHideState();
}

ShelfBackgroundType Shelf::GetBackgroundType() const {
  return shelf_widget_ ? shelf_widget_->GetBackgroundType()
                       : ShelfBackgroundType::kDefaultBg;
}

void Shelf::UpdateVisibilityState() {
  if (shelf_layout_manager_)
    shelf_layout_manager_->UpdateVisibilityState();
}

void Shelf::MaybeUpdateShelfBackground() {
  if (!shelf_layout_manager_)
    return;

  shelf_layout_manager_->MaybeUpdateShelfBackground(
      AnimationChangeType::ANIMATE);
}

ShelfVisibilityState Shelf::GetVisibilityState() const {
  return shelf_layout_manager_ ? shelf_layout_manager_->visibility_state()
                               : SHELF_HIDDEN;
}

gfx::Rect Shelf::GetShelfBoundsInScreen() const {
  return shelf_widget()->GetTargetBounds();
}

gfx::Rect Shelf::GetIdealBounds() const {
  return shelf_layout_manager_->GetIdealBounds();
}

gfx::Rect Shelf::GetIdealBoundsForWorkAreaCalculation() {
  return shelf_layout_manager_->GetIdealBoundsForWorkAreaCalculation();
}

gfx::Rect Shelf::GetScreenBoundsOfItemIconForWindow(aura::Window* window) {
  if (!shelf_widget_)
    return gfx::Rect();
  return shelf_widget_->GetScreenBoundsOfItemIconForWindow(window);
}

bool Shelf::ProcessGestureEvent(const ui::GestureEvent& event) {
  // Can be called at login screen.
  if (!shelf_layout_manager_)
    return false;
  return shelf_layout_manager_->ProcessGestureEvent(event);
}

void Shelf::ProcessMouseEvent(const ui::MouseEvent& event) {
  if (shelf_layout_manager_)
    shelf_layout_manager_->ProcessMouseEventFromShelf(event);
}

void Shelf::ProcessScrollEvent(ui::ScrollEvent* event) {
  if (event->finger_count() == 2 && event->type() == ui::ET_SCROLL) {
    ui::MouseWheelEvent wheel(*event);
    ProcessMouseWheelEvent(&wheel, /*from_touchpad=*/true);
    event->SetHandled();
  }
}

void Shelf::ProcessMouseWheelEvent(ui::MouseWheelEvent* event,
                                   bool from_touchpad) {
  event->SetHandled();

  // Early out if not in active session. Code below assumes that there is
  // an active user (shelf layout manager looks up active user's scroll
  // preferences) and crashes without this.
  if (!shelf_layout_manager_->is_active_session_state())
    return;

  if (!IsHorizontalAlignment())
    return;
  auto* app_list_controller = Shell::Get()->app_list_controller();
  DCHECK(app_list_controller);
  // If the App List is not visible, send MouseWheel events to the
  // |shelf_layout_manager_| because these events are used to show the App List.
  if (app_list_controller->IsVisible(shelf_layout_manager_->display_.id())) {
    app_list_controller->ProcessMouseWheelEvent(*event);
  } else {
    shelf_layout_manager_->ProcessMouseWheelEventFromShelf(event,
                                                           from_touchpad);
  }
}

void Shelf::AddObserver(ShelfObserver* observer) {
  observers_.AddObserver(observer);
}

void Shelf::RemoveObserver(ShelfObserver* observer) {
  observers_.RemoveObserver(observer);
}

void Shelf::NotifyShelfIconPositionsChanged() {
  for (auto& observer : observers_)
    observer.OnShelfIconPositionsChanged();
}

StatusAreaWidget* Shelf::GetStatusAreaWidget() const {
  return shelf_widget_ ? shelf_widget_->status_area_widget() : nullptr;
}

TrayBackgroundView* Shelf::GetSystemTrayAnchorView() const {
  return GetStatusAreaWidget()->GetSystemTrayAnchor();
}

gfx::Rect Shelf::GetSystemTrayAnchorRect() const {
  gfx::Rect work_area = GetWorkAreaInsets()->user_work_area_bounds();
  switch (alignment_) {
    case ShelfAlignment::kBottom:
    case ShelfAlignment::kBottomLocked:
      return gfx::Rect(
          base::i18n::IsRTL() ? work_area.x() : work_area.right() - 1,
          work_area.bottom() - 1, 0, 0);
    case ShelfAlignment::kLeft:
      return gfx::Rect(work_area.x(), work_area.bottom() - 1, 0, 0);
    case ShelfAlignment::kRight:
      return gfx::Rect(work_area.right() - 1, work_area.bottom() - 1, 0, 0);
  }
  NOTREACHED();
  return gfx::Rect();
}

bool Shelf::ShouldHideOnSecondaryDisplay(session_manager::SessionState state) {
  if (Shell::GetPrimaryRootWindowController()->shelf() == this)
    return false;

  return state != session_manager::SessionState::ACTIVE;
}

void Shelf::SetVirtualKeyboardBoundsForTesting(const gfx::Rect& bounds) {
  KeyboardStateDescriptor state;
  state.is_visible = !bounds.IsEmpty();
  state.visual_bounds = bounds;
  state.occluded_bounds_in_screen = bounds;
  state.displaced_bounds_in_screen = gfx::Rect();
  WorkAreaInsets* work_area_insets = GetWorkAreaInsets();
  work_area_insets->OnKeyboardVisibilityChanged(state.is_visible);
  work_area_insets->OnKeyboardVisibleBoundsChanged(state.visual_bounds);
  work_area_insets->OnKeyboardOccludedBoundsChanged(
      state.occluded_bounds_in_screen);
  work_area_insets->OnKeyboardDisplacingBoundsChanged(
      state.displaced_bounds_in_screen);
  work_area_insets->OnKeyboardAppearanceChanged(state);
}

ShelfLockingManager* Shelf::GetShelfLockingManagerForTesting() {
  return &shelf_locking_manager_;
}

ShelfView* Shelf::GetShelfViewForTesting() {
  return shelf_widget_->shelf_view_for_testing();
}

ui::AnimationMetricsReporter* Shelf::GetHotseatTransitionMetricsReporter(
    HotseatState target_state) {
  hotseat_transition_metrics_reporter_->SetTargetHotseatState(target_state);
  return hotseat_transition_metrics_reporter_.get();
}

ui::AnimationMetricsReporter* Shelf::GetTranslucentBackgroundMetricsReporter(
    HotseatState target_state) {
  translucent_background_metrics_reporter_->SetTargetHotseatState(target_state);
  return translucent_background_metrics_reporter_.get();
}

ui::AnimationMetricsReporter*
Shelf::GetNavigationWidgetAnimationMetricsReporter() {
  return navigation_widget_metrics_reporter_.get();
}

void Shelf::WillDeleteShelfLayoutManager() {
  // Clear event handlers that might forward events to the destroyed instance.
  auto_hide_event_handler_.reset();
  auto_dim_event_handler_.reset();
  navigation_widget_metrics_reporter_.reset();

  DCHECK(shelf_layout_manager_);
  shelf_layout_manager_->RemoveObserver(this);
  shelf_layout_manager_ = nullptr;
}

void Shelf::WillChangeVisibilityState(ShelfVisibilityState new_state) {
  for (auto& observer : observers_)
    observer.WillChangeVisibilityState(new_state);
  if (new_state != SHELF_AUTO_HIDE) {
    auto_hide_event_handler_.reset();
  } else if (!auto_hide_event_handler_) {
    auto_hide_event_handler_ = std::make_unique<AutoHideEventHandler>(this);
  }

  if (!auto_dim_event_handler_ && switches::IsUsingShelfAutoDim()) {
    auto_dim_event_handler_ = std::make_unique<AutoDimEventHandler>(this);
  }
}

void Shelf::OnAutoHideStateChanged(ShelfAutoHideState new_state) {
  for (auto& observer : observers_)
    observer.OnAutoHideStateChanged(new_state);
}

void Shelf::OnBackgroundUpdated(ShelfBackgroundType background_type,
                                AnimationChangeType change_type) {
  if (background_type == GetBackgroundType())
    return;

  // Shelf should undim when transitioning to show app list.
  if (auto_dim_event_handler_ && IsAppListBackground(background_type))
    UndimShelf();

  for (auto& observer : observers_)
    observer.OnBackgroundTypeChanged(background_type, change_type);
}

void Shelf::OnHotseatStateChanged(HotseatState old_state,
                                  HotseatState new_state) {
  for (auto& observer : observers_)
    observer.OnHotseatStateChanged(old_state, new_state);
}

void Shelf::OnWorkAreaInsetsChanged() {
  for (auto& observer : observers_)
    observer.OnShelfWorkAreaInsetsChanged();
}

void Shelf::DimShelf() {
  auto_dim_event_handler_->DimShelf();
}

void Shelf::UndimShelf() {
  auto_dim_event_handler_->UndimShelf();
}

bool Shelf::HasDimShelfTimer() {
  return auto_dim_event_handler_->HasDimShelfTimer();
}

WorkAreaInsets* Shelf::GetWorkAreaInsets() const {
  const aura::Window* window = GetWindow();
  DCHECK(window);
  return WorkAreaInsets::ForWindow(window->GetRootWindow());
}

}  // namespace ash
