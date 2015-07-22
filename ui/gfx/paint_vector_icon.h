// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_PAINT_VECTOR_ICON_H_
#define UI_GFX_PAINT_VECTOR_ICON_H_

#include "base/basictypes.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/image/image_skia.h"

namespace gfx {

class Canvas;
enum class VectorIconId;

// Draws a vector icon identified by |id| onto |canvas| at (0, 0). |dip_size|
// is the length of a single edge of the square icon, in device independent
// pixels. |color| is used as the fill.
GFX_EXPORT void PaintVectorIcon(Canvas* canvas,
                                VectorIconId id,
                                size_t dip_size,
                                SkColor color);

// Creates an ImageSkia which will render the icon on demand.
GFX_EXPORT ImageSkia CreateVectorIcon(VectorIconId id,
                                      size_t dip_size,
                                      SkColor color);

}  // namespace gfx

#endif  // UI_GFX_PAINT_VECTOR_ICON_H_
