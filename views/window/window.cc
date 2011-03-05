// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/window/window.h"

#include "base/string_util.h"
#include "ui/base/l10n/l10n_font_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "views/widget/widget.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// Window, public:

Window::Window() : native_window_(NULL) {
}

Window::~Window() {
}

// static
int Window::GetLocalizedContentsWidth(int col_resource_id) {
  return ui::GetLocalizedContentsWidthForFont(col_resource_id,
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::BaseFont));
}

// static
int Window::GetLocalizedContentsHeight(int row_resource_id) {
  return ui::GetLocalizedContentsHeightForFont(row_resource_id,
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::BaseFont));
}

// static
gfx::Size Window::GetLocalizedContentsSize(int col_resource_id,
                                           int row_resource_id) {
  return gfx::Size(GetLocalizedContentsWidth(col_resource_id),
                   GetLocalizedContentsHeight(row_resource_id));
}

// static
void Window::CloseSecondaryWidget(Widget* widget) {
  if (!widget)
    return;

  // Close widget if it's identified as a secondary window.
  Window* window = widget->GetWindow();
  if (window) {
    if (!window->IsAppWindow())
      window->Close();
  } else {
    // If it's not a Window, then close it anyway since it probably is
    // secondary.
    widget->Close();
  }
}

gfx::Rect Window::GetBounds() const {
  return gfx::Rect();
}

gfx::Rect Window::GetNormalBounds() const {
  return gfx::Rect();
}

void Window::SetWindowBounds(const gfx::Rect& bounds,
                             gfx::NativeWindow other_window) {
}

void Window::Show() {
}

void Window::HideWindow() {
}

void Window::SetNativeWindowProperty(const char* name, void* value) {
}

void* Window::GetNativeWindowProperty(const char* name) {
  return NULL;
}

void Window::Activate() {
}

void Window::Deactivate() {
}

void Window::Close() {
}

void Window::Maximize() {
}

void Window::Minimize() {
}

void Window::Restore() {
}

bool Window::IsActive() const {
  return false;
}

bool Window::IsVisible() const {
  return false;
}

bool Window::IsMaximized() const {
  return false;
}

bool Window::IsMinimized() const {
  return false;
}

void Window::SetFullscreen(bool fullscreen) {
}

bool Window::IsFullscreen() const {
  return false;
}

void Window::SetUseDragFrame(bool use_drag_frame) {
}

bool Window::IsAppWindow() const {
  return false;
}

void Window::EnableClose(bool enable) {
}

void Window::UpdateWindowTitle() {
}

void Window::UpdateWindowIcon() {
}

void Window::SetIsAlwaysOnTop(bool always_on_top) {
}

NonClientFrameView* Window::CreateFrameViewForWindow() {
  return NULL;
}

void Window::UpdateFrameAfterFrameChange() {
}

WindowDelegate* Window::GetDelegate() const {
  return NULL;
}

NonClientView* Window::GetNonClientView() const {
  return NULL;
}

ClientView* Window::GetClientView() const {
  return NULL;
}

gfx::NativeWindow Window::GetNativeWindow() const {
  return NULL;
}

bool Window::ShouldUseNativeFrame() const {
  return false;
}

void Window::FrameTypeChanged() {
}

}  // namespace views
