// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursor_loader_ozone.h"

#include "ui/base/cursor/cursor.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

namespace ui {

namespace {

// Creates a 1x1 cursor which will be fully transparent.
SkBitmap CreateInvisibleCursor() {
  SkBitmap cursor;
  cursor.setConfig(SkBitmap::kARGB_8888_Config, 1, 1);
  cursor.allocPixels();

  cursor.lockPixels();
  cursor.eraseARGB(0, 0, 0, 0);
  cursor.unlockPixels();

  return cursor;
}

}  // namespace

CursorLoaderOzone::CursorLoaderOzone()
    : invisible_cursor_(CreateInvisibleCursor()) {}

CursorLoaderOzone::~CursorLoaderOzone() {}

void CursorLoaderOzone::LoadImageCursor(int id,
                                        int resource_id,
                                        const gfx::Point& hot) {
  cursors_[id] = ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      resource_id);
}

void CursorLoaderOzone::LoadAnimatedCursor(int id,
                                           int resource_id,
                                           const gfx::Point& hot,
                                           int frame_delay_ms) {
  // TODO(dnicoara) Add support: crbug.com/343245
  NOTIMPLEMENTED();
}

void CursorLoaderOzone::UnloadAll() {
  cursors_.clear();
}

void CursorLoaderOzone::SetPlatformCursor(gfx::NativeCursor* cursor) {
  if (cursors_.find(cursor->native_type()) != cursors_.end()) {
    const gfx::ImageSkiaRep& image_rep =
        cursors_[cursor->native_type()]->GetRepresentation(
            display().device_scale_factor());

    cursor->SetPlatformCursor(&image_rep.sk_bitmap());
  } else if (*cursor == kCursorNone) {
    cursor->SetPlatformCursor(&invisible_cursor_);
  } else if (*cursor == kCursorCustom) {
    // TODO(dnicoara) Add support for custom cursors: crbug.com/343155
    cursor->SetPlatformCursor(cursor->platform());
  } else {
    const gfx::ImageSkiaRep& image_rep =
        cursors_[kCursorPointer]->GetRepresentation(
            display().device_scale_factor());

    cursor->SetPlatformCursor(&image_rep.sk_bitmap());
  }
}

CursorLoader* CursorLoader::Create() {
  return new CursorLoaderOzone();
}

}  // namespace ui
