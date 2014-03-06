// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/ozone/cursor_factory_ozone.h"

#include "base/logging.h"

namespace ui {

// static
CursorFactoryOzone* CursorFactoryOzone::impl_ = NULL;

CursorFactoryOzone::CursorFactoryOzone() {}

CursorFactoryOzone::~CursorFactoryOzone() {}

CursorFactoryOzone* CursorFactoryOzone::GetInstance() {
  CHECK(impl_) << "No CursorFactoryOzone implementation set.";
  return impl_;
}

void CursorFactoryOzone::SetInstance(CursorFactoryOzone* impl) { impl_ = impl; }

PlatformCursor CursorFactoryOzone::GetDefaultCursor(int type) {
  NOTIMPLEMENTED();
  return NULL;
}

PlatformCursor CursorFactoryOzone::CreateImageCursor(
    const SkBitmap& bitmap,
    const gfx::Point& hotspot) {
  NOTIMPLEMENTED();
  return NULL;
}

void CursorFactoryOzone::RefImageCursor(PlatformCursor cursor) {}

void CursorFactoryOzone::UnrefImageCursor(PlatformCursor cursor) {}

void CursorFactoryOzone::SetCursor(gfx::AcceleratedWidget widget,
                                   PlatformCursor cursor) {
  NOTIMPLEMENTED();
}

void CursorFactoryOzone::MoveCursorTo(gfx::AcceleratedWidget widget,
                                      const gfx::Point& location) {
  NOTIMPLEMENTED();
}

}  // namespace ui
