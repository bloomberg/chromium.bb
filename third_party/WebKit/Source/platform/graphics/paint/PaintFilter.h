// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintFilter_h
#define PaintFilter_h

#include "cc/paint/paint_filter.h"

namespace blink {
using cc::PaintFilter;
using cc::ColorFilterPaintFilter;
using cc::BlurPaintFilter;
using cc::DropShadowPaintFilter;
using cc::MagnifierPaintFilter;
using cc::ComposePaintFilter;
using cc::AlphaThresholdPaintFilter;
using cc::XfermodePaintFilter;
using cc::ArithmeticPaintFilter;
using cc::MatrixConvolutionPaintFilter;
using cc::DisplacementMapEffectPaintFilter;
using cc::ImagePaintFilter;
using cc::RecordPaintFilter;
using cc::MergePaintFilter;
using cc::MorphologyPaintFilter;
using cc::OffsetPaintFilter;
using cc::TilePaintFilter;
using cc::TurbulencePaintFilter;
using cc::PaintFlagsPaintFilter;
using cc::MatrixPaintFilter;
using cc::LightingDistantPaintFilter;
using cc::LightingPointPaintFilter;
using cc::LightingSpotPaintFilter;
}  // namespace blink

#endif  // PaintFilter_h
