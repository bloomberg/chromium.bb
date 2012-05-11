// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_view.h"

#include <string>

#include "ui/app_list/app_list_bubble_border.h"
#include "ui/app_list/app_list_item_view.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_model_view.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/page_switcher.h"
#include "ui/app_list/pagination_model.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace app_list {

namespace {

// 0.2 black
const SkColor kWidgetBackgroundColor = SkColorSetARGB(0x33, 0, 0, 0);

const float kModelViewAnimationScaleFactor = 0.9f;

const int kPreferredIconDimension = 48;
const int kPreferredCols = 4;
const int kPreferredRows = 4;
// Padding space in pixels between model view and page switcher footer.
const int kModelViewFooterPadding = 10;

ui::Transform GetScaleTransform(AppListModelView* model_view) {
  gfx::Rect pixel_bounds = model_view->GetLayerBoundsInPixel();
  gfx::Point center(pixel_bounds.width() / 2, pixel_bounds.height() / 2);
  return ui::GetScaleTransform(center, kModelViewAnimationScaleFactor);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AppListView:

AppListView::AppListView(AppListViewDelegate* delegate)
    : delegate_(delegate),
      pagination_model_(new PaginationModel),
      bubble_style_(false),
      bubble_border_(NULL),
      model_view_(NULL) {
}

AppListView::~AppListView() {
  // Deletes all child views while the models are still valid.
  RemoveAllChildViews(true);
}

void AppListView::InitAsFullscreenWidget(gfx::NativeView parent,
                                         const gfx::Rect& screen_bounds,
                                         const gfx::Rect& work_area) {
  bubble_style_ = false;
  set_background(views::Background::CreateSolidBackground(
      kWidgetBackgroundColor));
  work_area_ = work_area;

  model_view_ = new AppListModelView(this, pagination_model_.get());
  model_view_->SetPaintToLayer(true);
  model_view_->layer()->SetFillsBoundsOpaquely(false);
  AddChildView(model_view_);

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  widget_params.delegate = this;
  widget_params.transparent = true;
  widget_params.parent = parent;

  views::Widget* widget = new views::Widget;
  widget->Init(widget_params);
  widget->SetContentsView(this);
  widget->SetBounds(screen_bounds);

  // Turns off default animation.
  widget->SetVisibilityChangedAnimationsEnabled(false);

  // Sets initial transform. AnimateShow changes it back to identity transform.
  model_view_->SetTransform(GetScaleTransform(model_view_));
  UpdateModel();
}

void AppListView::InitAsBubble(gfx::NativeView parent, views::View* anchor) {
  bubble_style_ = true;
  set_background(NULL);

  SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, 0, 0, kModelViewFooterPadding));

  model_view_ = new AppListModelView(this, pagination_model_.get());
  model_view_->SetLayout(kPreferredIconDimension,
                         kPreferredCols,
                         kPreferredRows);
  AddChildView(model_view_);

  PageSwitcher* page_switcher = new PageSwitcher(pagination_model_.get());
  AddChildView(page_switcher);

  set_anchor_view(anchor);
  set_parent_window(parent);
  set_close_on_deactivate(false);
  views::BubbleDelegateView::CreateBubble(this);

  // Resets default background since AppListBubbleBorder paints background.
  GetBubbleFrameView()->set_background(NULL);

  // Overrides border with AppListBubbleBorder.
  bubble_border_ = new AppListBubbleBorder(this);
  GetBubbleFrameView()->SetBubbleBorder(bubble_border_);
  SizeToContents();  // Recalcuates with new border.

  UpdateModel();
}

void AppListView::AnimateShow(int duration_ms) {
  if (bubble_style_)
    return;

  ui::Layer* layer = model_view_->layer();
  ui::ScopedLayerAnimationSettings animation(layer->GetAnimator());
  animation.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(duration_ms));
  animation.SetTweenType(ui::Tween::EASE_OUT);
  model_view_->SetTransform(ui::Transform());
}

