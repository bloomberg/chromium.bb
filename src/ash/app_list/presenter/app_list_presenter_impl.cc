// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/presenter/app_list_presenter_impl.h"

#include <utility>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/pagination_model.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/views/widget/widget.h"

namespace app_list {
namespace {

// The y offset for app list animation when overview mode toggles.
constexpr int kOverviewAnimationYOffset = 100;

// The duration in milliseconds for app list animation when overview mode
// toggles.
constexpr base::TimeDelta kOverviewAnimationDuration =
    base::TimeDelta::FromMilliseconds(250);

inline ui::Layer* GetLayer(views::Widget* widget) {
  return widget->GetNativeView()->layer();
}

void UpdateOverviewSettings(ui::AnimationMetricsReporter* reporter,
                            ui::ScopedLayerAnimationSettings* settings,
                            bool observe) {
  settings->SetTransitionDuration(kOverviewAnimationDuration);
  settings->SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
  settings->SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

  DCHECK(reporter);
  settings->SetAnimationMetricsReporter(reporter);
}

// Callback from the compositor when it presented a valid frame. Used to
// record UMA of input latency.
void DidPresentCompositorFrame(base::TimeTicks event_time_stamp,
                               bool is_showing,
                               const gfx::PresentationFeedback& feedback) {
  const base::TimeTicks present_time = feedback.timestamp;
  if (present_time.is_null() || event_time_stamp.is_null() ||
      present_time < event_time_stamp) {
    return;
  }
  const base::TimeDelta input_latency = present_time - event_time_stamp;
  if (is_showing) {
    UMA_HISTOGRAM_TIMES(kAppListShowInputLatencyHistogram, input_latency);
  } else {
    UMA_HISTOGRAM_TIMES(kAppListHideInputLatencyHistogram, input_latency);
  }
}

}  // namespace

class AppListPresenterImpl::OverviewAnimationMetricsReporter
    : public ui::AnimationMetricsReporter {
 public:
  OverviewAnimationMetricsReporter() = default;
  ~OverviewAnimationMetricsReporter() override = default;

  void Start(bool enter) {
    enter_ = enter;
  }

  void Report(int value) override {
    if (enter_) {
      UMA_HISTOGRAM_PERCENTAGE(
          "Apps.StateTransition.AnimationSmoothness.EnterOverview", value);
    } else {
      UMA_HISTOGRAM_PERCENTAGE(
          "Apps.StateTransition.AnimationSmoothness.ExitOverview", value);
    }
  }

 private:
  bool enter_ = false;

  DISALLOW_COPY_AND_ASSIGN(OverviewAnimationMetricsReporter);
};

AppListPresenterImpl::AppListPresenterImpl(
    std::unique_ptr<AppListPresenterDelegate> delegate)
    : delegate_(std::move(delegate)),
      overview_animation_metrics_reporter_(
          std::make_unique<OverviewAnimationMetricsReporter>()) {
  DCHECK(delegate_);
  delegate_->SetPresenter(this);
}

AppListPresenterImpl::~AppListPresenterImpl() {
  Dismiss(base::TimeTicks());
  // Ensures app list view goes before the controller since pagination model
  // lives in the controller and app list view would access it on destruction.
  if (view_) {
    view_->GetAppsPaginationModel()->RemoveObserver(this);
    if (view_->GetWidget())
      view_->GetWidget()->CloseNow();
  }
}

aura::Window* AppListPresenterImpl::GetWindow() {
  return is_visible_ && view_ ? view_->GetWidget()->GetNativeWindow() : nullptr;
}

void AppListPresenterImpl::Show(int64_t display_id,
                                base::TimeTicks event_time_stamp) {
  if (is_visible_) {
    // Launcher is always visible on the internal display when home launcher is
    // enabled in tablet mode.
    if (display_id != GetDisplayId() && !delegate_->IsTabletMode()) {
      Dismiss(event_time_stamp);
    }
    return;
  }

  if (!delegate_->GetRootWindowForDisplayId(display_id)) {
    LOG(ERROR) << "Root window does not exist for display: " << display_id;
    return;
  }

  is_visible_ = true;
  RequestPresentationTime(display_id, event_time_stamp);

  if (!view_) {
    // Note |delegate_| outlives the AppListView. For Ash, the view
    // is destroyed when dismissed.
    AppListView* view = new AppListView(delegate_->GetAppListViewDelegate());
    delegate_->Init(view, display_id, current_apps_page_);
    SetView(view);
  }
  view_->ShowWhenReady();
  delegate_->OnShown(display_id);
  NotifyTargetVisibilityChanged(GetTargetVisibility());
  NotifyVisibilityChanged(GetTargetVisibility(), display_id);
}

void AppListPresenterImpl::Dismiss(base::TimeTicks event_time_stamp) {
  if (!is_visible_)
    return;

  // If the app list is currently visible, there should be an existing view.
  DCHECK(view_);

  is_visible_ = false;
  const int64_t display_id = GetDisplayId();
  RequestPresentationTime(display_id, event_time_stamp);
  // The dismissal may have occurred in response to the app list losing
  // activation. Otherwise, our widget is currently active. When the animation
  // completes we'll hide the widget, changing activation. If a menu is shown
  // before the animation completes then the activation change triggers the menu
  // to close. By deactivating now we ensure there is no activation change when
  // the animation completes and any menus stay open.
  if (view_->GetWidget()->IsActive())
    view_->GetWidget()->Deactivate();

  delegate_->OnClosing();
  ScheduleAnimation();
  NotifyTargetVisibilityChanged(GetTargetVisibility());
  base::RecordAction(base::UserMetricsAction("Launcher_Dismiss"));
}

bool AppListPresenterImpl::CloseOpenedPage() {
  if (!is_visible_)
    return false;

  // If the app list is currently visible, there should be an existing view.
  DCHECK(view_);

  return view_->CloseOpenedPage();
}

ash::ShelfAction AppListPresenterImpl::ToggleAppList(
    int64_t display_id,
    app_list::AppListShowSource show_source,
    base::TimeTicks event_time_stamp) {
  bool request_fullscreen = show_source == kSearchKeyFullscreen ||
                            show_source == kShelfButtonFullscreen;
  if (IsVisible()) {
    if (request_fullscreen) {
      if (view_->app_list_state() == AppListViewState::PEEKING) {
        view_->SetState(AppListViewState::FULLSCREEN_ALL_APPS);
        return ash::SHELF_ACTION_APP_LIST_SHOWN;
      } else if (view_->app_list_state() == AppListViewState::HALF) {
        view_->SetState(AppListViewState::FULLSCREEN_SEARCH);
        return ash::SHELF_ACTION_APP_LIST_SHOWN;
      }
    }
    Dismiss(event_time_stamp);
    return ash::SHELF_ACTION_APP_LIST_DISMISSED;
  }
  Show(display_id, event_time_stamp);
  if (request_fullscreen)
    view_->SetState(AppListViewState::FULLSCREEN_ALL_APPS);
  return ash::SHELF_ACTION_APP_LIST_SHOWN;
}

bool AppListPresenterImpl::IsVisible() const {
  return view_ && view_->GetWidget()->IsVisible();
}

bool AppListPresenterImpl::GetTargetVisibility() const {
  return is_visible_;
}

void AppListPresenterImpl::UpdateYPositionAndOpacity(int y_position_in_screen,
                                                     float background_opacity) {
  if (!is_visible_)
    return;

  if (view_)
    view_->UpdateYPositionAndOpacity(y_position_in_screen, background_opacity);
}

void AppListPresenterImpl::EndDragFromShelf(AppListViewState app_list_state) {
  if (view_) {
    if (app_list_state == AppListViewState::CLOSED ||
        view_->app_list_state() == AppListViewState::CLOSED) {
      view_->Dismiss();
    } else {
      view_->SetState(AppListViewState(app_list_state));
    }
    view_->SetIsInDrag(false);
    view_->UpdateChildViewsYPositionAndOpacity();
  }
}

void AppListPresenterImpl::ProcessMouseWheelOffset(
    const gfx::Vector2d& scroll_offset_vector) {
  if (view_)
    view_->HandleScroll(scroll_offset_vector, ui::ET_MOUSEWHEEL);
}

void AppListPresenterImpl::UpdateYPositionAndOpacityForHomeLauncher(
    int y_position_in_screen,
    float opacity,
    UpdateHomeLauncherAnimationSettingsCallback callback) {
  if (!GetTargetVisibility())
    return;

  const gfx::Transform translation(1.f, 0.f, 0.f, 1.f, 0.f,
                                   static_cast<float>(y_position_in_screen));
  // We want to animate the expand arrow, suggestion chips and apps grid in
  // app_list_main_view, and the search box.
  ui::Layer* layer = view_->GetWidget()->GetNativeWindow()->layer();
  layer->GetAnimator()->StopAnimating();
  std::unique_ptr<ui::ScopedLayerAnimationSettings> settings;
  if (!callback.is_null()) {
    settings = std::make_unique<ui::ScopedLayerAnimationSettings>(
        layer->GetAnimator());
    callback.Run(settings.get(), /*observe=*/false);
  }
  layer->SetOpacity(opacity);
  layer->SetTransform(translation);

  // Update child views' y positions to target state to avoid stale positions.
  view_->app_list_main_view()->contents_view()->UpdateYPositionAndOpacity();
}

void AppListPresenterImpl::ScheduleOverviewModeAnimation(bool start,
                                                         bool animate) {
  // If animating, set the source parameters.
  if (animate) {
    UpdateYPositionAndOpacityForHomeLauncher(
        start ? 0 : kOverviewAnimationYOffset, start ? 1.f : 0.f,
        base::NullCallback());

    overview_animation_metrics_reporter_->Start(start);
  }
  UpdateYPositionAndOpacityForHomeLauncher(
      start ? kOverviewAnimationYOffset : 0, start ? 0.f : 1.f,
      animate ? base::BindRepeating(&UpdateOverviewSettings,
                                    overview_animation_metrics_reporter_.get())
              : base::NullCallback());
}

void AppListPresenterImpl::ShowEmbeddedAssistantUI(bool show) {
  if (view_)
    view_->app_list_main_view()->contents_view()->ShowEmbeddedAssistantUI(show);
}

bool AppListPresenterImpl::IsShowingEmbeddedAssistantUI() const {
  if (view_) {
    return view_->app_list_main_view()
        ->contents_view()
        ->IsShowingEmbeddedAssistantUI();
  }

  return false;
}

void AppListPresenterImpl::SetExpandArrowViewVisibility(bool show) {
  if (view_) {
    view_->app_list_main_view()->contents_view()->SetExpandArrowViewVisibility(
        show);
  }
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterImpl, private:

void AppListPresenterImpl::SetView(AppListView* view) {
  DCHECK(view_ == nullptr);
  DCHECK(is_visible_);

  view_ = view;
  views::Widget* widget = view_->GetWidget();
  widget->AddObserver(this);
  aura::client::GetFocusClient(widget->GetNativeView())->AddObserver(this);
  view_->GetAppsPaginationModel()->AddObserver(this);

  // Sync the |onscreen_keyboard_shown_| in case |view_| is not initiated when
  // the on-screen is shown.
  view_->set_onscreen_keyboard_shown(delegate_->GetOnScreenKeyboardShown());
}

void AppListPresenterImpl::ResetView() {
  if (!view_)
    return;

  views::Widget* widget = view_->GetWidget();
  widget->RemoveObserver(this);
  GetLayer(widget)->GetAnimator()->RemoveObserver(this);
  aura::client::GetFocusClient(widget->GetNativeView())->RemoveObserver(this);

  view_->GetAppsPaginationModel()->RemoveObserver(this);

  view_ = nullptr;
}

void AppListPresenterImpl::ScheduleAnimation() {
  // Stop observing previous animation.
  StopObservingImplicitAnimations();

  views::Widget* widget = view_->GetWidget();
  ui::Layer* layer = GetLayer(widget);
  layer->GetAnimator()->StopAnimating();
  aura::Window* root_window = widget->GetNativeView()->GetRootWindow();
  const gfx::Vector2d offset =
      delegate_->GetVisibilityAnimationOffset(root_window);
  const base::TimeDelta animation_duration =
      delegate_->GetVisibilityAnimationDuration(root_window,
                                                view_->is_fullscreen());
  gfx::Rect target_bounds = widget->GetNativeView()->bounds();
  target_bounds.Offset(offset);
  widget->GetNativeView()->SetBounds(target_bounds);
  gfx::Transform transform;
  transform.Translate(-offset.x(), -offset.y());
  layer->SetTransform(transform);

  {
    ui::ScopedLayerAnimationSettings animation(layer->GetAnimator());
    animation.SetTransitionDuration(animation_duration);
    animation.SetAnimationMetricsReporter(
        view_->GetStateTransitionMetricsReporter());
    animation.AddObserver(this);

    layer->SetTransform(gfx::Transform());
  }
  view_->StartCloseAnimation(animation_duration);
}

int64_t AppListPresenterImpl::GetDisplayId() {
  views::Widget* widget = view_ ? view_->GetWidget() : nullptr;
  if (!widget)
    return display::kInvalidDisplayId;
  return display::Screen::GetScreen()
      ->GetDisplayNearestView(widget->GetNativeView())
      .id();
}

void AppListPresenterImpl::NotifyVisibilityChanged(bool visible,
                                                   int64_t display_id) {
  // Skip adjacent same changes.
  if (last_visible_ == visible && last_display_id_ == display_id)
    return;
  last_visible_ = visible;
  last_display_id_ = display_id;

  // Notify the Shell and its observers of the app list visibility change.
  delegate_->OnVisibilityChanged(visible, display_id);
}

void AppListPresenterImpl::NotifyTargetVisibilityChanged(bool visible) {
  // Skip adjacent same changes.
  if (last_target_visible_ == visible)
    return;
  last_target_visible_ = visible;

  delegate_->OnTargetVisibilityChanged(visible);
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterImpl,  aura::client::FocusChangeObserver implementation:

void AppListPresenterImpl::OnWindowFocused(aura::Window* gained_focus,
                                           aura::Window* lost_focus) {
  if (view_ && is_visible_) {
    aura::Window* applist_window = view_->GetWidget()->GetNativeView();
    aura::Window* applist_container = applist_window->parent();
    if (applist_container->Contains(lost_focus) &&
        (!gained_focus || !applist_container->Contains(gained_focus)) &&
        !switches::ShouldNotDismissOnBlur() && !delegate_->IsTabletMode()) {
      Dismiss(base::TimeTicks());
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterImpl, ui::ImplicitAnimationObserver implementation:

void AppListPresenterImpl::OnImplicitAnimationsCompleted() {
  NotifyVisibilityChanged(GetTargetVisibility(), GetDisplayId());

  if (is_visible_) {
    view_->GetWidget()->Activate();
  } else {
    view_->GetWidget()->Close();
  }
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterImpl, views::WidgetObserver implementation:

void AppListPresenterImpl::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(view_->GetWidget(), widget);
  if (is_visible_)
    Dismiss(base::TimeTicks());
  ResetView();
}

void AppListPresenterImpl::OnWidgetDestroyed(views::Widget* widget) {
  delegate_->OnClosed();
}

void AppListPresenterImpl::OnWidgetVisibilityChanged(views::Widget* widget,
                                                     bool visible) {
  DCHECK_EQ(view_->GetWidget(), widget);
  NotifyVisibilityChanged(visible, GetDisplayId());
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterImpl, PaginationModelObserver implementation:

void AppListPresenterImpl::TotalPagesChanged() {}

void AppListPresenterImpl::SelectedPageChanged(int old_selected,
                                               int new_selected) {
  current_apps_page_ = new_selected;
}

void AppListPresenterImpl::TransitionStarted() {}

void AppListPresenterImpl::TransitionChanged() {}

void AppListPresenterImpl::TransitionEnded() {}

void AppListPresenterImpl::RequestPresentationTime(
    int64_t display_id,
    base::TimeTicks event_time_stamp) {
  if (event_time_stamp.is_null())
    return;
  aura::Window* root_window = delegate_->GetRootWindowForDisplayId(display_id);
  if (!root_window)
    return;
  ui::Compositor* compositor = root_window->layer()->GetCompositor();
  if (!compositor)
    return;
  compositor->RequestPresentationTimeForNextFrame(base::BindOnce(
      &DidPresentCompositorFrame, event_time_stamp, is_visible_));
}

}  // namespace app_list
