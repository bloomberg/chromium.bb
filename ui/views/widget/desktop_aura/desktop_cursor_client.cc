// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_cursor_client.h"

#include "ui/aura/root_window.h"
#include "ui/base/cursor/cursor_loader.h"

namespace views {

DesktopCursorClient::DesktopCursorClient(aura::RootWindow* window)
    : root_window_(window),
      cursor_loader_(ui::CursorLoader::Create()),
      current_cursor_(ui::kCursorNone),
      cursor_visible_(true) {
}

DesktopCursorClient::~DesktopCursorClient() {
}

void DesktopCursorClient::SetCursor(gfx::NativeCursor cursor) {
  current_cursor_ = cursor;
  cursor_loader_->SetPlatformCursor(&current_cursor_);
  if (cursor_visible_)
    root_window_->SetCursor(current_cursor_);
}

void DesktopCursorClient::ShowCursor(bool show) {
  if (cursor_visible_ == show)
    return;
  cursor_visible_ = show;
  if (cursor_visible_)
    root_window_->SetCursor(current_cursor_);
  else
    root_window_->SetCursor(ui::kCursorNone);
  root_window_->OnCursorVisibilityChanged(cursor_visible_);
}

bool DesktopCursorClient::IsCursorVisible() const {
  return cursor_visible_;
}

void DesktopCursorClient::SetDeviceScaleFactor(float device_scale_factor) {
  cursor_loader_->UnloadAll();
  cursor_loader_->set_device_scale_factor(device_scale_factor);
}

void DesktopCursorClient::LockCursor() {
  // TODO(mazda): Implement this.
  NOTIMPLEMENTED();
}

void DesktopCursorClient::UnlockCursor() {
  // TODO(mazda): Implement this.
  NOTIMPLEMENTED();
}

}  // namespace views
