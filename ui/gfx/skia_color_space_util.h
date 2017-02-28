// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SKIA_COLOR_SPACE_UTIL_H_
#define UI_GFX_SKIA_COLOR_SPACE_UTIL_H_

#include "third_party/skia/include/core/SkColorSpace.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

float GFX_EXPORT SkTransferFnEval(const SkColorSpaceTransferFn& fn, float x);

SkColorSpaceTransferFn GFX_EXPORT
SkTransferFnInverse(const SkColorSpaceTransferFn& fn);

bool GFX_EXPORT
SkTransferFnsApproximatelyCancel(const SkColorSpaceTransferFn& a,
                                 const SkColorSpaceTransferFn& b);

bool GFX_EXPORT
SkTransferFnIsApproximatelyIdentity(const SkColorSpaceTransferFn& fn);

// Approximates the |n| points in |x| and |t| by the transfer function |fn|.
// Returns true if the approximation converged.
bool GFX_EXPORT SkApproximateTransferFn(const float* x,
                                        const float* t,
                                        size_t n,
                                        SkColorSpaceTransferFn* fn);

bool GFX_EXPORT SkMatrixIsApproximatelyIdentity(const SkMatrix44& m);

}  // namespace gfx

#endif  // UI_GFX_SKIA_COLOR_SPACE_UTIL_H_
