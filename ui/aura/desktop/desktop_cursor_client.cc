// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/desktop/desktop_cursor_client.h"

#include "ui/aura/root_window.h"
#include "ui/base/cursor/cursor_loader.h"

namespace aura {

DesktopCursorClient::DesktopCursorClient(aura::RootWindow* window)
    : root_window_(window),
      cursor_loader_(ui::CursorLoader::Create()) {
}

DesktopCursorClient::~DesktopCursorClient() {
}

void DesktopCursorClient::SetCursor(gfx::NativeCursor cursor) {
  cursor_loader_->SetPlatformCursor(&cursor);
  root_window_->SetCursor(cursor);
}

void DesktopCursorClient::ShowCursor(bool show) {
  root_window_->ShowCursor(show);
}

bool DesktopCursorClient::IsCursorVisible() const {
  return root_window_->cursor_shown();
}

void DesktopCursorClient::SetDeviceScaleFactor(float device_scale_factor) {
  cursor_loader_->UnloadAll();
  cursor_loader_->set_device_scale_factor(device_scale_factor);
}

}  // namespace aura
