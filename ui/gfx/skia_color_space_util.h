// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SKIA_COLOR_SPACE_UTIL_H_
#define UI_GFX_SKIA_COLOR_SPACE_UTIL_H_

#include "third_party/skia/include/core/SkColorSpace.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

float GFX_EXPORT EvalSkTransferFn(const SkColorSpaceTransferFn& fn, float x);

SkColorSpaceTransferFn GFX_EXPORT
SkTransferFnInverse(const SkColorSpaceTransferFn& fn);

bool GFX_EXPORT SkMatrixIsApproximatelyIdentity(const SkMatrix44& m);

}  // namespace gfx

#endif  // UI_GFX_SKIA_COLOR_SPACE_UTIL_H_
