// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SKIA_COLOR_SPACE_UTIL_H_
#define UI_GFX_SKIA_COLOR_SPACE_UTIL_H_

#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkICC.h"
#include "ui/gfx/color_space_export.h"

namespace gfx {

float COLOR_SPACE_EXPORT SkTransferFnEval(const SkColorSpaceTransferFn& fn,
                                          float x);

SkColorSpaceTransferFn COLOR_SPACE_EXPORT
SkTransferFnInverse(const SkColorSpaceTransferFn& fn);

bool COLOR_SPACE_EXPORT
SkTransferFnsApproximatelyCancel(const SkColorSpaceTransferFn& a,
                                 const SkColorSpaceTransferFn& b);

bool COLOR_SPACE_EXPORT
SkTransferFnIsApproximatelyIdentity(const SkColorSpaceTransferFn& fn);

// Approximates the |n| points in |x| and |t| by the transfer function |fn|.
// Returns true if the approximation converged.
bool COLOR_SPACE_EXPORT SkApproximateTransferFn(const float* x,
                                                const float* t,
                                                size_t n,
                                                SkColorSpaceTransferFn* fn);

// Approximates |sk_icc| by the transfer function |fn|. Returns in |max_error|
// the maximum pointwise of all color channels' transfer functions with |fn|.
// Returns false if no approximation was possible, or no approximation
// converged.
bool COLOR_SPACE_EXPORT SkApproximateTransferFn(sk_sp<SkICC> sk_icc,
                                                float* max_error,
                                                SkColorSpaceTransferFn* fn);

bool COLOR_SPACE_EXPORT SkMatrixIsApproximatelyIdentity(const SkMatrix44& m);

}  // namespace gfx

#endif  // UI_GFX_SKIA_COLOR_SPACE_UTIL_H_
