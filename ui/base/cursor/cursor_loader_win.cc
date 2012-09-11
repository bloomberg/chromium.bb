// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursor_loader_win.h"

namespace ui {

CursorLoader* CursorLoader::Create() {
  return new CursorLoaderWin;
}

CursorLoaderWin::CursorLoaderWin() {
}

CursorLoaderWin::~CursorLoaderWin() {
}

void CursorLoaderWin::LoadImageCursor(int id,
                                      int resource_id,
                                      const gfx::Point& hot) {
  // NOTIMPLEMENTED();
}

void CursorLoaderWin::LoadAnimatedCursor(int id,
                                         int resource_id,
                                         const gfx::Point& hot,
                                         int frame_delay_ms) {
  // NOTIMPLEMENTED();
}

void CursorLoaderWin::UnloadAll() {
  // NOTIMPLEMENTED();
}

void CursorLoaderWin::SetPlatformCursor(gfx::NativeCursor* cursor) {
  // NOTIMPLEMENTED();
}

}  // namespace ui
