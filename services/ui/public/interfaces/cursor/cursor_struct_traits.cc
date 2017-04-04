// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/public/interfaces/cursor/cursor_struct_traits.h"

#include "base/time/time.h"
#include "mojo/common/common_custom_types_struct_traits.h"
#include "skia/public/interfaces/bitmap_array_struct_traits.h"
#include "skia/public/interfaces/bitmap_skbitmap_struct_traits.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/cursor.h"
#include "ui/gfx/geometry/mojo/geometry_struct_traits.h"

namespace mojo {

// static
const base::TimeDelta&
StructTraits<ui::mojom::CursorDataDataView, ui::CursorData>::frame_delay(
    const ui::CursorData& c) {
  return c.frame_delay();
}

// static
const gfx::Point&
StructTraits<ui::mojom::CursorDataDataView, ui::CursorData>::hotspot_in_pixels(
    const ui::CursorData& c) {
  return c.hotspot_in_pixels();
}

// static
const std::vector<SkBitmap>&
StructTraits<ui::mojom::CursorDataDataView, ui::CursorData>::cursor_frames(
    const ui::CursorData& c) {
  return c.cursor_frames();
}

// static
bool StructTraits<ui::mojom::CursorDataDataView, ui::CursorData>::Read(
    ui::mojom::CursorDataDataView data,
    ui::CursorData* out) {
  ui::mojom::CursorType type = data.cursor_type();
  if (type != ui::mojom::CursorType::CUSTOM) {
    *out = ui::CursorData(static_cast<int>(type));
    return true;
  }

  gfx::Point hotspot_in_pixels;
  std::vector<SkBitmap> cursor_frames;
  float scale_factor = data.scale_factor();
  base::TimeDelta frame_delay;

  if (!data.ReadHotspotInPixels(&hotspot_in_pixels) ||
      !data.ReadCursorFrames(&cursor_frames) ||
      !data.ReadFrameDelay(&frame_delay)) {
    return false;
  }

  *out = ui::CursorData(hotspot_in_pixels, cursor_frames, scale_factor,
                        frame_delay);
  return true;
}

}  // namespace mojo
