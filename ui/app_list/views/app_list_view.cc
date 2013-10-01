// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/app_list_view.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/pagination_model.h"
#include "ui/app_list/signin_delegate.h"
#include "ui/app_list/views/app_list_background.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/search_box_view.h"
#include "ui/app_list/views/signin_view.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/path.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/aura/root_window.h"
#if defined(OS_WIN)
#include "ui/base/win/shell.h"
#endif
#endif

namespace app_list {

namespace {

base::Closure g_next_paint_callback;

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
    delegate_->InitModel(model_.get());
  model_->AddObserver(this);
}

AppListView::~AppListView() {
  model_->RemoveObserver(this);
  // Models are going away, ensure their references are cleared.
  RemoveAllChildViews(true);
}

void AppListView::InitAsBubbleAttachedToAnchor(
    gfx::NativeView parent,
    PaginationModel* pagination_model,
    views::View* anchor,
    const gfx::Vector2d& anchor_offset,
    views::BubbleBorder::Arrow arrow,
    bool border_accepts_events) {
  SetAnchorView(anchor);
  InitAsBubbleInternal(
      parent, pagination_model, arrow, border_accepts_events, anchor_offset);
}

void AppListView::InitAsBubbleAtFixedLocation(
    gfx::NativeView parent,
    PaginationModel* pagination_model,
    const gfx::Point& anchor_point_in_screen,
    views::BubbleBorder::Arrow arrow,
    bool border_accepts_events) {
  SetAnchorView(NULL);
  set_anchor_rect(gfx::Rect(anchor_point_in_screen, gfx::Size()));
  InitAsBubbleInternal(
      parent, pagination_model, arrow, border_accepts_events, gfx::Vector2d());
}

void AppListView::SetBubbleArrow(views::BubbleBorder::Arrow arrow) {
  GetBubbleFrameView()->bubble_border()->set_arrow(arrow);
  SizeToContents();  // Recalcuates with new border.
  GetBubbleFrameView()->SchedulePaint();
}

void AppListView::SetAnchorPoint(const gfx::Point& anchor_point) {
  set_anchor_rect(gfx::Rect(anchor_point, gfx::Size()));
  SizeToContents();  // Repositions view relative to the anchor.
}

void AppListView::SetDragAndDropHostOfCurrentAppList(
    ApplicationDragAndDropHost* drag_and_drop_host) {
  app_list_main_view_->SetDragAndDropHostOfCurrentAppList(drag_and_drop_host);
}

void AppListView::ShowWhenReady() {
  app_list_main_view_->ShowAppListWhenReady();
}

void AppListView::Close() {
  app_list_main_view_->Close();
  if (delegate_)
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

void AppListView::Paint(gfx::Canvas* canvas) {
  views::BubbleDelegateView::Paint(canvas);
  if (!g_next_paint_callback.is_null()) {
    g_next_paint_callback.Run();
    g_next_paint_callback.Reset();
  }
}

bool AppListView::ShouldHandleSystemCommands() const {
  return true;
}

void AppListView::Prerender() {
  app_list_main_view_->Prerender();
}

void AppListView::OnSigninStatusChanged() {
  signin_view_->SetVisible(!model_->signed_in());
  app_list_main_view_->SetVisible(model_->signed_in());
  app_list_main_view_->search_box_view()->InvalidateMenu();
}

void AppListView::SetProfileByPath(const base::FilePath& profile_path) {
  delegate_->SetProfileByPath(profile_path);
}

void AppListView::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AppListView::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

// static
void AppListView::SetNextPaintCallback(const base::Closure& callback) {
  g_next_paint_callback = callback;
}

#if defined(OS_WIN)
HWND AppListView::GetHWND() const {
#if defined(USE_AURA)
  gfx::NativeWindow window =
      GetWidget()->GetTopLevelWidget()->GetNativeWindow();
  return window->GetRootWindow()->GetAcceleratedWidget();
#else
  return GetWidget()->GetTopLevelWidget()->GetNativeWindow();
#endif
}
#endif

void AppListView::InitAsBubbleInternal(gfx::NativeView parent,
                                       PaginationModel* pagination_model,
                                       views::BubbleBorder::Arrow arrow,
                                       bool border_accepts_events,
                                       const gfx::Vector2d& anchor_offset) {
  app_list_main_view_ = new AppListMainView(delegate_.get(),
                                            model_.get(),
                                            pagination_model,
                                            parent);
  AddChildView(app_list_main_view_);
#if defined(USE_AURA)
  app_list_main_view_->SetPaintToLayer(true);
  app_list_main_view_->SetFillsBoundsOpaquely(false);
  app_list_main_view_->layer()->SetMasksToBounds(true);
#endif

  signin_view_ = new SigninView(
      delegate_ ? delegate_->GetSigninDelegate()
                : NULL,
      app_list_main_view_->GetPreferredSize().width());
  AddChildView(signin_view_);

  OnSigninStatusChanged();
  set_color(kContentsBackgroundColor);
  set_margins(gfx::Insets());
  set_move_with_anchor(true);
  set_parent_window(parent);
  set_close_on_deactivate(false);
  set_close_on_esc(false);
  set_anchor_view_insets(gfx::Insets(kArrowOffset + anchor_offset.y(),
                                     kArrowOffset + anchor_offset.x(),
                                     kArrowOffset - anchor_offset.y(),
                                     kArrowOffset - anchor_offset.x()));
  set_border_accepts_events(border_accepts_events);
  set_shadow(views::BubbleBorder::BIG_SHADOW);
#if defined(USE_AURA) && defined(OS_WIN)
  if (!ui::win::IsAeroGlassEnabled() ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableDwmComposition)) {
    set_shadow(views::BubbleBorder::NO_SHADOW_OPAQUE_BORDER);
  }
#endif
  views::BubbleDelegateView::CreateBubble(this);
  SetBubbleArrow(arrow);

#if defined(USE_AURA)
  GetWidget()->GetNativeWindow()->layer()->SetMasksToBounds(true);
  GetBubbleFrameView()->set_background(new AppListBackground(
      GetBubbleFrameView()->bubble_border()->GetBorderCornerRadius(),
      app_list_main_view_));
  set_background(NULL);
#else
  set_background(new AppListBackground(
      GetBubbleFrameView()->bubble_border()->GetBorderCornerRadius(),
      app_list_main_view_));

  // On non-aura the bubble has two widgets, and it's possible for the border
  // to be shown independently in odd situations. Explicitly hide the bubble
  // widget to ensure that any WM_WINDOWPOSCHANGED messages triggered by the
  // window manager do not have the SWP_SHOWWINDOW flag set which would cause
  // the border to be shown. See http://crbug.com/231687 .
  GetWidget()->Hide();
#endif
}

views::View* AppListView::GetInitiallyFocusedView() {
  return app_list_main_view_->search_box_view()->search_box();
}

gfx::ImageSkia AppListView::GetWindowIcon() {
  if (delegate_)
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
    if (app_list_main_view_->search_box_view()->HasSearch()) {
      app_list_main_view_->search_box_view()->ClearSearch();
    } else {
      GetWidget()->Deactivate();
      Close();
    }
    return true;
  }

