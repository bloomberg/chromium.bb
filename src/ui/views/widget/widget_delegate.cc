// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/widget_delegate.h"

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/view.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/client_view.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// WidgetDelegate:

WidgetDelegate::Params::Params() = default;
WidgetDelegate::Params::~Params() = default;

WidgetDelegate::WidgetDelegate() = default;
WidgetDelegate::~WidgetDelegate() {
  CHECK(can_delete_this_) << "A WidgetDelegate must outlive its Widget";
}

void WidgetDelegate::SetCanActivate(bool can_activate) {
  can_activate_ = can_activate;
}

void WidgetDelegate::OnWidgetMove() {}

void WidgetDelegate::OnDisplayChanged() {}

void WidgetDelegate::OnWorkAreaChanged() {}

bool WidgetDelegate::OnCloseRequested(Widget::ClosedReason close_reason) {
  return true;
}

View* WidgetDelegate::GetInitiallyFocusedView() {
  return nullptr;
}

BubbleDialogDelegateView* WidgetDelegate::AsBubbleDialogDelegate() {
  return nullptr;
}

DialogDelegate* WidgetDelegate::AsDialogDelegate() {
  return nullptr;
}

bool WidgetDelegate::CanResize() const {
  return params_.can_resize;
}

bool WidgetDelegate::CanMaximize() const {
  return params_.can_maximize;
}

bool WidgetDelegate::CanMinimize() const {
  return params_.can_minimize;
}

bool WidgetDelegate::CanActivate() const {
  return can_activate_;
}

ui::ModalType WidgetDelegate::GetModalType() const {
  return ui::MODAL_TYPE_NONE;
}

ax::mojom::Role WidgetDelegate::GetAccessibleWindowRole() {
  return ax::mojom::Role::kWindow;
}

base::string16 WidgetDelegate::GetAccessibleWindowTitle() const {
  return GetWindowTitle();
}

base::string16 WidgetDelegate::GetWindowTitle() const {
  return params_.title;
}

bool WidgetDelegate::ShouldShowWindowTitle() const {
  return params_.show_title;
}

bool WidgetDelegate::ShouldCenterWindowTitleText() const {
#if defined(USE_AURA)
  return params_.center_title;
#else
  return false;
#endif
}

bool WidgetDelegate::ShouldShowCloseButton() const {
  return params_.show_close_button;
}

gfx::ImageSkia WidgetDelegate::GetWindowAppIcon() {
  // Use the window icon as app icon by default.
  return GetWindowIcon();
}

// Returns the icon to be displayed in the window.
gfx::ImageSkia WidgetDelegate::GetWindowIcon() {
  return params_.icon;
}

bool WidgetDelegate::ShouldShowWindowIcon() const {
  return params_.show_icon;
}

bool WidgetDelegate::ExecuteWindowsCommand(int command_id) {
  return false;
}

std::string WidgetDelegate::GetWindowName() const {
  return std::string();
}

void WidgetDelegate::SaveWindowPlacement(const gfx::Rect& bounds,
                                         ui::WindowShowState show_state) {
  std::string window_name = GetWindowName();
  if (!window_name.empty()) {
    ViewsDelegate::GetInstance()->SaveWindowPlacement(GetWidget(), window_name,
                                                      bounds, show_state);
  }
}

bool WidgetDelegate::GetSavedWindowPlacement(
    const Widget* widget,
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  std::string window_name = GetWindowName();
  if (window_name.empty() ||
      !ViewsDelegate::GetInstance()->GetSavedWindowPlacement(
          widget, window_name, bounds, show_state))
    return false;
  // Try to find a display intersecting the saved bounds.
  const auto& display =
      display::Screen::GetScreen()->GetDisplayMatching(*bounds);
  return display.bounds().Intersects(*bounds);
}

bool WidgetDelegate::ShouldRestoreWindowSize() const {
  return true;
}

