// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/shower/app_list_shower_impl.h"

#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/pagination_model.h"
#include "ui/app_list/shower/app_list_shower_delegate_factory.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/widget/widget.h"

namespace app_list {
namespace {

// Duration for show/hide animation in milliseconds.
const int kAnimationDurationMs = 200;

// The maximum shift in pixels when over-scroll happens.
const int kMaxOverScrollShift = 48;

ui::Layer* GetLayer(views::Widget* widget) {
  return widget->GetNativeView()->layer();
}

}  // namespace

AppListShowerImpl::AppListShowerImpl(AppListShowerDelegateFactory* factory)
    : factory_(factory) {
  DCHECK(factory);
}

AppListShowerImpl::~AppListShowerImpl() {
  shower_delegate_.reset();
  // Ensures app list view goes before the controller since pagination model
  // lives in the controller and app list view would access it on destruction.
  if (view_) {
    view_->GetAppsPaginationModel()->RemoveObserver(this);
    if (view_->GetWidget())
      view_->GetWidget()->CloseNow();
  }
}

aura::Window* AppListShowerImpl::GetWindow() {
  return is_visible_ && view_ ? view_->GetWidget()->GetNativeWindow() : nullptr;
}

void AppListShowerImpl::Show(aura::Window* window) {
  if (is_visible_)
    return;

  DCHECK(window);
  is_visible_ = true;
  aura::Window* root_window = window->GetRootWindow();
  if (view_) {
    ScheduleAnimation();
  } else {
    shower_delegate_ = factory_->GetDelegate(this);
    AppListViewDelegate* view_delegate = shower_delegate_->GetViewDelegate();
    DCHECK(view_delegate);
    // Note the AppListViewDelegate outlives the AppListView. For Ash, the view
    // is destroyed when dismissed.
    AppListView* view = new AppListView(view_delegate);
    shower_delegate_->Init(view, root_window, current_apps_page_);
    SetView(view);
  }
  shower_delegate_->OnShown(root_window);
}

void AppListShowerImpl::Dismiss() {
  if (!is_visible_)
    return;

  // If the app list is currently visible, there should be an existing view.
  DCHECK(view_);

  is_visible_ = false;

  // Our widget is currently active. When the animation completes we'll hide
  // the widget, changing activation. If a menu is shown before the animation
  // completes then the activation change triggers the menu to close. By
  // deactivating now we ensure there is no activation change when the
  // animation completes and any menus stay open.
  view_->GetWidget()->Deactivate();

  shower_delegate_->OnDismissed();
  ScheduleAnimation();
}

bool AppListShowerImpl::IsVisible() const {
  return view_ && view_->GetWidget()->IsVisible();
}

bool AppListShowerImpl::GetTargetVisibility() const {
  return is_visible_;
}

////////////////////////////////////////////////////////////////////////////////
// AppListShowerImpl, private:

void AppListShowerImpl::SetView(AppListView* view) {
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

void AppListShowerImpl::ResetView() {
  if (!view_)
    return;

  views::Widget* widget = view_->GetWidget();
  widget->RemoveObserver(this);
  GetLayer(widget)->GetAnimator()->RemoveObserver(this);
  shower_delegate_.reset();
  widget->GetNativeView()->GetRootWindow()->RemoveObserver(this);
  aura::client::GetFocusClient(widget->GetNativeView())->RemoveObserver(this);

  view_->GetAppsPaginationModel()->RemoveObserver(this);

  view_ = nullptr;
}

void AppListShowerImpl::ScheduleAnimation() {
  // Stop observing previous animation.
  StopObservingImplicitAnimations();

  views::Widget* widget = view_->GetWidget();
  ui::Layer* layer = GetLayer(widget);
  layer->GetAnimator()->StopAnimating();

  gfx::Rect target_bounds;
  gfx::Vector2d offset = shower_delegate_->GetVisibilityAnimationOffset(
      widget->GetNativeView()->GetRootWindow());
  if (is_visible_) {
    target_bounds = widget->GetWindowBoundsInScreen();
    gfx::Rect start_bounds = gfx::Rect(target_bounds);
    start_bounds.Offset(offset);
    widget->SetBounds(start_bounds);
  } else {
    target_bounds = widget->GetWindowBoundsInScreen();
    target_bounds.Offset(offset);
  }

  ui::ScopedLayerAnimationSettings animation(layer->GetAnimator());
  animation.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
      is_visible_ ? 0 : kAnimationDurationMs));
  animation.AddObserver(this);

  layer->SetOpacity(is_visible_ ? 1.0 : 0.0);
  widget->SetBounds(target_bounds);
}

////////////////////////////////////////////////////////////////////////////////
// AppListShowerImpl,  aura::client::FocusChangeObserver implementation:

void AppListShowerImpl::OnWindowFocused(aura::Window* gained_focus,
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
// AppListShowerImpl,  aura::WindowObserver implementation:
void AppListShowerImpl::OnWindowBoundsChanged(aura::Window* root,
                                              const gfx::Rect& old_bounds,
                                              const gfx::Rect& new_bounds) {
  if (shower_delegate_)
    shower_delegate_->UpdateBounds();
}

////////////////////////////////////////////////////////////////////////////////
// AppListShowerImpl, ui::ImplicitAnimationObserver implementation:

void AppListShowerImpl::OnImplicitAnimationsCompleted() {
  if (is_visible_)
    view_->GetWidget()->Activate();
  else
    view_->GetWidget()->Close();
}

////////////////////////////////////////////////////////////////////////////////
// AppListShowerImpl, views::WidgetObserver implementation:

void AppListShowerImpl::OnWidgetDestroying(views::Widget* widget) {
  DCHECK(view_->GetWidget() == widget);
  if (is_visible_)
    Dismiss();
  ResetView();
}

////////////////////////////////////////////////////////////////////////////////
// AppListShowerImpl, PaginationModelObserver implementation:

void AppListShowerImpl::TotalPagesChanged() {}

void AppListShowerImpl::SelectedPageChanged(int old_selected,
                                            int new_selected) {
  current_apps_page_ = new_selected;
}

void AppListShowerImpl::TransitionStarted() {}

void AppListShowerImpl::TransitionChanged() {
  // |view_| could be NULL when app list is closed with a running transition.
  if (!view_)
    return;

  PaginationModel* pagination_model = view_->GetAppsPaginationModel();

  const PaginationModel::Transition& transition =
      pagination_model->transition();
  if (pagination_model->is_valid_page(transition.target_page))
    return;

  views::Widget* widget = view_->GetWidget();
  ui::LayerAnimator* widget_animator =
      widget->GetNativeView()->layer()->GetAnimator();
  if (!pagination_model->IsRevertingCurrentTransition()) {
    // Update cached |view_bounds_| if it is the first over-scroll move and
    // widget does not have running animations.
    if (!should_snap_back_ && !widget_animator->is_animating())
      view_bounds_ = widget->GetWindowBoundsInScreen();

    const int current_page = pagination_model->selected_page();
    const int dir = transition.target_page > current_page ? -1 : 1;

    const double progress = 1.0 - pow(1.0 - transition.progress, 4);
    const int shift = kMaxOverScrollShift * progress * dir;

    gfx::Rect shifted(view_bounds_);
    shifted.set_x(shifted.x() + shift);

    widget->SetBounds(shifted);

    should_snap_back_ = true;
  } else if (should_snap_back_) {
    should_snap_back_ = false;
    ui::ScopedLayerAnimationSettings animation(widget_animator);
    animation.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kOverscrollPageTransitionDurationMs));
    widget->SetBounds(view_bounds_);
  }
}

}  // namespace app_list