void AppListView::AnimateHide(int duration_ms) {
  if (bubble_style_)
    return;

  ui::Layer* layer = model_view_->layer();
  ui::ScopedLayerAnimationSettings animation(layer->GetAnimator());
  animation.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(duration_ms));
  animation.SetTweenType(ui::Tween::EASE_IN);
  model_view_->SetTransform(GetScaleTransform(model_view_));
}

void AppListView::Close() {
  if (delegate_.get())
    delegate_->Close();
  else
    GetWidget()->Close();
}

void AppListView::UpdateBounds(const gfx::Rect& screen_bounds,
                               const gfx::Rect& work_area) {
  if (bubble_style_) {
    SizeToContents();
  } else {
    work_area_ = work_area;
    GetWidget()->SetBounds(screen_bounds);
  }
}

void AppListView::UpdateModel() {
  if (delegate_.get()) {
    scoped_ptr<AppListModel> new_model(new AppListModel);
    delegate_->SetModel(new_model.get());
    delegate_->UpdateModel(std::string());
    model_view_->SetModel(new_model.get());
    model_.reset(new_model.release());
  }
}

views::View* AppListView::GetInitiallyFocusedView() {
  return model_view_;
}

void AppListView::Layout() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  if (bubble_style_) {
    views::View::Layout();
  } else {
    // Gets work area rect, which is in screen coordinates.
    gfx::Rect workarea(work_area_);

    // Converts |workarea| into view's coordinates.
    gfx::Point origin(workarea.origin());
    views::View::ConvertPointFromScreen(this, &origin);
    workarea.Offset(-origin.x(), -origin.y());

    rect = rect.Intersect(workarea);
    model_view_->SetBoundsRect(rect);
  }
}

bool AppListView::OnKeyPressed(const views::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_ESCAPE) {
    Close();
    return true;
  }

  return false;
}

bool AppListView::OnMousePressed(const views::MouseEvent& event) {
  // For full screen mode, if mouse click reaches us, this means user clicks
  // on blank area. So close.
  if (!bubble_style_)
    Close();

  return true;
}

void AppListView::ButtonPressed(views::Button* sender,
                                const views::Event& event) {
  if (sender->GetClassName() != AppListItemView::kViewClassName)
    return;

  if (delegate_.get()) {
    delegate_->OnAppListItemActivated(
        static_cast<AppListItemView*>(sender)->model(),
        event.flags());
  }
  Close();
}

gfx::Rect AppListView::GetBubbleBounds() {
  // This happens before replacing the default border.
  if (!bubble_border_)
    return views::BubbleDelegateView::GetBubbleBounds();

  const int old_arrow_offset = bubble_border_->arrow_offset();
  const gfx::Rect anchor_rect = GetAnchorRect();

  bubble_border_->set_arrow_offset(0);
  gfx::Rect bubble_rect = GetBubbleFrameView()->GetUpdatedWindowBounds(
      anchor_rect,
      GetPreferredSize(),
      false /* try_mirroring_arrow */);

  gfx::Rect monitor_rect = gfx::Screen::GetMonitorNearestPoint(
      anchor_rect.CenterPoint()).work_area();
  if (monitor_rect.IsEmpty() || monitor_rect.Contains(bubble_rect))
    return bubble_rect;

  int offset = 0;
  if (bubble_rect.x() < monitor_rect.x())
    offset = monitor_rect.x() - bubble_rect.x();
  else if (bubble_rect.right() > monitor_rect.right())
    offset = monitor_rect.right() - bubble_rect.right();

  bubble_rect.set_x(bubble_rect.x() + offset);

  // Moves bubble arrow in the opposite direction. i.e. If bubble bounds is
  // moved to right (positive offset), we need to move arrow to left so that
  // it points to the same position before the move.
  bubble_border_->set_arrow_offset(-offset);

  // Repaints border if arrow offset is changed.
  if (bubble_border_->arrow_offset() != old_arrow_offset)
    GetBubbleFrameView()->SchedulePaint();

  return bubble_rect;
}

}  // namespace app_list