void WidgetDelegate::WindowWillClose() {
  // TODO(ellyjones): For this and the other callback methods, establish whether
  // any other code calls these methods. If not, DCHECK here and below that
  // these methods are only called once.
  for (auto&& callback : window_will_close_callbacks_)
    std::move(callback).Run();
}

void WidgetDelegate::WindowClosing() {
  for (auto&& callback : window_closing_callbacks_)
    std::move(callback).Run();
}

void WidgetDelegate::DeleteDelegate() {
  for (auto&& callback : delete_delegate_callbacks_)
    std::move(callback).Run();
}

View* WidgetDelegate::GetContentsView() {
  if (!default_contents_view_)
    default_contents_view_ = new View;
  return default_contents_view_;
}

ClientView* WidgetDelegate::CreateClientView(Widget* widget) {
  return new ClientView(widget, GetContentsView());
}

NonClientFrameView* WidgetDelegate::CreateNonClientFrameView(Widget* widget) {
  return nullptr;
}

View* WidgetDelegate::CreateOverlayView() {
  return nullptr;
}

bool WidgetDelegate::WillProcessWorkAreaChange() const {
  return false;
}

bool WidgetDelegate::WidgetHasHitTestMask() const {
  return false;
}

void WidgetDelegate::GetWidgetHitTestMask(SkPath* mask) const {
  DCHECK(mask);
}

bool WidgetDelegate::ShouldDescendIntoChildForEventHandling(
    gfx::NativeView child,
    const gfx::Point& location) {
  return true;
}

void WidgetDelegate::SetCanMaximize(bool can_maximize) {
  params_.can_maximize = can_maximize;
}

void WidgetDelegate::SetCanMinimize(bool can_minimize) {
  params_.can_minimize = can_minimize;
}

void WidgetDelegate::SetCanResize(bool can_resize) {
  params_.can_resize = can_resize;
}

void WidgetDelegate::SetFocusTraversesOut(bool focus_traverses_out) {
  params_.focus_traverses_out = focus_traverses_out;
}

void WidgetDelegate::SetIcon(const gfx::ImageSkia& icon) {
  params_.icon = icon;
}

void WidgetDelegate::SetShowCloseButton(bool show_close_button) {
  params_.show_close_button = show_close_button;
}

void WidgetDelegate::SetShowIcon(bool show_icon) {
  params_.show_icon = show_icon;
}

void WidgetDelegate::SetShowTitle(bool show_title) {
  params_.show_title = show_title;
}

void WidgetDelegate::SetTitle(const base::string16& title) {
  if (params_.title == title)
    return;
  params_.title = title;
  if (GetWidget())
    GetWidget()->UpdateWindowTitle();
}

#if defined(USE_AURA)
void WidgetDelegate::SetCenterTitle(bool center_title) {
  params_.center_title = center_title;
}
#endif

void WidgetDelegate::RegisterWindowWillCloseCallback(
    base::OnceClosure callback) {
  window_will_close_callbacks_.emplace_back(std::move(callback));
}

void WidgetDelegate::RegisterWindowClosingCallback(base::OnceClosure callback) {
  window_closing_callbacks_.emplace_back(std::move(callback));
}

void WidgetDelegate::RegisterDeleteDelegateCallback(
    base::OnceClosure callback) {
  delete_delegate_callbacks_.emplace_back(std::move(callback));
}

////////////////////////////////////////////////////////////////////////////////
// WidgetDelegateView:

WidgetDelegateView::WidgetDelegateView() {
  // A WidgetDelegate should be deleted on DeleteDelegate.
  set_owned_by_client();
}

WidgetDelegateView::~WidgetDelegateView() = default;

void WidgetDelegateView::DeleteDelegate() {
  delete this;
}

Widget* WidgetDelegateView::GetWidget() {
  return View::GetWidget();
}

const Widget* WidgetDelegateView::GetWidget() const {
  return View::GetWidget();
}

views::View* WidgetDelegateView::GetContentsView() {
  return this;
}

BEGIN_METADATA(WidgetDelegateView)
METADATA_PARENT_CLASS(View)
END_METADATA()

}  // namespace views
