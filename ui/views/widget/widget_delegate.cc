// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/widget_delegate.h"

#include "base/utf_string_conversions.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/client_view.h"
#include "views/view.h"
#include "views/views_delegate.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// WidgetDelegate:

WidgetDelegate::WidgetDelegate() : default_contents_view_(NULL) {
}

void WidgetDelegate::OnWidgetMove() {
}

void WidgetDelegate::OnDisplayChanged() {
}

void WidgetDelegate::OnWorkAreaChanged() {
}

View* WidgetDelegate::GetInitiallyFocusedView() {
  return NULL;
}

BubbleDelegateView* WidgetDelegate::AsBubbleDelegate() {
  return NULL;
}

DialogDelegate* WidgetDelegate::AsDialogDelegate() {
  return NULL;
}

bool WidgetDelegate::CanResize() const {
  return false;
}

bool WidgetDelegate::CanMaximize() const {
  return false;
}

bool WidgetDelegate::CanActivate() const {
  return true;
}

bool WidgetDelegate::IsModal() const {
  return false;
}

ui::AccessibilityTypes::Role WidgetDelegate::GetAccessibleWindowRole() const {
  return ui::AccessibilityTypes::ROLE_WINDOW;
}

ui::AccessibilityTypes::State WidgetDelegate::GetAccessibleWindowState() const {
  return 0;
}

string16 WidgetDelegate::GetAccessibleWindowTitle() const {
  return GetWindowTitle();
}

string16 WidgetDelegate::GetWindowTitle() const {
  return string16();
}

bool WidgetDelegate::ShouldShowWindowTitle() const {
  return true;
}

bool WidgetDelegate::ShouldShowClientEdge() const {
  return true;
}

SkBitmap WidgetDelegate::GetWindowAppIcon() {
  // Use the window icon as app icon by default.
  return GetWindowIcon();
}

// Returns the icon to be displayed in the window.
SkBitmap WidgetDelegate::GetWindowIcon() {
  return SkBitmap();
}

bool WidgetDelegate::ShouldShowWindowIcon() const {
  return false;
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
  if (!ViewsDelegate::views_delegate || window_name.empty())
    return;

  ViewsDelegate::views_delegate->SaveWindowPlacement(
      GetWidget(), window_name, bounds, show_state);
}

bool WidgetDelegate::GetSavedWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  std::string window_name = GetWindowName();
  if (!ViewsDelegate::views_delegate || window_name.empty())
    return false;

  return ViewsDelegate::views_delegate->GetSavedWindowPlacement(
      window_name, bounds, show_state);
}

bool WidgetDelegate::ShouldRestoreWindowSize() const {
  return true;
}

View* WidgetDelegate::GetContentsView() {
  if (!default_contents_view_)
    default_contents_view_ = new View;
  return default_contents_view_;
}

ClientView* WidgetDelegate::CreateClientView(Widget* widget) {
  return new ClientView(widget, GetContentsView());
}

NonClientFrameView* WidgetDelegate::CreateNonClientFrameView() {
  return NULL;
}

bool WidgetDelegate::WillProcessWorkAreaChange() const {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// WidgetDelegateView:

WidgetDelegateView::WidgetDelegateView() {
}

WidgetDelegateView::~WidgetDelegateView() {
}

Widget* WidgetDelegateView::GetWidget() {
  return View::GetWidget();
}

const Widget* WidgetDelegateView::GetWidget() const {
  return View::GetWidget();
}

}  // namespace views
