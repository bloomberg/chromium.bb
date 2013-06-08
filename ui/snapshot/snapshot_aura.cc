// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/snapshot/snapshot.h"

#include "base/logging.h"
#include "base/safe_numerics.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/display.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/rect_f.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/gfx/transform.h"

namespace ui {

bool GrabViewSnapshot(gfx::NativeView view,
                      std::vector<unsigned char>* png_representation,
                      const gfx::Rect& snapshot_bounds) {
  return GrabWindowSnapshot(view, png_representation, snapshot_bounds);
}

bool GrabWindowSnapshot(gfx::NativeWindow window,
                        std::vector<unsigned char>* png_representation,
                        const gfx::Rect& snapshot_bounds) {
  ui::Compositor* compositor = window->layer()->GetCompositor();

  gfx::RectF read_pixels_bounds = snapshot_bounds;

  // We must take into account the window's position on the desktop.
  read_pixels_bounds.Offset(
      window->GetBoundsInRootWindow().origin().OffsetFromOrigin());
  aura::RootWindow* root_window = window->GetRootWindow();
  if (root_window)
    root_window->GetRootTransform().TransformRect(&read_pixels_bounds);

  gfx::Rect read_pixels_bounds_in_pixel =
      gfx::ToEnclosingRect(read_pixels_bounds);

  // Sometimes (i.e. when using Aero on Windows) the compositor's size is
  // smaller than the window bounds. So trim appropriately.
  read_pixels_bounds_in_pixel.Intersect(gfx::Rect(compositor->size()));

  DCHECK_LE(0, read_pixels_bounds.x());
  DCHECK_LE(0, read_pixels_bounds.y());

  SkBitmap bitmap;
  if (!compositor->ReadPixels(&bitmap, read_pixels_bounds_in_pixel))
    return false;

  gfx::Display display =
      gfx::Screen::GetScreenFor(window)->GetDisplayNearestWindow(window);
  switch (display.rotation()) {
    case gfx::Display::ROTATE_0:
      break;
    case gfx::Display::ROTATE_90:
      bitmap = SkBitmapOperations::Rotate(
          bitmap, SkBitmapOperations::ROTATION_270_CW);
      break;
    case gfx::Display::ROTATE_180:
      bitmap = SkBitmapOperations::Rotate(
          bitmap, SkBitmapOperations::ROTATION_180_CW);
      break;
    case gfx::Display::ROTATE_270:
      bitmap = SkBitmapOperations::Rotate(
          bitmap, SkBitmapOperations::ROTATION_90_CW);
      break;
  }

  unsigned char* pixels = reinterpret_cast<unsigned char*>(
      bitmap.pixelRef()->pixels());
  gfx::PNGCodec::Encode(pixels, gfx::PNGCodec::FORMAT_BGRA,
                        gfx::Size(bitmap.width(), bitmap.height()),
                        base::checked_numeric_cast<int>(bitmap.rowBytes()),
                        true, std::vector<gfx::PNGCodec::Comment>(),
                        png_representation);
  return true;
}

}  // namespace ui
