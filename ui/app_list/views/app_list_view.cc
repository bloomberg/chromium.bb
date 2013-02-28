// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/app_list_view.h"

#include "base/string_util.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/pagination_model.h"
#include "ui/app_list/signin_delegate.h"
#include "ui/app_list/views/app_list_background.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/search_box_view.h"
#include "ui/app_list/views/signin_view.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/path.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace app_list {

namespace {

// The distance between the arrow tip and edge of the anchor view.
const int kArrowOffset = 10;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AppListView:

AppListView::AppListView(AppListViewDelegate* delegate)
    : model_(new AppListModel),
      delegate_(delegate),
      app_list_main_view_(NULL),
      signin_view_(NULL) {
  if (delegate_)
    delegate_->SetModel(model_.get());
  if (GetSigninDelegate())
    GetSigninDelegate()->AddObserver(this);
}

AppListView::~AppListView() {
  if (GetSigninDelegate())
    GetSigninDelegate()->RemoveObserver(this);

  // Models are going away, ensure their references are cleared.
  RemoveAllChildViews(true);
}

void AppListView::InitAsBubble(
    gfx::NativeView parent,
    PaginationModel* pagination_model,
    views::View* anchor,
    const gfx::Point& anchor_point,
    views::BubbleBorder::ArrowLocation arrow_location,
    bool border_accepts_events) {

  app_list_main_view_ = new AppListMainView(delegate_.get(),
                                            model_.get(),
                                            pagination_model,
                                            anchor);
  AddChildView(app_list_main_view_);
#if defined(USE_AURA)
  app_list_main_view_->SetPaintToLayer(true);
  app_list_main_view_->SetFillsBoundsOpaquely(false);
  app_list_main_view_->layer()->SetMasksToBounds(true);
#endif

  signin_view_ = new SigninView(
      GetSigninDelegate(),
      app_list_main_view_->GetPreferredSize().width());
  AddChildView(signin_view_);

  set_anchor_view(anchor);
  set_anchor_point(anchor_point);
  set_color(kContentsBackgroundColor);
  set_margins(gfx::Insets());
  set_move_with_anchor(true);
  set_parent_window(parent);
  set_close_on_deactivate(false);
  set_close_on_esc(false);
  set_anchor_insets(gfx::Insets(kArrowOffset, kArrowOffset,
                                kArrowOffset, kArrowOffset));
  set_border_accepts_events(border_accepts_events);
  set_shadow(views::BubbleBorder::BIG_SHADOW);
  views::BubbleDelegateView::CreateBubble(this);
  SetBubbleArrowLocation(arrow_location);

#if defined(USE_AURA)
  GetBubbleFrameView()->set_background(new AppListBackground(
      GetBubbleFrameView()->bubble_border()->GetBorderCornerRadius(),
      app_list_main_view_->search_box_view()));

  set_background(NULL);
#else
  set_background(new AppListBackground(
      GetBubbleFrameView()->bubble_border()->GetBorderCornerRadius(),
      app_list_main_view_->search_box_view()));
#endif
}

void AppListView::SetBubbleArrowLocation(
    views::BubbleBorder::ArrowLocation arrow_location) {
  GetBubbleFrameView()->bubble_border()->set_arrow_location(arrow_location);
  SizeToContents();  // Recalcuates with new border.
  GetBubbleFrameView()->SchedulePaint();
}

void AppListView::SetAnchorPoint(const gfx::Point& anchor_point) {
  set_anchor_point(anchor_point);
  SizeToContents();  // Repositions view relative to the anchor.
}

void AppListView::ShowWhenReady() {
  app_list_main_view_->ShowAppListWhenReady();
}

void AppListView::Close() {
  app_list_main_view_->Close();

  if (delegate_.get())
    delegate_->Dismiss();
  else
    GetWidget()->Close();
}

void AppListView::UpdateBounds() {
  SizeToContents();
}

gfx::Size AppListView::GetPreferredSize() {
  return app_list_main_view_->GetPreferredSize();
}

views::View* AppListView::GetInitiallyFocusedView() {
  return app_list_main_view_->search_box_view()->search_box();
}

gfx::ImageSkia AppListView::GetWindowIcon() {
  if (delegate_.get())
    return delegate_->GetWindowIcon();

  return gfx::ImageSkia();
}

bool AppListView::WidgetHasHitTestMask() const {
  return true;
}

void AppListView::GetWidgetHitTestMask(gfx::Path* mask) const {
  DCHECK(mask);
  mask->addRect(gfx::RectToSkRect(
      GetBubbleFrameView()->GetContentsBounds()));
}

bool AppListView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  // The accelerator is added by BubbleDelegateView.
  if (accelerator.key_code() == ui::VKEY_ESCAPE) {
    Close();
    return true;
  }

  return false;
}

void AppListView::Layout() {
  if (!signin_view_) {
    app_list_main_view_->SetBounds(0, 0, width(), height());
    return;
  }

  if (GetSigninDelegate() && GetSigninDelegate()->NeedSignin()) {
    signin_view_->SetBounds(0, 0, width(), height());
    app_list_main_view_->SetBounds(width(), 0, width(), height());
    return;
  }

  signin_view_->SetBounds(-width(), 0, width(), height());
  app_list_main_view_->SetBounds(0, 0, width(), height());
}

void AppListView::OnWidgetDestroying(views::Widget* widget) {
  BubbleDelegateView::OnWidgetDestroying(widget);
  if (delegate_.get() && widget == GetWidget())
    delegate_->ViewClosing();
}

void AppListView::OnWidgetActivationChanged(views::Widget* widget,
                                            bool active) {
  // Do not called inherited function as the bubble delegate auto close
  // functionality is not used.
  if (delegate_.get() && widget == GetWidget())
    delegate_->ViewActivationChanged(active);
}

void AppListView::OnWidgetVisibilityChanged(views::Widget* widget,
                                            bool visible) {
  BubbleDelegateView::OnWidgetVisibilityChanged(widget, visible);

  if (widget != GetWidget())
    return;

  // Whether we need to signin or not may have changed since last time we were
  // shown.
  Layout();
}

void AppListView::OnSigninSuccess() {
  Layout();
}

SigninDelegate* AppListView::GetSigninDelegate() {
  return delegate_ ? delegate_->GetSigninDelegate() : NULL;
}

}  // namespace app_list