  return false;
}

void AppListView::Layout() {
  const gfx::Rect contents_bounds = GetContentsBounds();
  app_list_main_view_->SetBoundsRect(contents_bounds);
  signin_view_->SetBoundsRect(contents_bounds);
}

void AppListView::OnWidgetDestroying(views::Widget* widget) {
  BubbleDelegateView::OnWidgetDestroying(widget);
  if (delegate_ && widget == GetWidget())
    delegate_->ViewClosing();
}

void AppListView::OnWidgetActivationChanged(views::Widget* widget,
                                            bool active) {
  // Do not called inherited function as the bubble delegate auto close
  // functionality is not used.
  if (widget == GetWidget())
    FOR_EACH_OBSERVER(Observer, observers_,
                      OnActivationChanged(widget, active));
}

void AppListView::OnWidgetVisibilityChanged(views::Widget* widget,
                                            bool visible) {
  BubbleDelegateView::OnWidgetVisibilityChanged(widget, visible);

  if (widget != GetWidget())
    return;

  // We clear the search when hiding so the next time the app list appears it is
  // not showing search results.
  if (!visible)
    app_list_main_view_->search_box_view()->ClearSearch();

  // Whether we need to signin or not may have changed since last time we were
  // shown.
  Layout();
}

void AppListView::OnAppListModelSigninStatusChanged() {
  OnSigninStatusChanged();
}

void AppListView::OnAppListModelUsersChanged() {
  OnSigninStatusChanged();
}

}  // namespace app_list
