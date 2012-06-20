// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_view.h"

#include "ui/app_list/app_list_bubble_border.h"
#include "ui/app_list/app_list_item_view.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/contents_view.h"
#include "ui/app_list/pagination_model.h"
#include "ui/app_list/search_box_model.h"
#include "ui/app_list/search_box_view.h"
#include "ui/gfx/screen.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace app_list {

namespace {

// Inner padding space in pixels of bubble contents.
const int kInnerPadding = 1;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AppListView:

AppListView::AppListView(AppListViewDelegate* delegate)
    : delegate_(delegate),
      bubble_border_(NULL),
      search_box_view_(NULL),
      contents_view_(NULL) {
}

AppListView::~AppListView() {
  // Deletes all child views while the models are still valid.
  RemoveAllChildViews(true);
}

void AppListView::InitAsBubble(
    gfx::NativeView parent,
    PaginationModel* pagination_model,
    views::View* anchor,
    views::BubbleBorder::ArrowLocation arrow_location) {
  set_background(NULL);

  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical,
                                        kInnerPadding,
                                        kInnerPadding,
                                        kInnerPadding));

  search_box_view_ = new SearchBoxView(this);
  AddChildView(search_box_view_);

  contents_view_ = new ContentsView(this, pagination_model);
  AddChildView(contents_view_);

  search_box_view_->set_contents_view(contents_view_);

  set_anchor_view(anchor);
  set_margin(0);
  set_move_with_anchor(true);
  set_parent_window(parent);
  set_close_on_deactivate(false);
  views::BubbleDelegateView::CreateBubble(this);

  // Resets default background since AppListBubbleBorder paints background.
  GetBubbleFrameView()->set_background(NULL);

  // Overrides border with AppListBubbleBorder.
  bubble_border_ = new AppListBubbleBorder(this, search_box_view_);
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

void AppListView::Close() {
  if (delegate_.get())
    delegate_->Close();
  else
    GetWidget()->Close();
}

void AppListView::UpdateBounds() {
  SizeToContents();
}

void AppListView::CreateModel() {
  if (delegate_.get()) {
    // Creates a new model and update all references before releasing old one.
    scoped_ptr<AppListModel> new_model(new AppListModel);

    delegate_->SetModel(new_model.get());
    search_box_view_->SetModel(new_model->search_box());
    contents_view_->SetModel(new_model.get());

    model_.reset(new_model.release());
  }
}

views::View* AppListView::GetInitiallyFocusedView() {
  return search_box_view_->search_box();
}

bool AppListView::HasHitTestMask() const {
  return true;
}

void AppListView::GetHitTestMask(gfx::Path* mask) const {
  DCHECK(mask);
  bubble_border_->GetMask(GetBubbleFrameView()->bounds(), mask);
}

bool AppListView::OnKeyPressed(const views::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_ESCAPE) {
    Close();
    return true;
  }

  return false;
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

  gfx::Rect monitor_rect = gfx::Screen::GetDisplayNearestPoint(
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
  bool should_show_search = !model_->search_box()->text().empty();
  contents_view_->ShowSearchResults(should_show_search);

  if (delegate_.get()) {
    if (should_show_search)
      delegate_->StartSearch();
    else
      delegate_->StopSearch();
  }
}

void AppListView::OpenResult(const SearchResult& result, int event_flags) {
  if (delegate_.get())
    delegate_->OpenSearchResult(result, event_flags);
  Close();
}

}  // namespace app_list
