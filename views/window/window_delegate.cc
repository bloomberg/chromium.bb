// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/window/window_delegate.h"
#include "views/views_delegate.h"
#include "views/window/client_view.h"
#include "views/window/window.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace views {

WindowDelegate::WindowDelegate() : window_(NULL) {
}

WindowDelegate::~WindowDelegate() {
}

DialogDelegate* WindowDelegate::AsDialogDelegate() {
  return NULL;
}

bool WindowDelegate::CanResize() const {
  return false;
}

bool WindowDelegate::CanMaximize() const {
  return false;
}

bool WindowDelegate::CanActivate() const {
  return true;
}

bool WindowDelegate::IsModal() const {
  return false;
}

ui::AccessibilityTypes::Role WindowDelegate::GetAccessibleRole() const {
  return ui::AccessibilityTypes::ROLE_WINDOW;
}

ui::AccessibilityTypes::State WindowDelegate::GetAccessibleState() const {
  return 0;
}

std::wstring WindowDelegate::GetAccessibleWindowTitle() const {
  return GetWindowTitle();
}

std::wstring WindowDelegate::GetWindowTitle() const {
  return L"";
}

View* WindowDelegate::GetInitiallyFocusedView() {
  return NULL;
}

bool WindowDelegate::ShouldShowWindowTitle() const {
  return true;
}

bool WindowDelegate::ShouldShowClientEdge() const {
  return true;
}

SkBitmap WindowDelegate::GetWindowAppIcon() {
  // Use the window icon as app icon by default.
  return GetWindowIcon();
}

// Returns the icon to be displayed in the window.
SkBitmap WindowDelegate::GetWindowIcon() {
  return SkBitmap();
}

bool WindowDelegate::ShouldShowWindowIcon() const {
  return false;
}

bool WindowDelegate::ExecuteWindowsCommand(int command_id) {
  return false;
}

std::wstring WindowDelegate::GetWindowName() const {
  return std::wstring();
}

void WindowDelegate::SaveWindowPlacement(const gfx::Rect& bounds,
                                         bool maximized) {
  std::wstring window_name = GetWindowName();
  if (!ViewsDelegate::views_delegate || window_name.empty())
    return;

  ViewsDelegate::views_delegate->SaveWindowPlacement(
      window_, window_name, bounds, maximized);
}

bool WindowDelegate::GetSavedWindowBounds(gfx::Rect* bounds) const {
  std::wstring window_name = GetWindowName();
  if (!ViewsDelegate::views_delegate || window_name.empty())
    return false;

  return ViewsDelegate::views_delegate->GetSavedWindowBounds(
      window_, window_name, bounds);
}

bool WindowDelegate::GetSavedMaximizedState(bool* maximized) const {
  std::wstring window_name = GetWindowName();
  if (!ViewsDelegate::views_delegate || window_name.empty())
    return false;

  return ViewsDelegate::views_delegate->GetSavedMaximizedState(
      window_, window_name, maximized);
}

bool WindowDelegate::ShouldRestoreWindowSize() const {
  return true;
}

View* WindowDelegate::GetContentsView() {
  return NULL;
}

ClientView* WindowDelegate::CreateClientView(Window* window) {
  return new ClientView(window, GetContentsView());
}

}  // namespace views
