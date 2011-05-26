// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/window/native_window_views.h"

#include "views/view.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// NativeWindowViews, public:

NativeWindowViews::NativeWindowViews(View* host,
                                     internal::NativeWindowDelegate* delegate)
    : NativeWidgetViews(host, delegate->AsNativeWidgetDelegate()),
      delegate_(delegate) {
}

NativeWindowViews::~NativeWindowViews() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeWindowViews, NativeWindow implementation:

Window* NativeWindowViews::GetWindow() {
  return delegate_->AsWindow();
}

const Window* NativeWindowViews::GetWindow() const {
  return delegate_->AsWindow();
}

NativeWidget* NativeWindowViews::AsNativeWidget() {
  return this;
}

const NativeWidget* NativeWindowViews::AsNativeWidget() const {
  return this;
}

gfx::Rect NativeWindowViews::GetRestoredBounds() const {
  NOTIMPLEMENTED();
  return GetView()->bounds();
}

void NativeWindowViews::ShowNativeWindow(ShowState state) {
  NOTIMPLEMENTED();
  GetView()->SetVisible(true);
}

void NativeWindowViews::BecomeModal() {
  NOTIMPLEMENTED();
}

void NativeWindowViews::CenterWindow(const gfx::Size& size) {
  // TODO(beng): actually center.
  GetView()->SetBounds(0, 0, size.width(), size.height());
}

void NativeWindowViews::GetWindowBoundsAndMaximizedState(
    gfx::Rect* bounds,
    bool* maximized) const {
  *bounds = GetView()->bounds();
  *maximized = false;
}

void NativeWindowViews::EnableClose(bool enable) {
}

void NativeWindowViews::SetWindowTitle(const std::wstring& title) {
}

void NativeWindowViews::SetWindowIcons(const SkBitmap& window_icon,
                                       const SkBitmap& app_icon) {
}

void NativeWindowViews::SetAccessibleName(const std::wstring& name) {
}

void NativeWindowViews::SetAccessibleRole(ui::AccessibilityTypes::Role role) {
}

void NativeWindowViews::SetAccessibleState(
    ui::AccessibilityTypes::State state) {
}

void NativeWindowViews::SetWindowBounds(const gfx::Rect& bounds,
                                        gfx::NativeWindow other_window) {
  if (other_window)
    NOTIMPLEMENTED();
  GetView()->SetBoundsRect(bounds);
}

void NativeWindowViews::HideWindow() {
  GetView()->SetVisible(false);
}

void NativeWindowViews::Activate() {
  NOTIMPLEMENTED();
}

void NativeWindowViews::Deactivate() {
  NOTIMPLEMENTED();
}

void NativeWindowViews::Maximize() {
  NOTIMPLEMENTED();
}

void NativeWindowViews::Minimize() {
  NOTIMPLEMENTED();
}

void NativeWindowViews::Restore() {
  NOTIMPLEMENTED();
}

bool NativeWindowViews::IsActive() const {
  return NativeWidgetViews::IsActive();
}

bool NativeWindowViews::IsVisible() const {
  return GetView()->IsVisible();
}

bool NativeWindowViews::IsMaximized() const {
  NOTIMPLEMENTED();
  return false;
}

bool NativeWindowViews::IsMinimized() const {
  NOTIMPLEMENTED();
  return false;
}

void NativeWindowViews::SetFullscreen(bool fullscreen) {
}

bool NativeWindowViews::IsFullscreen() const {
  NOTIMPLEMENTED();
  return false;
}

void NativeWindowViews::SetAlwaysOnTop(bool always_on_top) {
}

void NativeWindowViews::SetUseDragFrame(bool use_drag_frame) {
}

NonClientFrameView* NativeWindowViews::CreateFrameViewForWindow() {
  return NULL;
}

void NativeWindowViews::UpdateFrameAfterFrameChange() {
}

gfx::NativeWindow NativeWindowViews::GetNativeWindow() const {
  return NULL;
}

bool NativeWindowViews::ShouldUseNativeFrame() const {
  NOTIMPLEMENTED();
  return false;
}

void NativeWindowViews::FrameTypeChanged() {
}

}  // namespace views
