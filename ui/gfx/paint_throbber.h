// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_PAINT_THROBBER_H_
#define UI_GFX_PAINT_THROBBER_H_

#include "base/basictypes.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/gfx_export.h"

namespace base {
class TimeDelta;
}

namespace gfx {

class Canvas;
class Rect;

// Paints a single frame of the throbber in the "spinning", aka Material, state.
GFX_EXPORT void PaintThrobberSpinning(Canvas* canvas,
    const Rect& bounds, SkColor color, const base::TimeDelta& elapsed_time);
// As above, but frame is passed in rather than calculated. Frame duration is
// assumed to be 30ms.
GFX_EXPORT void PaintThrobberSpinningForFrame(Canvas* canvas,
    const Rect& bounds, SkColor color, uint32_t frame);

// Paints a throbber in the "waiting" state. Used when waiting on a network
// response, for example.
GFX_EXPORT void PaintThrobberWaiting(Canvas* canvas,
    const Rect& bounds, SkColor color, const base::TimeDelta& elapsed_time);
GFX_EXPORT void PaintThrobberWaitingForFrame(Canvas* canvas,
    const Rect& bounds, SkColor color, uint32_t frame);

}  // namespace gfx

#endif  // UI_GFX_PAINT_THROBBER_H_
