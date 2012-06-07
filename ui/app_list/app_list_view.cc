// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_view.h"

#include <algorithm>
#include <string>

#include "ui/app_list/app_list_bubble_border.h"
#include "ui/app_list/app_list_item_view.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/apps_grid_view.h"
#include "ui/app_list/page_switcher.h"
#include "ui/app_list/pagination_model.h"
#include "ui/app_list/search_box_model.h"
#include "ui/app_list/search_box_view.h"
#include "ui/app_list/search_result_list_view.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget.h"

namespace app_list {

namespace {

// 0.2 black
const SkColor kWidgetBackgroundColor = SkColorSetARGB(0x33, 0, 0, 0);

const float kModelViewAnimationScaleFactor = 0.9f;

const int kPreferredIconDimension = 48;
const int kPreferredCols = 4;
const int kPreferredRows = 4;

// Inner padding space in pixels of bubble contents.
const int kInnerPadding = 1;

ui::Transform GetScaleTransform(AppsGridView* model_view) {
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
      apps_view_(NULL),
      page_switcher_view_(NULL),
      search_box_view_(NULL),
      search_results_view_(NULL) {
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

  apps_view_ = new AppsGridView(this, pagination_model_.get());
  apps_view_->SetPaintToLayer(true);
  apps_view_->layer()->SetFillsBoundsOpaquely(false);
  AddChildView(apps_view_);

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
  apps_view_->SetTransform(GetScaleTransform(apps_view_));
  CreateModel();
}

void AppListView::InitAsBubble(
    gfx::NativeView parent,
    views::View* anchor,
    views::BubbleBorder::ArrowLocation arrow_location) {
  bubble_style_ = true;
  set_background(NULL);

  search_box_view_ = new SearchBoxView(this);
  AddChildView(search_box_view_);

  apps_view_ = new AppsGridView(this, pagination_model_.get());
  apps_view_->SetLayout(kPreferredIconDimension,
                        kPreferredCols,
                        kPreferredRows);
  AddChildView(apps_view_);

  search_results_view_ = new SearchResultListView(this);
  search_results_view_->SetVisible(false);
  AddChildView(search_results_view_);

  page_switcher_view_ = new PageSwitcher(pagination_model_.get());
  AddChildView(page_switcher_view_);

  search_box_view_->set_grid_view(apps_view_);
  search_box_view_->set_results_view(search_results_view_);

  set_anchor_view(anchor);
  set_margin(0);
  set_parent_window(parent);
  set_close_on_deactivate(false);
  views::BubbleDelegateView::CreateBubble(this);

  // Resets default background since AppListBubbleBorder paints background.
  GetBubbleFrameView()->set_background(NULL);

  // Overrides border with AppListBubbleBorder.
  bubble_border_ = new AppListBubbleBorder(this,
                                           search_box_view_,
                                           apps_view_,
                                           search_results_view_);
  GetBubbleFrameView()->SetBubbleBorder(bubble_border_);
  SetBubbleArrowLocation(arrow_location);

  CreateModel();
}

void AppListView::SetBubbleArrowLocation(
    views::BubbleBorder::ArrowLocation arrow_location) {
  DCHECK(bubble_border_);
  bubble_border_->set_arrow_location(arrow_location);
  SizeToContents();  // Recalcuates with new border.
}

void AppListView::AnimateShow(int duration_ms) {
  if (bubble_style_)
    return;

  ui::Layer* layer = apps_view_->layer();
  ui::ScopedLayerAnimationSettings animation(layer->GetAnimator());
  animation.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(duration_ms));
  animation.SetTweenType(ui::Tween::EASE_OUT);
  apps_view_->SetTransform(ui::Transform());
}

void AppListView::AnimateHide(int duration_ms) {
  if (bubble_style_)
    return;

  ui::Layer* layer = apps_view_->layer();
  ui::ScopedLayerAnimationSettings animation(layer->GetAnimator());
  animation.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(duration_ms));
  animation.SetTweenType(ui::Tween::EASE_IN);
  apps_view_->SetTransform(GetScaleTransform(apps_view_));
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

void AppListView::CreateModel() {
  if (delegate_.get()) {
    // Creates a new model and update all references before releasing old one.
    scoped_ptr<AppListModel> new_model(new AppListModel);

    delegate_->SetModel(new_model.get());
    apps_view_->SetModel(new_model->apps());

    // search_box_view_ etc are not created for v1.
    // TODO(xiyuan): Update this after v2 is ready.
    if (search_box_view_)
      search_box_view_->SetModel(new_model->search_box());
    if (search_results_view_)
      search_results_view_->SetResults(new_model->results());

    model_.reset(new_model.release());
  }
}

views::View* AppListView::GetInitiallyFocusedView() {
  if (bubble_style_)
    return search_box_view_->search_box();
  else
    return apps_view_;
}

gfx::Size AppListView::GetPreferredSize() {
  if (!bubble_style_)
    return View::GetPreferredSize();

  const gfx::Size search_box_size = search_box_view_->GetPreferredSize();
  const gfx::Size grid_size = apps_view_->GetPreferredSize();
  const gfx::Size page_switcher_size = page_switcher_view_->GetPreferredSize();
  const gfx::Size results_size = search_results_view_->GetPreferredSize();

  int width = std::max(
      std::max(search_box_size.width(), results_size.width()),
      std::max(grid_size.width(), page_switcher_size.width()));
  int height = search_box_size.height() +
      std::max(grid_size.height() + page_switcher_size.height(),
               results_size.height());
  return gfx::Size(width + 2 * kInnerPadding,
                   height + 2 * kInnerPadding);
}

void AppListView::Layout() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  if (bubble_style_) {
    rect.Inset(kInnerPadding, kInnerPadding);

    const int x = rect.x();
    const int width = rect.width();

    // SeachBoxView, AppsGridView and PageSwitcher uses a vertical box layout.
    int y = rect.y();
    const int search_box_height = search_box_view_->GetPreferredSize().height();
    gfx::Rect search_box_frame(gfx::Point(x, y),
                               gfx::Size(width, search_box_height));
    search_box_view_->SetBoundsRect(rect.Intersect(search_box_frame));

    y = search_box_view_->bounds().bottom();
    const int grid_height = apps_view_->GetPreferredSize().height();
    gfx::Rect grid_frame(gfx::Point(x, y), gfx::Size(width, grid_height));
    apps_view_->SetBoundsRect(rect.Intersect(grid_frame));

    y = apps_view_->bounds().bottom();
    const int page_switcher_height = rect.bottom() - y;
    gfx::Rect page_switcher_frame(gfx::Point(x, y),
                                  gfx::Size(width, page_switcher_height));
    page_switcher_view_->SetBoundsRect(rect.Intersect(page_switcher_frame));

    // Results view is mutually exclusive with AppsGridView and PageSwitcher.
    // It occupies the same space as those two views.
    gfx::Rect results_frame(grid_frame.Union(page_switcher_frame));
    search_results_view_->SetBoundsRect(rect.Intersect(results_frame));
  } else {
    // Gets work area rect, which is in screen coordinates.
    gfx::Rect workarea(work_area_);

    // Converts |workarea| into view's coordinates.
    gfx::Point origin(workarea.origin());
    views::View::ConvertPointFromScreen(this, &origin);
    workarea.Offset(-origin.x(), -origin.y());

    rect = rect.Intersect(workarea);
    apps_view_->SetBoundsRect(rect);
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
    delegate_->ActivateAppListItem(
        static_cast<AppListItemView*>(sender)->model(),
        event.flags());
  }
  Close();
}

gfx::Rect AppListView::GetBubbleBounds() {
  // This happens before replacing the default border.
  if (!bubble_border_)
    return views::BubbleDelegateView::GetBubbleBounds();

  const gfx::Point old_offset = bubble_border_->offset();
  const gfx::Rect anchor_rect = GetAnchorRect();

  bubble_border_->set_offset(gfx::Point());
  gfx::Rect bubble_rect = GetBubbleFrameView()->GetUpdatedWindowBounds(
      anchor_rect,
      GetPreferredSize(),
      false /* try_mirroring_arrow */);

  gfx::Rect monitor_rect = gfx::Screen::GetMonitorNearestPoint(
      anchor_rect.CenterPoint()).work_area();
  if (monitor_rect.IsEmpty() || monitor_rect.Contains(bubble_rect))
    return bubble_rect;

  gfx::Point offset;

  if (bubble_border_->ArrowAtTopOrBottom()) {
    if (bubble_rect.x() < monitor_rect.x())
      offset.set_x(monitor_rect.x() - bubble_rect.x());
    else if (bubble_rect.right() > monitor_rect.right())
      offset.set_x(monitor_rect.right() - bubble_rect.right());
  } else if (bubble_border_->ArrowOnLeftOrRight()) {
    if (bubble_rect.y() < monitor_rect.y())
      offset.set_y(monitor_rect.y() - bubble_rect.y());
    else if (bubble_rect.bottom() > monitor_rect.bottom())
      offset.set_y(monitor_rect.bottom() - bubble_rect.bottom());
  }

  bubble_rect.Offset(offset);
  bubble_border_->set_offset(offset);

  // Repaints border if arrow offset is changed.
  if (bubble_border_->offset() != old_offset)
    GetBubbleFrameView()->SchedulePaint();

  return bubble_rect;
}

void AppListView::QueryChanged(SearchBoxView* sender) {
  bool showing_search = search_results_view_->visible();
  bool should_show_search = !model_->search_box()->text().empty();

  if (delegate_.get()) {
    if (should_show_search)
      delegate_->StartSearch();
    else
      delegate_->StopSearch();
  }

  if (showing_search != should_show_search) {
    // TODO(xiyuan): Animate this transition.
    apps_view_->SetVisible(!should_show_search);
    page_switcher_view_->SetVisible(!should_show_search);
    search_results_view_->SetVisible(should_show_search);

    // TODO(xiyuan): Highlight default match instead of the first.
    if (search_results_view_->visible())
      search_results_view_->SetSelectedIndex(0);

    // Needs to repaint frame as well.
    GetBubbleFrameView()->SchedulePaint();
  }
}

void AppListView::OpenResult(const SearchResult& result, int event_flags) {
  if (delegate_.get())
    delegate_->OpenSearchResult(result, event_flags);
  Close();
}

}  // namespace app_list
