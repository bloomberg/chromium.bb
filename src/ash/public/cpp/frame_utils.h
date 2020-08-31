// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_FRAME_UTILS_H_
#define ASH_PUBLIC_CPP_FRAME_UTILS_H_

#include "ash/public/cpp/ash_public_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/widget/widget.h"

namespace gfx {
class Point;
}  // namespace gfx

namespace views {
class NonClientFrameView;
}

namespace ash {

// Returns the HitTestCompat for the specified point.
ASH_PUBLIC_EXPORT int FrameBorderNonClientHitTest(
    views::NonClientFrameView* view,
    const gfx::Point& point_in_widget);

// Resolve the inferred opacity and updates the params.
ASH_PUBLIC_EXPORT void ResolveInferredOpacity(
    views::Widget::InitParams* params);

// Checks whether we should draw the restored window frame on |widget|.
ASH_PUBLIC_EXPORT bool ShouldUseRestoreFrame(const views::Widget* widget);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_FRAME_UTILS_H_
