// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/app_list.h"

#include "base/bind.h"
#include "ui/aura/event.h"
#include "ui/aura/window.h"
#include "ui/aura_shell/shell.h"
#include "ui/aura_shell/shell_delegate.h"
#include "ui/aura_shell/shell_window_ids.h"
#include "ui/gfx/screen.h"

namespace aura_shell {
namespace internal {

namespace {

// Gets preferred bounds of app list window in show/hide state.
gfx::Rect GetPreferredBounds(bool show) {
  // The y-axis offset used at the beginning of showing animation.
  static const int kMoveUpAnimationOffset = 50;

  gfx::Point cursor = gfx::Screen::GetCursorScreenPoint();
  gfx::Rect work_area = gfx::Screen::GetMonitorWorkAreaNearestPoint(cursor);
  gfx::Rect widget_bounds(work_area);
  widget_bounds.Inset(150, 100);
  if (!show)
    widget_bounds.Offset(0, kMoveUpAnimationOffset);

  return widget_bounds;
}

ui::Layer* GetLayer(views::Widget* widget) {
  return widget->GetNativeView()->layer();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AppList, public:

AppList::AppList()
    : aura::EventFilter(NULL),
      is_visible_(false),
      widget_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(set_widget_factory_(this)) {
}

AppList::~AppList() {
  ResetWidget();
}

void AppList::SetVisible(bool visible) {
  if (visible == is_visible_)
    return;

  is_visible_ = visible;

  if (widget_) {
    ScheduleAnimation();
  } else if (is_visible_ && !set_widget_factory_.HasWeakPtrs()) {
    Shell::GetInstance()->delegate()->RequestAppListWidget(
        GetPreferredBounds(false),
        base::Bind(&AppList::SetWidget, set_widget_factory_.GetWeakPtr()));
  }
}

bool AppList::IsVisible() {
  return widget_ && widget_->IsVisible();
}

////////////////////////////////////////////////////////////////////////////////
// AppList, private:

void AppList::SetWidget(views::Widget* widget) {
  DCHECK(widget_ == NULL);
  set_widget_factory_.InvalidateWeakPtrs();

  if (is_visible_) {
    widget_ = widget;
    widget_->AddObserver(this);
    GetLayer(widget_)->GetAnimator()->AddObserver(this);
    Shell::GetInstance()->AddDesktopEventFilter(this);

    widget_->SetBounds(GetPreferredBounds(false));
    widget_->SetOpacity(0);
    ScheduleAnimation();

    widget_->Show();
  } else {
    widget->Close();
  }
}

void AppList::ResetWidget() {
  if (!widget_)
    return;

  widget_->RemoveObserver(this);
  GetLayer(widget_)->GetAnimator()->RemoveObserver(this);
  Shell::GetInstance()->RemoveDesktopEventFilter(this);
  widget_ = NULL;
}

void AppList::ScheduleAnimation() {
  ui::Layer* layer = GetLayer(widget_);
  ui::LayerAnimator::ScopedSettings app_list_animation(layer->GetAnimator());
  layer->SetBounds(GetPreferredBounds(is_visible_));
  layer->SetOpacity(is_visible_ ? 1.0 : 0.0);

  ui::Layer* default_container_layer = Shell::GetInstance()->GetContainer(
      internal::kShellWindowId_DefaultContainer)->layer();
  ui::LayerAnimator::ScopedSettings default_container_animation(
      default_container_layer->GetAnimator());
  default_container_layer->SetOpacity(is_visible_ ? 0.0 : 1.0);
}

////////////////////////////////////////////////////////////////////////////////
// AppList, aura::EventFilter implementation:

bool AppList::PreHandleKeyEvent(aura::Window* target,
                                aura::KeyEvent* event) {
  return false;
}

bool AppList::PreHandleMouseEvent(aura::Window* target,
                                 aura::MouseEvent* event) {
  if (widget_ && is_visible_ && event->type() == ui::ET_MOUSE_PRESSED) {
    aura::MouseEvent translated(*event, target, widget_->GetNativeView());
    if (!widget_->GetNativeView()->ContainsPoint(translated.location()))
      SetVisible(false);
  }
  return false;
}

ui::TouchStatus AppList::PreHandleTouchEvent(aura::Window* target,
                                             aura::TouchEvent* event) {
  return ui::TOUCH_STATUS_UNKNOWN;
}

////////////////////////////////////////////////////////////////////////////////
// AppList, ui::LayerAnimationObserver implementation:

void AppList::OnLayerAnimationEnded(
    const ui::LayerAnimationSequence* sequence) {
  if (!is_visible_ )
    widget_->Close();
}

void AppList::OnLayerAnimationAborted(
    const ui::LayerAnimationSequence* sequence) {
}

void AppList::OnLayerAnimationScheduled(
    const ui::LayerAnimationSequence* sequence) {
}

////////////////////////////////////////////////////////////////////////////////
// AppList, views::Widget::Observer implementation:

void AppList::OnWidgetClosing(views::Widget* widget) {
  DCHECK(widget_ == widget);
  if (is_visible_)
    SetVisible(false);
  ResetWidget();
}

void AppList::OnWidgetActivationChanged(views::Widget* widget, bool active) {
  DCHECK(widget_ == widget);
  if (widget_ && is_visible_ && !active)
    SetVisible(false);
}

}  // namespace internal
}  // namespace aura_shell
