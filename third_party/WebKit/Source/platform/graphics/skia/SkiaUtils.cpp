/*
 * Copyright (c) 2006,2007,2008, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/graphics/skia/SkiaUtils.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/PaintFlags.h"
#include "third_party/skia/include/effects/SkCornerPathEffect.h"

namespace blink {

static const struct CompositOpToXfermodeMode {
  CompositeOperator composit_op;
  SkBlendMode xfermode_mode;
} kGMapCompositOpsToXfermodeModes[] = {
    {kCompositeClear, SkBlendMode::kClear},
    {kCompositeCopy, SkBlendMode::kSrc},
    {kCompositeSourceOver, SkBlendMode::kSrcOver},
    {kCompositeSourceIn, SkBlendMode::kSrcIn},
    {kCompositeSourceOut, SkBlendMode::kSrcOut},
    {kCompositeSourceAtop, SkBlendMode::kSrcATop},
    {kCompositeDestinationOver, SkBlendMode::kDstOver},
    {kCompositeDestinationIn, SkBlendMode::kDstIn},
    {kCompositeDestinationOut, SkBlendMode::kDstOut},
    {kCompositeDestinationAtop, SkBlendMode::kDstATop},
    {kCompositeXOR, SkBlendMode::kXor},
    {kCompositePlusLighter, SkBlendMode::kPlus}};

// Keep this array in sync with the WebBlendMode enum in
// public/platform/WebBlendMode.h.
static const SkBlendMode kGMapBlendOpsToXfermodeModes[] = {
    SkBlendMode::kSrcOver,     // WebBlendModeNormal
    SkBlendMode::kMultiply,    // WebBlendModeMultiply
    SkBlendMode::kScreen,      // WebBlendModeScreen
    SkBlendMode::kOverlay,     // WebBlendModeOverlay
    SkBlendMode::kDarken,      // WebBlendModeDarken
    SkBlendMode::kLighten,     // WebBlendModeLighten
    SkBlendMode::kColorDodge,  // WebBlendModeColorDodge
    SkBlendMode::kColorBurn,   // WebBlendModeColorBurn
    SkBlendMode::kHardLight,   // WebBlendModeHardLight
    SkBlendMode::kSoftLight,   // WebBlendModeSoftLight
    SkBlendMode::kDifference,  // WebBlendModeDifference
    SkBlendMode::kExclusion,   // WebBlendModeExclusion
    SkBlendMode::kHue,         // WebBlendModeHue
    SkBlendMode::kSaturation,  // WebBlendModeSaturation
    SkBlendMode::kColor,       // WebBlendModeColor
    SkBlendMode::kLuminosity   // WebBlendModeLuminosity
};

SkBlendMode WebCoreCompositeToSkiaComposite(CompositeOperator op,
                                            WebBlendMode blend_mode) {
  DCHECK(op == kCompositeSourceOver || blend_mode == WebBlendMode::kNormal);
  if (blend_mode != WebBlendMode::kNormal) {
    if (static_cast<uint8_t>(blend_mode) >=
        SK_ARRAY_COUNT(kGMapBlendOpsToXfermodeModes)) {
      SkDEBUGF(
          ("GraphicsContext::setPlatformCompositeOperation unknown "
           "WebBlendMode %d\n",
           blend_mode));
      return SkBlendMode::kSrcOver;
    }
    return kGMapBlendOpsToXfermodeModes[static_cast<uint8_t>(blend_mode)];
  }

  const CompositOpToXfermodeMode* table = kGMapCompositOpsToXfermodeModes;
  if (static_cast<uint8_t>(op) >=
      SK_ARRAY_COUNT(kGMapCompositOpsToXfermodeModes)) {
    SkDEBUGF(
        ("GraphicsContext::setPlatformCompositeOperation unknown "
         "CompositeOperator %d\n",
         op));
    return SkBlendMode::kSrcOver;
  }
  SkASSERT(table[static_cast<uint8_t>(op)].composit_op == op);
  return table[static_cast<uint8_t>(op)].xfermode_mode;
}

CompositeOperator CompositeOperatorFromSkia(SkBlendMode xfer_mode) {
  switch (xfer_mode) {
    case SkBlendMode::kClear:
      return kCompositeClear;
    case SkBlendMode::kSrc:
      return kCompositeCopy;
    case SkBlendMode::kSrcOver:
      return kCompositeSourceOver;
    case SkBlendMode::kSrcIn:
      return kCompositeSourceIn;
    case SkBlendMode::kSrcOut:
      return kCompositeSourceOut;
    case SkBlendMode::kSrcATop:
      return kCompositeSourceAtop;
    case SkBlendMode::kDstOver:
      return kCompositeDestinationOver;
    case SkBlendMode::kDstIn:
      return kCompositeDestinationIn;
    case SkBlendMode::kDstOut:
      return kCompositeDestinationOut;
    case SkBlendMode::kDstATop:
      return kCompositeDestinationAtop;
    case SkBlendMode::kXor:
      return kCompositeXOR;
    case SkBlendMode::kPlus:
      return kCompositePlusLighter;
    default:
      break;
  }
  return kCompositeSourceOver;
}

WebBlendMode BlendModeFromSkia(SkBlendMode xfer_mode) {
  switch (xfer_mode) {
    case SkBlendMode::kSrcOver:
      return WebBlendMode::kNormal;
    case SkBlendMode::kMultiply:
      return WebBlendMode::kMultiply;
    case SkBlendMode::kScreen:
      return WebBlendMode::kScreen;
    case SkBlendMode::kOverlay:
      return WebBlendMode::kOverlay;
    case SkBlendMode::kDarken:
      return WebBlendMode::kDarken;
    case SkBlendMode::kLighten:
      return WebBlendMode::kLighten;
    case SkBlendMode::kColorDodge:
      return WebBlendMode::kColorDodge;
    case SkBlendMode::kColorBurn:
      return WebBlendMode::kColorBurn;
    case SkBlendMode::kHardLight:
      return WebBlendMode::kHardLight;
    case SkBlendMode::kSoftLight:
      return WebBlendMode::kSoftLight;
    case SkBlendMode::kDifference:
      return WebBlendMode::kDifference;
    case SkBlendMode::kExclusion:
      return WebBlendMode::kExclusion;
    case SkBlendMode::kHue:
      return WebBlendMode::kHue;
    case SkBlendMode::kSaturation:
      return WebBlendMode::kSaturation;
    case SkBlendMode::kColor:
      return WebBlendMode::kColor;
    case SkBlendMode::kLuminosity:
      return WebBlendMode::kLuminosity;
    default:
      break;
  }
  return WebBlendMode::kNormal;
}

SkMatrix AffineTransformToSkMatrix(const AffineTransform& source) {
  SkMatrix result;

  result.setScaleX(WebCoreDoubleToSkScalar(source.A()));
  result.setSkewX(WebCoreDoubleToSkScalar(source.C()));
  result.setTranslateX(WebCoreDoubleToSkScalar(source.E()));

  result.setScaleY(WebCoreDoubleToSkScalar(source.D()));
  result.setSkewY(WebCoreDoubleToSkScalar(source.B()));
  result.setTranslateY(WebCoreDoubleToSkScalar(source.F()));

  // FIXME: Set perspective properly.
  result.setPerspX(0);
  result.setPerspY(0);
  result.set(SkMatrix::kMPersp2, SK_Scalar1);

  return result;
}

bool NearlyIntegral(float value) {
  return fabs(value - floorf(value)) < std::numeric_limits<float>::epsilon();
}

InterpolationQuality LimitInterpolationQuality(
    const GraphicsContext& context,
    InterpolationQuality resampling) {
  return std::min(resampling, context.ImageInterpolationQuality());
}

InterpolationQuality ComputeInterpolationQuality(float src_width,
                                                 float src_height,
                                                 float dest_width,
                                                 float dest_height,
                                                 bool is_data_complete) {
  // The percent change below which we will not resample. This usually means
  // an off-by-one error on the web page, and just doing nearest neighbor
  // sampling is usually good enough.
  const float kFractionalChangeThreshold = 0.025f;

  // Images smaller than this in either direction are considered "small" and
  // are not resampled ever (see below).
  const int kSmallImageSizeThreshold = 8;

  // The amount an image can be stretched in a single direction before we
  // say that it is being stretched so much that it must be a line or
  // background that doesn't need resampling.
  const float kLargeStretch = 3.0f;

  // Figure out if we should resample this image. We try to prune out some
  // common cases where resampling won't give us anything, since it is much
  // slower than drawing stretched.
  float diff_width = fabs(dest_width - src_width);
  float diff_height = fabs(dest_height - src_height);
  bool width_nearly_equal = diff_width < std::numeric_limits<float>::epsilon();
  bool height_nearly_equal =
      diff_height < std::numeric_limits<float>::epsilon();
  // We don't need to resample if the source and destination are the same.
  if (width_nearly_equal && height_nearly_equal)
    return kInterpolationNone;

  if (src_width <= kSmallImageSizeThreshold ||
      src_height <= kSmallImageSizeThreshold ||
      dest_width <= kSmallImageSizeThreshold ||
      dest_height <= kSmallImageSizeThreshold) {
    // Small image detected.

    // Resample in the case where the new size would be non-integral.
    // This can cause noticeable breaks in repeating patterns, except
    // when the source image is only one pixel wide in that dimension.
    if ((!NearlyIntegral(dest_width) &&
         src_width > 1 + std::numeric_limits<float>::epsilon()) ||
        (!NearlyIntegral(dest_height) &&
         src_height > 1 + std::numeric_limits<float>::epsilon()))
      return kInterpolationLow;

    // Otherwise, don't resample small images. These are often used for
    // borders and rules (think 1x1 images used to make lines).
    return kInterpolationNone;
  }

  if (src_height * kLargeStretch <= dest_height ||
      src_width * kLargeStretch <= dest_width) {
    // Large image detected.

    // Don't resample if it is being stretched a lot in only one direction.
    // This is trying to catch cases where somebody has created a border
    // (which might be large) and then is stretching it to fill some part
    // of the page.
    if (width_nearly_equal || height_nearly_equal)
      return kInterpolationNone;

    // The image is growing a lot and in more than one direction. Resampling
    // is slow and doesn't give us very much when growing a lot.
    return kInterpolationLow;
  }

  if ((diff_width / src_width < kFractionalChangeThreshold) &&
      (diff_height / src_height < kFractionalChangeThreshold)) {
    // It is disappointingly common on the web for image sizes to be off by
    // one or two pixels. We don't bother resampling if the size difference
    // is a small fraction of the original size.
    return kInterpolationNone;
  }

  // When the image is not yet done loading, use linear. We don't cache the
  // partially resampled images, and as they come in incrementally, it causes
  // us to have to resample the whole thing every time.
  if (!is_data_complete)
    return kInterpolationLow;

  // Everything else gets resampled at high quality.
  return kInterpolationHigh;
}

int ClampedAlphaForBlending(float alpha) {
  if (alpha < 0)
    return 0;
  int rounded_alpha = roundf(alpha * 256);
  if (rounded_alpha > 256)
    rounded_alpha = 256;
  return rounded_alpha;
}

SkColor ScaleAlpha(SkColor color, float alpha) {
  return ScaleAlpha(color, ClampedAlphaForBlending(alpha));
}

SkColor ScaleAlpha(SkColor color, int alpha) {
  int a = (SkColorGetA(color) * alpha) >> 8;
  return (color & 0x00FFFFFF) | (a << 24);
}

template <typename PrimitiveType>
void DrawFocusRingPrimitive(const PrimitiveType&,
                            PaintCanvas*,
                            const PaintFlags&,
                            float corner_radius) {
  NOTREACHED();  // Missing an explicit specialization?
}

template <>
void DrawFocusRingPrimitive<SkRect>(const SkRect& rect,
                                    PaintCanvas* canvas,
                                    const PaintFlags& flags,
                                    float corner_radius) {
  SkRRect rrect;
  rrect.setRectXY(rect, SkFloatToScalar(corner_radius),
                  SkFloatToScalar(corner_radius));
  canvas->drawRRect(rrect, flags);
}

template <>
void DrawFocusRingPrimitive<SkPath>(const SkPath& path,
                                    PaintCanvas* canvas,
                                    const PaintFlags& flags,
                                    float corner_radius) {
  PaintFlags path_flags = flags;
  path_flags.setPathEffect(
      SkCornerPathEffect::Make(SkFloatToScalar(corner_radius)));
  canvas->drawPath(path, path_flags);
}

template <typename PrimitiveType>
void DrawPlatformFocusRing(const PrimitiveType& primitive,
                           PaintCanvas* canvas,
                           SkColor color,
                           float width) {
  PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(PaintFlags::kStroke_Style);
  flags.setColor(color);
  flags.setStrokeWidth(width);

#if OS(MACOSX)
  flags.setAlpha(64);
  const float corner_radius = (width - 1) * 0.5f;
#else
  const float corner_radius = width;
#endif

  DrawFocusRingPrimitive(primitive, canvas, flags, corner_radius);

#if OS(MACOSX)
  // Inner part
  flags.setAlpha(128);
  flags.setStrokeWidth(flags.getStrokeWidth() * 0.5f);
  DrawFocusRingPrimitive(primitive, canvas, flags, corner_radius);
#endif
}

template void PLATFORM_EXPORT DrawPlatformFocusRing<SkRect>(const SkRect&,
                                                            PaintCanvas*,
                                                            SkColor,
                                                            float width);
template void PLATFORM_EXPORT DrawPlatformFocusRing<SkPath>(const SkPath&,
                                                            PaintCanvas*,
                                                            SkColor,
                                                            float width);

}  // namespace blink
