// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/presenter/app_list_presenter_impl.h"

#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_features.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/pagination_model.h"
#include "ui/app_list/presenter/app_list_presenter_delegate_factory.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"

namespace app_list {
namespace {

inline ui::Layer* GetLayer(views::Widget* widget) {
  return widget->GetNativeView()->layer();
}

class StateAnimationMetricsReporter : public ui::AnimationMetricsReporter {
 public:
  StateAnimationMetricsReporter() = default;
  ~StateAnimationMetricsReporter() override = default;

  void Report(int value) override {
    UMA_HISTOGRAM_PERCENTAGE("Apps.StateTransition.AnimationSmoothness", value);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(StateAnimationMetricsReporter);
};

}  // namespace

AppListPresenterImpl::AppListPresenterImpl(
    std::unique_ptr<AppListPresenterDelegateFactory> factory)
    : factory_(std::move(factory)),
      state_animation_metrics_reporter_(
          std::make_unique<StateAnimationMetricsReporter>()) {
  DCHECK(factory_);
}

AppListPresenterImpl::~AppListPresenterImpl() {
  Dismiss();
  presenter_delegate_.reset();
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

void AppListPresenterImpl::Show(int64_t display_id) {
  if (is_visible_) {
    if (display_id != GetDisplayId())
      Dismiss();
    return;
  }

  is_visible_ = true;
  if (app_list_) {
    app_list_->OnTargetVisibilityChanged(GetTargetVisibility());
    app_list_->OnVisibilityChanged(GetTargetVisibility(), display_id);
  }

  if (view_) {
    ScheduleAnimation();
  } else {
    presenter_delegate_ = factory_->GetDelegate(this);
    AppListViewDelegate* view_delegate = presenter_delegate_->GetViewDelegate();
    DCHECK(view_delegate);
    // Note the AppListViewDelegate outlives the AppListView. For Ash, the view
    // is destroyed when dismissed.
    AppListView* view = new AppListView(view_delegate);
    presenter_delegate_->Init(view, display_id, current_apps_page_);
    SetView(view);
  }
  presenter_delegate_->OnShown(display_id);
  base::RecordAction(base::UserMetricsAction("Launcher_Show"));
}

void AppListPresenterImpl::Dismiss() {
  if (!is_visible_)
    return;

  // If the app list is currently visible, there should be an existing view.
  DCHECK(view_);

  is_visible_ = false;
  if (app_list_) {
    app_list_->OnTargetVisibilityChanged(GetTargetVisibility());
    app_list_->OnVisibilityChanged(GetTargetVisibility(), GetDisplayId());
  }
  // The dismissal may have occurred in response to the app list losing
  // activation. Otherwise, our widget is currently active. When the animation
  // completes we'll hide the widget, changing activation. If a menu is shown
  // before the animation completes then the activation change triggers the menu
  // to close. By deactivating now we ensure there is no activation change when
  // the animation completes and any menus stay open.
  if (view_->GetWidget()->IsActive())
    view_->GetWidget()->Deactivate();

  presenter_delegate_->OnDismissed();
  ScheduleAnimation();
  base::RecordAction(base::UserMetricsAction("Launcher_Dismiss"));
}

void AppListPresenterImpl::ToggleAppList(int64_t display_id) {
  if (IsVisible()) {
    Dismiss();
    return;
  }
  Show(display_id);
}

bool AppListPresenterImpl::IsVisible() const {
  return view_ && view_->GetWidget()->IsVisible();
}

bool AppListPresenterImpl::GetTargetVisibility() const {
  return is_visible_;
}

void AppListPresenterImpl::SetAppList(mojom::AppListPtr app_list) {
  DCHECK(app_list);
  app_list_ = std::move(app_list);
  // Notify the app list interface of the current [target] visibility.
  app_list_->OnTargetVisibilityChanged(GetTargetVisibility());
  app_list_->OnVisibilityChanged(IsVisible(), GetDisplayId());
}

void AppListPresenterImpl::UpdateYPositionAndOpacity(int y_position_in_screen,
                                                     float background_opacity) {
  if (!is_visible_)
    return;

  if (view_)
    view_->UpdateYPositionAndOpacity(y_position_in_screen, background_opacity);
}

void AppListPresenterImpl::EndDragFromShelf(
    mojom::AppListState app_list_state) {
  if (view_) {
    if (app_list_state == mojom::AppListState::CLOSED)
      view_->Dismiss();
    else
      view_->SetState(AppListViewState(app_list_state));
    view_->SetIsInDrag(false);
    view_->DraggingLayout();
  }
}

void AppListPresenterImpl::ProcessMouseWheelOffset(int y_scroll_offset) {
  if (view_)
    view_->HandleScroll(y_scroll_offset, ui::ET_MOUSEWHEEL);
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterImpl, private:

void AppListPresenterImpl::SetView(AppListView* view) {
  DCHECK(view_ == nullptr);
  DCHECK(is_visible_);

  view_ = view;
  views::Widget* widget = view_->GetWidget();
  widget->AddObserver(this);
  widget->GetNativeView()->GetRootWindow()->AddObserver(this);
  aura::client::GetFocusClient(widget->GetNativeView())->AddObserver(this);
  view_->GetAppsPaginationModel()->AddObserver(this);
  view_->ShowWhenReady();
}

void AppListPresenterImpl::ResetView() {
  if (!view_)
    return;

  views::Widget* widget = view_->GetWidget();
  widget->RemoveObserver(this);
  GetLayer(widget)->GetAnimator()->RemoveObserver(this);
  presenter_delegate_.reset();
  widget->GetNativeView()->GetRootWindow()->RemoveObserver(this);
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
  ui::ScopedLayerAnimationSettings animation(layer->GetAnimator());
  aura::Window* root_window = widget->GetNativeView()->GetRootWindow();
  const gfx::Vector2d offset =
      presenter_delegate_->GetVisibilityAnimationOffset(root_window);
  base::TimeDelta animation_duration =
      presenter_delegate_->GetVisibilityAnimationDuration(root_window,
                                                          is_visible_);
  animation.SetTransitionDuration(animation_duration);
  gfx::Rect target_bounds = widget->GetNativeView()->bounds();

  view_->StartCloseAnimation(animation_duration);
  target_bounds.Offset(offset);

  animation.SetAnimationMetricsReporter(
      state_animation_metrics_reporter_.get());

  animation.AddObserver(this);
  widget->GetNativeView()->SetBounds(target_bounds);
}

int64_t AppListPresenterImpl::GetDisplayId() {
  views::Widget* widget = view_ ? view_->GetWidget() : nullptr;
  if (!widget)
    return display::kInvalidDisplayId;
  return display::Screen::GetScreen()
      ->GetDisplayNearestView(widget->GetNativeView())
      .id();
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
        !switches::ShouldNotDismissOnBlur()) {
      Dismiss();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterImpl,  aura::WindowObserver implementation:
void AppListPresenterImpl::OnWindowBoundsChanged(
    aura::Window* root,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  if (presenter_delegate_)
    presenter_delegate_->UpdateBounds();
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterImpl, ui::ImplicitAnimationObserver implementation:

void AppListPresenterImpl::OnImplicitAnimationsCompleted() {
  if (is_visible_)
    view_->GetWidget()->Activate();
  else
    view_->GetWidget()->Close();
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterImpl, views::WidgetObserver implementation:

void AppListPresenterImpl::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(view_->GetWidget(), widget);
  if (is_visible_)
    Dismiss();
  ResetView();
}

void AppListPresenterImpl::OnWidgetVisibilityChanged(views::Widget* widget,
                                                     bool visible) {
  DCHECK_EQ(view_->GetWidget(), widget);
  if (app_list_)
    app_list_->OnVisibilityChanged(visible, GetDisplayId());
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

}  // namespace app_list
