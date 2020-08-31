// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fxge/skia/fx_skia_device.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "build/build_config.h"
#include "core/fpdfapi/page/cpdf_expintfunc.h"
#include "core/fpdfapi/page/cpdf_function.h"
#include "core/fpdfapi/page/cpdf_meshstream.h"
#include "core/fpdfapi/page/cpdf_sampledfunc.h"
#include "core/fpdfapi/page/cpdf_shadingpattern.h"
#include "core/fpdfapi/page/cpdf_stitchfunc.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fpdfapi/parser/cpdf_stream_acc.h"
#include "core/fxcrt/cfx_bitstream.h"
#include "core/fxcrt/fx_memory_wrappers.h"
#include "core/fxcrt/fx_system.h"
#include "core/fxge/cfx_defaultrenderdevice.h"
#include "core/fxge/cfx_font.h"
#include "core/fxge/cfx_graphstatedata.h"
#include "core/fxge/cfx_pathdata.h"
#include "core/fxge/cfx_renderdevice.h"
#include "core/fxge/dib/cfx_bitmapcomposer.h"
#include "core/fxge/dib/cfx_dibitmap.h"
#include "core/fxge/dib/cfx_imagerenderer.h"
#include "core/fxge/dib/cfx_imagestretcher.h"
#include "core/fxge/text_char_pos.h"
#include "third_party/base/logging.h"
#include "third_party/base/ptr_util.h"
#include "third_party/base/span.h"
#include "third_party/base/stl_util.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkClipOp.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRSXform.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkTextBlob.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/effects/SkDashPathEffect.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "third_party/skia/include/pathops/SkPathOps.h"

#ifdef _SKIA_SUPPORT_PATHS_
#include "core/fxge/cfx_cliprgn.h"
#endif  // _SKIA_SUPPORT_PATHS_

#ifdef _SKIA_SUPPORT_
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkMaskFilter.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#endif  // _SKIA_SUPPORT_

namespace {

#ifdef _SKIA_SUPPORT_PATHS_
void RgbByteOrderTransferBitmap(const RetainPtr<CFX_DIBitmap>& pBitmap,
                                int dest_left,
                                int dest_top,
                                int width,
                                int height,
                                const RetainPtr<CFX_DIBBase>& pSrcBitmap,
                                int src_left,
                                int src_top) {
  if (!pBitmap)
    return;

  if (!pBitmap->GetOverlapRect(dest_left, dest_top, width, height,
                               pSrcBitmap->GetWidth(), pSrcBitmap->GetHeight(),
                               src_left, src_top, nullptr)) {
    return;
  }

  int Bpp = pBitmap->GetBPP() / 8;
  FXDIB_Format dest_format = pBitmap->GetFormat();
  FXDIB_Format src_format = pSrcBitmap->GetFormat();
  int pitch = pBitmap->GetPitch();
  uint8_t* buffer = pBitmap->GetBuffer();
  if (dest_format == src_format) {
    for (int row = 0; row < height; row++) {
      uint8_t* dest_scan = buffer + (dest_top + row) * pitch + dest_left * Bpp;
      const uint8_t* src_scan =
          pSrcBitmap->GetScanline(src_top + row) + src_left * Bpp;
      if (Bpp == 4) {
        for (int col = 0; col < width; col++) {
          FXARGB_SETDIB(dest_scan, ArgbEncode(src_scan[3], src_scan[0],
                                              src_scan[1], src_scan[2]));
          dest_scan += 4;
          src_scan += 4;
        }
      } else {
        for (int col = 0; col < width; col++) {
          *dest_scan++ = src_scan[2];
          *dest_scan++ = src_scan[1];
          *dest_scan++ = src_scan[0];
          src_scan += 3;
        }
      }
    }
    return;
  }

  uint8_t* dest_buf = buffer + dest_top * pitch + dest_left * Bpp;
  if (dest_format == FXDIB_Rgb) {
    if (src_format == FXDIB_Rgb32) {
      for (int row = 0; row < height; row++) {
        uint8_t* dest_scan = dest_buf + row * pitch;
        const uint8_t* src_scan =
            pSrcBitmap->GetScanline(src_top + row) + src_left * 4;
        for (int col = 0; col < width; col++) {
          *dest_scan++ = src_scan[2];
          *dest_scan++ = src_scan[1];
          *dest_scan++ = src_scan[0];
          src_scan += 4;
        }
      }
    } else {
      NOTREACHED();
    }
    return;
  }

  if (dest_format == FXDIB_Argb || dest_format == FXDIB_Rgb32) {
    if (src_format == FXDIB_Rgb) {
      for (int row = 0; row < height; row++) {
        uint8_t* dest_scan = (uint8_t*)(dest_buf + row * pitch);
        const uint8_t* src_scan =
            pSrcBitmap->GetScanline(src_top + row) + src_left * 3;
        for (int col = 0; col < width; col++) {
          FXARGB_SETDIB(dest_scan, ArgbEncode(0xff, src_scan[0], src_scan[1],
                                              src_scan[2]));
          dest_scan += 4;
          src_scan += 3;
        }
      }
    } else if (src_format == FXDIB_Rgb32) {
      ASSERT(dest_format == FXDIB_Argb);
      for (int row = 0; row < height; row++) {
        uint8_t* dest_scan = dest_buf + row * pitch;
        const uint8_t* src_scan =
            pSrcBitmap->GetScanline(src_top + row) + src_left * 4;
        for (int col = 0; col < width; col++) {
          FXARGB_SETDIB(dest_scan, ArgbEncode(0xff, src_scan[0], src_scan[1],
                                              src_scan[2]));
          src_scan += 4;
          dest_scan += 4;
        }
      }
    }
    return;
  }

  NOTREACHED();
}

#endif  // _SKIA_SUPPORT_PATHS_

#define SHOW_SKIA_PATH 0  // set to 1 to print the path contents
#if SHOW_SKIA_PATH
#define SHOW_SKIA_PATH_SHORTHAND 0  // set to 1 for abbreviated path contents
#endif
#define DRAW_SKIA_CLIP 0  // set to 1 to draw a green rectangle around the clip
#define SHOW_TEXT_GLYPHS 0  // set to 1 to print unichar equivalent of glyph

#if SHOW_SKIA_PATH
void DebugShowSkiaPaint(const SkPaint& paint) {
  if (SkPaint::kFill_Style == paint.getStyle()) {
    printf("fill 0x%08x\n", paint.getColor());
  } else {
    printf("stroke 0x%08x width %g\n", paint.getColor(),
           paint.getStrokeWidth());
  }
}
#endif  // SHOW_SKIA_PATH

void DebugShowSkiaPath(const SkPath& path) {
#if SHOW_SKIA_PATH
#if SHOW_SKIA_PATH_SHORTHAND
  printf(" **\n");
#else
  SkDynamicMemoryWStream stream;
  path.dump(&stream, false, false);
  std::unique_ptr<char, FxFreeDeleter> storage;
  storage.reset(FX_Alloc(char, stream.bytesWritten()));
  stream.copyTo(storage.get());
  printf("%.*s", (int)stream.bytesWritten(), storage.get());
#endif  // SHOW_SKIA_PATH_SHORTHAND
#endif  // SHOW_SKIA_PATH
}

void DebugShowCanvasClip(CFX_SkiaDeviceDriver* driver, const SkCanvas* canvas) {
#if SHOW_SKIA_PATH
  SkMatrix matrix = canvas->getTotalMatrix();
  SkScalar m[9];
  matrix.get9(m);
  printf("matrix (%g,%g,%g) (%g,%g,%g) (%g,%g,%g)\n", m[0], m[1], m[2], m[3],
         m[4], m[5], m[6], m[7], m[8]);
  SkRect local = canvas->getLocalClipBounds();
  SkIRect device = canvas->getDeviceClipBounds();

  printf("local bounds %g %g %g %g\n", local.fLeft, local.fTop, local.fRight,
         local.fBottom);
  printf("device bounds %d %d %d %d\n", device.fLeft, device.fTop,
         device.fRight, device.fBottom);
  FX_RECT clipBox;
  driver->GetClipBox(&clipBox);
  printf("reported bounds %d %d %d %d\n", clipBox.left, clipBox.top,
         clipBox.right, clipBox.bottom);
#endif  // SHOW_SKIA_PATH
}

void DebugShowSkiaDrawPath(CFX_SkiaDeviceDriver* driver,
                           const SkCanvas* canvas,
                           const SkPaint& paint,
                           const SkPath& path) {
#if SHOW_SKIA_PATH
  DebugShowSkiaPaint(paint);
  DebugShowCanvasClip(driver, canvas);
  DebugShowSkiaPath(path);
  printf("\n");
#endif  // SHOW_SKIA_PATH
}

void DebugShowSkiaDrawRect(CFX_SkiaDeviceDriver* driver,
                           const SkCanvas* canvas,
                           const SkPaint& paint,
                           const SkRect& rect) {
#if SHOW_SKIA_PATH
  DebugShowSkiaPaint(paint);
  DebugShowCanvasClip(driver, canvas);
  printf("rect %g %g %g %g\n", rect.fLeft, rect.fTop, rect.fRight,
         rect.fBottom);
#endif  // SHOW_SKIA_PATH
}

#if DRAW_SKIA_CLIP

SkPaint DebugClipPaint() {
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor(SK_ColorGREEN);
  paint.setStyle(SkPaint::kStroke_Style);
  return paint;
}

void DebugDrawSkiaClipRect(SkCanvas* canvas, const SkRect& rect) {
  SkPaint paint = DebugClipPaint();
  canvas->drawRect(rect, paint);
}

void DebugDrawSkiaClipPath(SkCanvas* canvas, const SkPath& path) {
  SkPaint paint = DebugClipPaint();
  canvas->drawPath(path, paint);
}

#else  // DRAW_SKIA_CLIP

void DebugDrawSkiaClipRect(SkCanvas* canvas, const SkRect& rect) {}

void DebugDrawSkiaClipPath(SkCanvas* canvas, const SkPath& path) {}

#endif  // DRAW_SKIA_CLIP

#ifdef _SKIA_SUPPORT_
static void DebugValidate(const RetainPtr<CFX_DIBitmap>& bitmap,
                          const RetainPtr<CFX_DIBitmap>& device) {
  if (bitmap) {
    ASSERT(bitmap->GetBPP() == 8 || bitmap->GetBPP() == 32);
    if (bitmap->GetBPP() == 32) {
      bitmap->DebugVerifyBitmapIsPreMultiplied(nullptr);
    }
  }
  if (device) {
    ASSERT(device->GetBPP() == 8 || device->GetBPP() == 32);
    if (device->GetBPP() == 32) {
      device->DebugVerifyBitmapIsPreMultiplied(nullptr);
    }
  }
}
#endif  // _SKIA_SUPPORT_

constexpr int kAlternateOrWindingFillModeMask =
    FXFILL_ALTERNATE | FXFILL_WINDING;

SkColorType Get32BitSkColorType(bool is_rgb_byte_order) {
  return is_rgb_byte_order ? kRGBA_8888_SkColorType : kBGRA_8888_SkColorType;
}

int GetAlternateOrWindingFillMode(int fill_mode) {
  return fill_mode & kAlternateOrWindingFillModeMask;
}

bool IsAlternateFillMode(int fill_mode) {
  // TODO(thestig): This function should be able to assert
  // GetAlternateOrWindingFillMode(fill_mode) != 0.
  return GetAlternateOrWindingFillMode(fill_mode) == FXFILL_ALTERNATE;
}

SkPathFillType GetAlternateOrWindingFillType(int fill_mode) {
  return IsAlternateFillMode(fill_mode) ? SkPathFillType::kEvenOdd
                                        : SkPathFillType::kWinding;
}

bool IsEvenOddFillType(SkPathFillType fill) {
  return fill == SkPathFillType::kEvenOdd ||
         fill == SkPathFillType::kInverseEvenOdd;
}

SkPath BuildPath(const CFX_PathData* pPathData) {
  SkPath sk_path;
  pdfium::span<const FX_PATHPOINT> points = pPathData->GetPoints();
  for (size_t i = 0; i < points.size(); ++i) {
    const CFX_PointF& point = points[i].m_Point;
    FXPT_TYPE point_type = points[i].m_Type;
    if (point_type == FXPT_TYPE::MoveTo) {
      sk_path.moveTo(point.x, point.y);
    } else if (point_type == FXPT_TYPE::LineTo) {
      sk_path.lineTo(point.x, point.y);
    } else if (point_type == FXPT_TYPE::BezierTo) {
      const CFX_PointF& point2 = points[i + 1].m_Point;
      const CFX_PointF& point3 = points[i + 2].m_Point;
      sk_path.cubicTo(point.x, point.y, point2.x, point2.y, point3.x, point3.y);
      i += 2;
    }
    if (points[i].m_CloseFigure)
      sk_path.close();
  }
  return sk_path;
}

SkMatrix ToSkMatrix(const CFX_Matrix& m) {
  SkMatrix skMatrix;
  skMatrix.setAll(m.a, m.c, m.e, m.b, m.d, m.f, 0, 0, 1);
  return skMatrix;
}

// use when pdf's y-axis points up instead of down
SkMatrix ToFlippedSkMatrix(const CFX_Matrix& m, SkScalar flip) {
  SkMatrix skMatrix;
  skMatrix.setAll(m.a * flip, -m.c * flip, m.e, m.b * flip, -m.d * flip, m.f, 0,
                  0, 1);
  return skMatrix;
}

SkBlendMode GetSkiaBlendMode(BlendMode blend_type) {
  switch (blend_type) {
    case BlendMode::kMultiply:
      return SkBlendMode::kMultiply;
    case BlendMode::kScreen:
      return SkBlendMode::kScreen;
    case BlendMode::kOverlay:
      return SkBlendMode::kOverlay;
    case BlendMode::kDarken:
      return SkBlendMode::kDarken;
    case BlendMode::kLighten:
      return SkBlendMode::kLighten;
    case BlendMode::kColorDodge:
      return SkBlendMode::kColorDodge;
    case BlendMode::kColorBurn:
      return SkBlendMode::kColorBurn;
    case BlendMode::kHardLight:
      return SkBlendMode::kHardLight;
    case BlendMode::kSoftLight:
      return SkBlendMode::kSoftLight;
    case BlendMode::kDifference:
      return SkBlendMode::kDifference;
    case BlendMode::kExclusion:
      return SkBlendMode::kExclusion;
    case BlendMode::kHue:
      return SkBlendMode::kHue;
    case BlendMode::kSaturation:
      return SkBlendMode::kSaturation;
    case BlendMode::kColor:
      return SkBlendMode::kColor;
    case BlendMode::kLuminosity:
      return SkBlendMode::kLuminosity;
    case BlendMode::kNormal:
    default:
      return SkBlendMode::kSrcOver;
  }
}

// Add begin & end colors into |skColors| array for each gradient transition.
//
// |is_encode_reversed| must be set to true when the parent function of |pFunc|
// has an Encode array, and the matching pair of encode values for |pFunc| are
// in decreasing order.
bool AddColors(const CPDF_ExpIntFunc* pFunc,
               SkTDArray<SkColor>* skColors,
               bool is_encode_reversed) {
  if (pFunc->CountInputs() != 1)
    return false;
  if (pFunc->m_Exponent != 1)
    return false;
  if (pFunc->m_nOrigOutputs != 3)
    return false;

  auto begin_values = pFunc->m_BeginValues.begin();
  auto end_values = pFunc->m_EndValues.begin();
  if (is_encode_reversed)
    std::swap(begin_values, end_values);

  skColors->push_back(SkColorSetARGB(0xFF,
                                     SkUnitScalarClampToByte(begin_values[0]),
                                     SkUnitScalarClampToByte(begin_values[1]),
                                     SkUnitScalarClampToByte(begin_values[2])));
  skColors->push_back(SkColorSetARGB(0xFF,
                                     SkUnitScalarClampToByte(end_values[0]),
                                     SkUnitScalarClampToByte(end_values[1]),
                                     SkUnitScalarClampToByte(end_values[2])));
  return true;
}

uint8_t FloatToByte(float f) {
  ASSERT(f >= 0);
  ASSERT(f <= 1);
  return (uint8_t)(f * 255.99f);
}

bool AddSamples(const CPDF_SampledFunc* pFunc,
                SkTDArray<SkColor>* skColors,
                SkTDArray<SkScalar>* skPos) {
  if (pFunc->CountInputs() != 1)
    return false;
  if (pFunc->CountOutputs() != 3)  // expect rgb
    return false;
  if (pFunc->GetEncodeInfo().empty())
    return false;
  const CPDF_SampledFunc::SampleEncodeInfo& encodeInfo =
      pFunc->GetEncodeInfo()[0];
  if (encodeInfo.encode_min != 0)
    return false;
  if (encodeInfo.encode_max != encodeInfo.sizes - 1)
    return false;
  uint32_t sampleSize = pFunc->GetBitsPerSample();
  uint32_t sampleCount = encodeInfo.sizes;
  if (sampleCount != 1U << sampleSize)
    return false;
  if (pFunc->GetSampleStream()->GetSize() < sampleCount * 3 * sampleSize / 8)
    return false;

  float colorsMin[3];
  float colorsMax[3];
  for (int i = 0; i < 3; ++i) {
    colorsMin[i] = pFunc->GetRange(i * 2);
    colorsMax[i] = pFunc->GetRange(i * 2 + 1);
  }
  pdfium::span<const uint8_t> pSampleData = pFunc->GetSampleStream()->GetSpan();
  CFX_BitStream bitstream(pSampleData);
  for (uint32_t i = 0; i < sampleCount; ++i) {
    float floatColors[3];
    for (uint32_t j = 0; j < 3; ++j) {
      float sample = static_cast<float>(bitstream.GetBits(sampleSize));
      float interp = sample / (sampleCount - 1);
      floatColors[j] = colorsMin[j] + (colorsMax[j] - colorsMin[j]) * interp;
    }
    SkColor color =
        SkPackARGB32(0xFF, FloatToByte(floatColors[0]),
                     FloatToByte(floatColors[1]), FloatToByte(floatColors[2]));
    skColors->push_back(color);
    skPos->push_back((float)i / (sampleCount - 1));
  }
  return true;
}

bool AddStitching(const CPDF_StitchFunc* pFunc,
                  SkTDArray<SkColor>* skColors,
                  SkTDArray<SkScalar>* skPos) {
  float boundsStart = pFunc->GetDomain(0);

  const auto& subFunctions = pFunc->GetSubFunctions();
  int subFunctionCount = subFunctions.size();
  for (int i = 0; i < subFunctionCount; ++i) {
    const CPDF_ExpIntFunc* pSubFunc = subFunctions[i]->ToExpIntFunc();
    if (!pSubFunc)
      return false;
    // Check if the matching encode values are reversed
    bool is_encode_reversed =
        pFunc->GetEncode(2 * i) > pFunc->GetEncode(2 * i + 1);
    if (!AddColors(pSubFunc, skColors, is_encode_reversed))
      return false;
    float boundsEnd =
        i < subFunctionCount - 1 ? pFunc->GetBound(i + 1) : pFunc->GetDomain(1);
    skPos->push_back(boundsStart);
    skPos->push_back(boundsEnd);
    boundsStart = boundsEnd;
  }
  return true;
}

// see https://en.wikipedia.org/wiki/Distance_from_a_point_to_a_line
SkScalar LineSide(const SkPoint line[2], const SkPoint& pt) {
  return (line[1].fY - line[0].fY) * pt.fX - (line[1].fX - line[0].fX) * pt.fY +
         line[1].fX * line[0].fY - line[1].fY * line[0].fX;
}

SkPoint IntersectSides(const SkPoint& parallelPt,
                       const SkVector& paraRay,
                       const SkPoint& perpendicularPt) {
  SkVector perpRay = {paraRay.fY, -paraRay.fX};
  SkScalar denom = perpRay.fY * paraRay.fX - paraRay.fY * perpRay.fX;
  if (!denom) {
    SkPoint zeroPt = {0, 0};
    return zeroPt;
  }
  SkVector ab0 = parallelPt - perpendicularPt;
  SkScalar numerA = ab0.fY * perpRay.fX - perpRay.fY * ab0.fX;
  numerA /= denom;
  SkPoint result = {parallelPt.fX + paraRay.fX * numerA,
                    parallelPt.fY + paraRay.fY * numerA};
  return result;
}

void ClipAngledGradient(const SkPoint pts[2],
                        SkPoint rectPts[4],
                        bool clipStart,
                        bool clipEnd,
                        SkPath* clip) {
  // find the corners furthest from the gradient perpendiculars
  SkScalar minPerpDist = SK_ScalarMax;
  SkScalar maxPerpDist = SK_ScalarMin;
  int minPerpPtIndex = -1;
  int maxPerpPtIndex = -1;
  SkVector slope = pts[1] - pts[0];
  SkPoint startPerp[2] = {pts[0], {pts[0].fX + slope.fY, pts[0].fY - slope.fX}};
  SkPoint endPerp[2] = {pts[1], {pts[1].fX + slope.fY, pts[1].fY - slope.fX}};
  for (int i = 0; i < 4; ++i) {
    SkScalar sDist = LineSide(startPerp, rectPts[i]);
    SkScalar eDist = LineSide(endPerp, rectPts[i]);
    if (sDist * eDist <= 0)  // if the signs are different,
      continue;              // the point is inside the gradient
    if (sDist < 0) {
      SkScalar smaller = std::min(sDist, eDist);
      if (minPerpDist > smaller) {
        minPerpDist = smaller;
        minPerpPtIndex = i;
      }
    } else {
      SkScalar larger = std::max(sDist, eDist);
      if (maxPerpDist < larger) {
        maxPerpDist = larger;
        maxPerpPtIndex = i;
      }
    }
  }
  if (minPerpPtIndex < 0 && maxPerpPtIndex < 0)  // nothing's outside
    return;

  // determine if negative distances are before start or after end
  SkPoint beforeStart = {pts[0].fX * 2 - pts[1].fX, pts[0].fY * 2 - pts[1].fY};
  bool beforeNeg = LineSide(startPerp, beforeStart) < 0;

  int noClipStartIndex = maxPerpPtIndex;
  int noClipEndIndex = minPerpPtIndex;
  if (beforeNeg)
    std::swap(noClipStartIndex, noClipEndIndex);
  if ((!clipStart && noClipStartIndex < 0) ||
      (!clipEnd && noClipEndIndex < 0)) {
    return;
  }

  const SkPoint& startEdgePt = clipStart ? pts[0] : rectPts[noClipStartIndex];
  const SkPoint& endEdgePt = clipEnd ? pts[1] : rectPts[noClipEndIndex];

  // find the corners that bound the gradient
  SkScalar minDist = SK_ScalarMax;
  SkScalar maxDist = SK_ScalarMin;
  int minBounds = -1;
  int maxBounds = -1;
  for (int i = 0; i < 4; ++i) {
    SkScalar dist = LineSide(pts, rectPts[i]);
    if (minDist > dist) {
      minDist = dist;
      minBounds = i;
    }
    if (maxDist < dist) {
      maxDist = dist;
      maxBounds = i;
    }
  }
  if (minBounds < 0 || maxBounds < 0)
    return;
  if (minBounds == maxBounds)
    return;
  // construct a clip parallel to the gradient that goes through
  // rectPts[minBounds] and rectPts[maxBounds] and perpendicular to the
  // gradient that goes through startEdgePt, endEdgePt.
  clip->moveTo(IntersectSides(rectPts[minBounds], slope, startEdgePt));
  clip->lineTo(IntersectSides(rectPts[minBounds], slope, endEdgePt));
  clip->lineTo(IntersectSides(rectPts[maxBounds], slope, endEdgePt));
  clip->lineTo(IntersectSides(rectPts[maxBounds], slope, startEdgePt));
}

#ifdef _SKIA_SUPPORT_
void SetBitmapMatrix(const CFX_Matrix& m,
                     int width,
                     int height,
                     SkMatrix* skMatrix) {
  skMatrix->setAll(m.a / width, -m.c / height, m.c + m.e, m.b / width,
                   -m.d / height, m.d + m.f, 0, 0, 1);
}

void SetBitmapPaint(bool isAlphaMask,
                    uint32_t argb,
                    int bitmap_alpha,
                    BlendMode blend_type,
                    SkPaint* paint) {
  paint->setAntiAlias(true);
  if (isAlphaMask)
    paint->setColorFilter(SkColorFilters::Blend(argb, SkBlendMode::kSrc));

  // paint->setFilterQuality(kHigh_SkFilterQuality);
  paint->setBlendMode(GetSkiaBlendMode(blend_type));
  paint->setAlpha(bitmap_alpha);
}

bool Upsample(const RetainPtr<CFX_DIBBase>& pSource,
              std::unique_ptr<uint8_t, FxFreeDeleter>& dst8Storage,
              std::unique_ptr<uint32_t, FxFreeDeleter>& dst32Storage,
              SkBitmap* skBitmap,
              int* widthPtr,
              int* heightPtr,
              bool forceAlpha,
              bool bRgbByteOrder) {
  void* buffer = pSource->GetBuffer();
  if (!buffer)
    return false;
  SkColorType colorType = forceAlpha || pSource->IsAlphaMask()
                              ? SkColorType::kAlpha_8_SkColorType
                              : SkColorType::kGray_8_SkColorType;
  SkAlphaType alphaType =
      pSource->IsAlphaMask() ? kPremul_SkAlphaType : kOpaque_SkAlphaType;
  int width = pSource->GetWidth();
  int height = pSource->GetHeight();
  int rowBytes = pSource->GetPitch();
  switch (pSource->GetBPP()) {
    case 1: {
      dst8Storage.reset(FX_Alloc2D(uint8_t, width, height));
      uint8_t* dst8Pixels = dst8Storage.get();
      for (int y = 0; y < height; ++y) {
        const uint8_t* srcRow =
            static_cast<const uint8_t*>(buffer) + y * rowBytes;
        uint8_t* dstRow = dst8Pixels + y * width;
        for (int x = 0; x < width; ++x)
          dstRow[x] = srcRow[x >> 3] & (1 << (~x & 0x07)) ? 0xFF : 0x00;
      }
      buffer = dst8Storage.get();
      rowBytes = width;
      break;
    }
    case 8:
      // we upscale ctables to 32bit.
      if (pSource->GetPalette()) {
        dst32Storage.reset(FX_Alloc2D(uint32_t, width, height));
        SkPMColor* dst32Pixels = dst32Storage.get();
        const SkPMColor* ctable = pSource->GetPalette();
        const unsigned ctableSize = pSource->GetPaletteSize();
        for (int y = 0; y < height; ++y) {
          const uint8_t* srcRow =
              static_cast<const uint8_t*>(buffer) + y * rowBytes;
          uint32_t* dstRow = dst32Pixels + y * width;
          for (int x = 0; x < width; ++x) {
            unsigned index = srcRow[x];
            if (index >= ctableSize) {
              index = 0;
            }
            dstRow[x] = ctable[index];
          }
        }
        buffer = dst32Storage.get();
        rowBytes = width * sizeof(uint32_t);
        colorType = Get32BitSkColorType(bRgbByteOrder);
      }
      break;
    case 24: {
      dst32Storage.reset(FX_Alloc2D(uint32_t, width, height));
      uint32_t* dst32Pixels = dst32Storage.get();
      for (int y = 0; y < height; ++y) {
        const uint8_t* srcRow =
            static_cast<const uint8_t*>(buffer) + y * rowBytes;
        uint32_t* dstRow = dst32Pixels + y * width;
        for (int x = 0; x < width; ++x) {
          dstRow[x] = SkPackARGB32(0xFF, srcRow[x * 3 + 2], srcRow[x * 3 + 1],
                                   srcRow[x * 3 + 0]);
        }
      }
      buffer = dst32Storage.get();
      rowBytes = width * sizeof(uint32_t);
      colorType = Get32BitSkColorType(bRgbByteOrder);
      alphaType = kOpaque_SkAlphaType;
      break;
    }
    case 32:
      colorType = Get32BitSkColorType(bRgbByteOrder);
      alphaType = kPremul_SkAlphaType;
      pSource->DebugVerifyBitmapIsPreMultiplied(buffer);
      break;
    default:
      NOTREACHED();  // TODO(bug_11) ensure that all cases are covered
      colorType = SkColorType::kUnknown_SkColorType;
  }
  SkImageInfo imageInfo =
      SkImageInfo::Make(width, height, colorType, alphaType);
  skBitmap->installPixels(imageInfo, buffer, rowBytes);
  *widthPtr = width;
  *heightPtr = height;
  return true;
}
#endif  // _SKIA_SUPPORT_

}  // namespace

// Encapsulate the state used for successive text and path draws so that
// they can be combined.
class SkiaState {
 public:
  enum class Clip {
    kSave,
    kPath,
  };

  enum class Accumulator {
    kNone,
    kPath,
    kText,
    kOther,
  };

  // mark all cached state as uninitialized
  explicit SkiaState(CFX_SkiaDeviceDriver* pDriver) : m_pDriver(pDriver) {}

  bool DrawPath(const CFX_PathData* pPathData,
                const CFX_Matrix* pMatrix,
                const CFX_GraphStateData* pDrawState,
                uint32_t fill_color,
                uint32_t stroke_color,
                int fill_mode,
                BlendMode blend_type) {
    if (m_debugDisable)
      return false;
    Dump(__func__);
    int drawIndex = std::min(m_drawIndex, m_commands.count());
    if (Accumulator::kText == m_type || drawIndex != m_commandIndex ||
        (Accumulator::kPath == m_type &&
         DrawChanged(pMatrix, pDrawState, fill_color, stroke_color, fill_mode,
                     blend_type, m_pDriver->GetGroupKnockout()))) {
      Flush();
    }
    if (Accumulator::kPath != m_type) {
      m_skPath.reset();
      m_fillFullCover = !!(fill_mode & FXFILL_FULLCOVER);
      m_fillPath = GetAlternateOrWindingFillMode(fill_mode) && fill_color;
      m_skPath.setFillType(GetAlternateOrWindingFillType(fill_mode));
      if (pDrawState)
        m_drawState = *pDrawState;
      m_fillColor = fill_color;
      m_strokeColor = stroke_color;
      m_blendType = blend_type;
      m_groupKnockout = m_pDriver->GetGroupKnockout();
      if (pMatrix)
        m_drawMatrix = *pMatrix;
      m_drawIndex = m_commandIndex;
      m_type = Accumulator::kPath;
    }
    SkPath skPath = BuildPath(pPathData);
    SkPoint delta;
    if (MatrixOffset(pMatrix, &delta))
      skPath.offset(delta.fX, delta.fY);
    m_skPath.addPath(skPath);
    return true;
  }

  void FlushPath() {
    Dump(__func__);
    SkMatrix skMatrix = ToSkMatrix(m_drawMatrix);
    SkPaint skPaint;
    skPaint.setAntiAlias(true);
    if (m_fillFullCover)
      skPaint.setBlendMode(SkBlendMode::kPlus);
    int stroke_alpha = FXARGB_A(m_strokeColor);
    if (stroke_alpha)
      m_pDriver->PaintStroke(&skPaint, &m_drawState, skMatrix);
    SkCanvas* skCanvas = m_pDriver->SkiaCanvas();
    SkAutoCanvasRestore scoped_save_restore(skCanvas, /*doSave=*/true);
    skCanvas->concat(skMatrix);
    bool do_stroke = true;
    if (m_fillPath) {
      SkPath strokePath;
      const SkPath* fillPath = &m_skPath;
      if (stroke_alpha) {
        if (m_groupKnockout) {
          skPaint.getFillPath(m_skPath, &strokePath);
          if (m_strokeColor == m_fillColor &&
              Op(m_skPath, strokePath, SkPathOp::kUnion_SkPathOp,
                 &strokePath)) {
            fillPath = &strokePath;
            do_stroke = false;
          } else if (Op(m_skPath, strokePath, SkPathOp::kDifference_SkPathOp,
                        &strokePath)) {
            fillPath = &strokePath;
          }
        }
      }
      skPaint.setStyle(SkPaint::kFill_Style);
      skPaint.setColor(m_fillColor);
#ifdef _SKIA_SUPPORT_PATHS_
      m_pDriver->PreMultiply();
#endif  // _SKIA_SUPPORT_PATHS_
      DebugShowSkiaDrawPath(m_pDriver.Get(), skCanvas, skPaint, *fillPath);
      skCanvas->drawPath(*fillPath, skPaint);
    }
    if (stroke_alpha && do_stroke) {
      skPaint.setStyle(SkPaint::kStroke_Style);
      skPaint.setColor(m_strokeColor);
#ifdef _SKIA_SUPPORT_PATHS_
      m_pDriver->PreMultiply();
#endif  // _SKIA_SUPPORT_PATHS_
      DebugShowSkiaDrawPath(m_pDriver.Get(), skCanvas, skPaint, m_skPath);
      skCanvas->drawPath(m_skPath, skPaint);
    }
    m_drawIndex = INT_MAX;
    m_type = Accumulator::kNone;
    m_drawMatrix = CFX_Matrix();
  }

  bool HasRSX(int nChars,
              const TextCharPos* pCharPos,
              float* scaleXPtr,
              bool* oneAtATimePtr) const {
    bool useRSXform = false;
    bool oneAtATime = false;
    float scaleX = 1;
    for (int index = 0; index < nChars; ++index) {
      const TextCharPos& cp = pCharPos[index];
      if (!cp.m_bGlyphAdjust)
        continue;
      bool upright = 0 == cp.m_AdjustMatrix[1] && 0 == cp.m_AdjustMatrix[2];
      if (cp.m_AdjustMatrix[0] != cp.m_AdjustMatrix[3]) {
        if (upright && 1 == cp.m_AdjustMatrix[3]) {
          if (1 == scaleX)
            scaleX = cp.m_AdjustMatrix[0];
          else if (scaleX != cp.m_AdjustMatrix[0])
            oneAtATime = true;
        } else {
          oneAtATime = true;
        }
      } else if (cp.m_AdjustMatrix[1] != -cp.m_AdjustMatrix[2]) {
        oneAtATime = true;
      } else {
        useRSXform = true;
      }
    }
    *oneAtATimePtr = oneAtATime;
    *scaleXPtr = oneAtATime ? 1 : scaleX;
    return oneAtATime ? false : useRSXform;
  }

  bool DrawText(int nChars,
                const TextCharPos* pCharPos,
                CFX_Font* pFont,
                const CFX_Matrix& matrix,
                float font_size,
                uint32_t color) {
    if (m_debugDisable)
      return false;
    Dump(__func__);
    float scaleX = 1;
    bool oneAtATime = false;
    bool hasRSX = HasRSX(nChars, pCharPos, &scaleX, &oneAtATime);
    if (oneAtATime) {
      Flush();
      return false;
    }
    int drawIndex = std::min(m_drawIndex, m_commands.count());
    if (Accumulator::kPath == m_type || drawIndex != m_commandIndex ||
        (Accumulator::kText == m_type &&
         (FontChanged(pFont, matrix, font_size, scaleX, color) ||
          hasRSX == m_rsxform.isEmpty()))) {
      Flush();
    }
    if (Accumulator::kText != m_type) {
      m_italicAngle = pFont->GetSubstFontItalicAngle();
      m_isSubstFontBold = pFont->IsSubstFontBold();
      m_charDetails.SetCount(0);
      m_rsxform.setCount(0);
      if (pFont->GetFaceRec())
        m_pTypeFace.reset(SkSafeRef(pFont->GetDeviceCache()));
      else
        m_pTypeFace.reset();
      m_fontSize = font_size;
      m_scaleX = scaleX;
      m_fillColor = color;
      m_drawMatrix = matrix;
      m_drawIndex = m_commandIndex;
      m_type = Accumulator::kText;
      m_pFont = pFont;
    }
    if (!hasRSX && !m_rsxform.isEmpty())
      FlushText();

    int count = m_charDetails.Count();
    m_charDetails.SetCount(nChars + count);
    if (hasRSX)
      m_rsxform.setCount(nChars + count);

    SkScalar flip = m_fontSize < 0 ? -1 : 1;
    SkScalar vFlip = flip;
    if (pFont->IsVertical())
      vFlip *= -1;
    for (int index = 0; index < nChars; ++index) {
      const TextCharPos& cp = pCharPos[index];
      int cur_index = index + count;
      m_charDetails.SetPositionAt(
          cur_index, {cp.m_Origin.x * flip, cp.m_Origin.y * vFlip});
      m_charDetails.SetGlyphAt(cur_index,
                               static_cast<uint16_t>(cp.m_GlyphIndex));
      m_charDetails.SetFontCharWidthAt(cur_index, cp.m_FontCharWidth);
#if defined(OS_MACOSX)
      if (cp.m_ExtGID) {
        m_charDetails.SetGlyphAt(cur_index, static_cast<uint16_t>(cp.m_ExtGID));
      }
#endif
    }
    SkPoint delta;
    if (MatrixOffset(&matrix, &delta)) {
      for (int index = 0; index < nChars; ++index) {
        m_charDetails.OffsetPositionAt(index + count, delta.fX * flip,
                                       -delta.fY * flip);
      }
    }
    if (hasRSX) {
      const SkTDArray<SkPoint>& positions = m_charDetails.GetPositions();
      for (int index = 0; index < nChars; ++index) {
        const TextCharPos& cp = pCharPos[index];
        SkRSXform* rsxform = &m_rsxform[index + count];
        if (cp.m_bGlyphAdjust) {
          rsxform->fSCos = cp.m_AdjustMatrix[0];
          rsxform->fSSin = cp.m_AdjustMatrix[1];
          rsxform->fTx = cp.m_AdjustMatrix[0] * positions[index].fX;
          rsxform->fTy = cp.m_AdjustMatrix[1] * positions[index].fY;
        } else {
          rsxform->fSCos = 1;
          rsxform->fSSin = 0;
          rsxform->fTx = positions[index].fX;
          rsxform->fTy = positions[index].fY;
        }
      }
    }
    return true;
  }

  void FlushText() {
    Dump(__func__);
    SkPaint skPaint;
    skPaint.setAntiAlias(true);
    skPaint.setColor(m_fillColor);

    SkFont font;
    if (m_pTypeFace) {  // exclude placeholder test fonts
      font.setTypeface(m_pTypeFace);
    }
    font.setEmbolden(m_isSubstFontBold);
    font.setHinting(SkFontHinting::kNone);
    font.setScaleX(m_scaleX);
    font.setSkewX(tanf(m_italicAngle * FX_PI / 180.0));
    font.setSize(SkTAbs(m_fontSize));
    font.setSubpixel(true);

    SkCanvas* skCanvas = m_pDriver->SkiaCanvas();
    SkAutoCanvasRestore scoped_save_restore(skCanvas, /*doSave=*/true);
    SkScalar flip = m_fontSize < 0 ? -1 : 1;
    SkMatrix skMatrix = ToFlippedSkMatrix(m_drawMatrix, flip);
    skCanvas->concat(skMatrix);
    const SkTDArray<uint16_t>& glyphs = m_charDetails.GetGlyphs();
#ifdef _SKIA_SUPPORT_PATHS_
    m_pDriver->PreMultiply();
#endif  // _SKIA_SUPPORT_PATHS_
#if SHOW_TEXT_GLYPHS
    SkTDArray<SkUnichar> text;
    // TODO(nigi): |m_glyphs| are deprecated and glyphToUnichars() takes 4
    // parameters now.
    text.setCount(m_glyphs.count());
    skPaint.glyphsToUnichars(m_glyphs.begin(), m_glyphs.count(), text.begin());
    for (int i = 0; i < m_glyphs.count(); ++i)
      printf("%lc", m_glyphs[i]);
    printf("\n");
#endif

    if (m_rsxform.count()) {
      sk_sp<SkTextBlob> blob = SkTextBlob::MakeFromRSXform(
          glyphs.begin(), glyphs.bytes(), m_rsxform.begin(), font,
          SkTextEncoding::kGlyphID);
      skCanvas->drawTextBlob(blob, 0, 0, skPaint);
    } else {
      const SkTDArray<SkPoint>& positions = m_charDetails.GetPositions();
      const SkTDArray<uint32_t>& widths = m_charDetails.GetFontCharWidths();
      for (int i = 0; i < m_charDetails.Count(); ++i) {
        uint32_t font_glyph_width =
            m_pFont ? m_pFont->GetGlyphWidth(glyphs[i]) : 0;
        uint32_t pdf_glyph_width = widths[i];
        if (font_glyph_width && pdf_glyph_width &&
            font_glyph_width > pdf_glyph_width) {
          font.setScaleX(SkIntToScalar(pdf_glyph_width) / font_glyph_width);
        } else {
          font.setScaleX(SkIntToScalar(1));
        }
        sk_sp<SkTextBlob> blob = SkTextBlob::MakeFromText(
            &glyphs[i], sizeof(glyphs[i]), font, SkTextEncoding::kGlyphID);
        skCanvas->drawTextBlob(blob, positions[i].fX, positions[i].fY, skPaint);
      }
    }

    m_drawIndex = INT_MAX;
    m_type = Accumulator::kNone;
    m_drawMatrix = CFX_Matrix();
    m_pFont = nullptr;
    m_italicAngle = 0;
    m_isSubstFontBold = false;
  }

  bool IsEmpty() const { return !m_commands.count(); }

  bool SetClipFill(const CFX_PathData* pPathData,
                   const CFX_Matrix* pMatrix,
                   int fill_mode) {
    if (m_debugDisable)
      return false;
    Dump(__func__);
    SkPath skClipPath;
    if (pPathData->GetPoints().size() == 5 ||
        pPathData->GetPoints().size() == 4) {
      Optional<CFX_FloatRect> maybe_rectf = pPathData->GetRect(pMatrix);
      if (maybe_rectf.has_value()) {
        CFX_FloatRect& rectf = maybe_rectf.value();
        rectf.Intersect(CFX_FloatRect(
            0, 0,
            static_cast<float>(m_pDriver->GetDeviceCaps(FXDC_PIXEL_WIDTH)),
            static_cast<float>(m_pDriver->GetDeviceCaps(FXDC_PIXEL_HEIGHT))));
        FX_RECT outer = rectf.GetOuterRect();
        // note that PDF's y-axis goes up; Skia's y-axis goes down
        skClipPath.addRect({(float)outer.left, (float)outer.bottom,
                            (float)outer.right, (float)outer.top});
      }
    }
    if (skClipPath.isEmpty()) {
      skClipPath = BuildPath(pPathData);
      skClipPath.setFillType(GetAlternateOrWindingFillType(fill_mode));
      SkMatrix skMatrix = ToSkMatrix(*pMatrix);
      skClipPath.transform(skMatrix);
    }
    return SetClip(skClipPath);
  }

  bool SetClip(const SkPath& skClipPath) {
    // if a pending draw depends on clip state that is cached, flush it and draw
    if (m_commandIndex < m_commands.count()) {
      if (m_commands[m_commandIndex] == Clip::kPath &&
          m_clips[m_commandIndex] == skClipPath) {
        ++m_commandIndex;
        return true;
      }
      Flush();
    }
    while (m_clipIndex > m_commandIndex) {
      do {
        --m_clipIndex;
        ASSERT(m_clipIndex >= 0);
      } while (m_commands[m_clipIndex] != Clip::kSave);
      m_pDriver->SkiaCanvas()->restore();
    }
    if (m_commandIndex < m_commands.count()) {
      m_commands[m_commandIndex] = Clip::kPath;
      m_clips[m_commandIndex] = skClipPath;
    } else {
      m_commands.push_back(Clip::kPath);
      m_clips.push_back(skClipPath);
    }
    ++m_commandIndex;
    return true;
  }

  bool SetClipStroke(const CFX_PathData* pPathData,
                     const CFX_Matrix* pMatrix,
                     const CFX_GraphStateData* pGraphState) {
    if (m_debugDisable)
      return false;
    Dump(__func__);
    SkPath skPath = BuildPath(pPathData);
    SkMatrix skMatrix = ToSkMatrix(*pMatrix);
    SkPaint skPaint;
    m_pDriver->PaintStroke(&skPaint, pGraphState, skMatrix);
    SkPath dst_path;
    skPaint.getFillPath(skPath, &dst_path);
    dst_path.transform(skMatrix);
    return SetClip(dst_path);
  }

  bool MatrixOffset(const CFX_Matrix* pMatrix, SkPoint* delta) {
    CFX_Matrix identityMatrix;
    if (!pMatrix)
      pMatrix = &identityMatrix;
    delta->set(pMatrix->e - m_drawMatrix.e, pMatrix->f - m_drawMatrix.f);
    if (!delta->fX && !delta->fY)
      return true;
    SkMatrix drawMatrix = ToSkMatrix(m_drawMatrix);
    if (!(drawMatrix.getType() & ~SkMatrix::kTranslate_Mask))
      return true;
    SkMatrix invDrawMatrix;
    if (!drawMatrix.invert(&invDrawMatrix))
      return false;
    SkMatrix invNewMatrix;
    SkMatrix newMatrix = ToSkMatrix(*pMatrix);
    if (!newMatrix.invert(&invNewMatrix))
      return false;
    delta->set(invDrawMatrix.getTranslateX() - invNewMatrix.getTranslateX(),
               invDrawMatrix.getTranslateY() - invNewMatrix.getTranslateY());
    return true;
  }

  // returns true if caller should apply command to skia canvas
  bool ClipSave() {
    if (m_debugDisable)
      return false;
    Dump(__func__);
    int count = m_commands.count();
    if (m_commandIndex < count) {
      if (Clip::kSave == m_commands[m_commandIndex]) {
        ++m_commandIndex;
        return true;
      }
      Flush();
      AdjustClip(m_commandIndex);
      m_commands[m_commandIndex] = Clip::kSave;
      m_clips[m_commandIndex] = m_skEmptyPath;
    } else {
      AdjustClip(m_commandIndex);
      m_commands.push_back(Clip::kSave);
      m_clips.push_back(m_skEmptyPath);
    }
    ++m_commandIndex;
    return true;
  }

  bool ClipRestore() {
    if (m_debugDisable)
      return false;
    Dump(__func__);

    for (int i = m_commandIndex - 1; i > 0; --i) {
      if (m_commands[i] == Clip::kSave) {
        m_commandIndex = i;
        break;
      }
    }
    return true;
  }

  bool DrawChanged(const CFX_Matrix* pMatrix,
                   const CFX_GraphStateData* pState,
                   uint32_t fill_color,
                   uint32_t stroke_color,
                   int fill_mode,
                   BlendMode blend_type,
                   bool group_knockout) const {
    return MatrixChanged(pMatrix) || StateChanged(pState, m_drawState) ||
           fill_color != m_fillColor || stroke_color != m_strokeColor ||
           IsEvenOddFillType(m_skPath.getFillType()) ||
           IsAlternateFillMode(fill_mode) || blend_type != m_blendType ||
           group_knockout != m_groupKnockout;
  }

  bool FontChanged(CFX_Font* pFont,
                   const CFX_Matrix& matrix,
                   float font_size,
                   float scaleX,
                   uint32_t color) const {
    CFX_TypeFace* typeface =
        pFont->GetFaceRec() ? pFont->GetDeviceCache() : nullptr;
    return typeface != m_pTypeFace.get() || MatrixChanged(&matrix) ||
           font_size != m_fontSize || scaleX != m_scaleX ||
           color != m_fillColor ||
           pFont->GetSubstFontItalicAngle() != m_italicAngle ||
           pFont->IsSubstFontBold() != m_isSubstFontBold;
  }

  bool MatrixChanged(const CFX_Matrix* pMatrix) const {
    return pMatrix ? *pMatrix != m_drawMatrix : m_drawMatrix.IsIdentity();
  }

  bool StateChanged(const CFX_GraphStateData* pState,
                    const CFX_GraphStateData& refState) const {
    CFX_GraphStateData identityState;
    if (!pState)
      pState = &identityState;
    return pState->m_LineWidth != refState.m_LineWidth ||
           pState->m_LineCap != refState.m_LineCap ||
           pState->m_LineJoin != refState.m_LineJoin ||
           pState->m_MiterLimit != refState.m_MiterLimit ||
           DashChanged(pState, refState);
  }

  bool DashChanged(const CFX_GraphStateData* pState,
                   const CFX_GraphStateData& refState) const {
    bool dashArray = pState && !pState->m_DashArray.empty();
    if (!dashArray && refState.m_DashArray.empty())
      return false;
    if (!dashArray || refState.m_DashArray.empty())
      return true;
    return pState->m_DashPhase != refState.m_DashPhase ||
           pState->m_DashArray != refState.m_DashArray;
  }

  void AdjustClip(int limit) {
    while (m_clipIndex > limit) {
      do {
        --m_clipIndex;
        ASSERT(m_clipIndex >= 0);
      } while (m_commands[m_clipIndex] != Clip::kSave);
      m_pDriver->SkiaCanvas()->restore();
    }
    while (m_clipIndex < limit) {
      if (Clip::kSave == m_commands[m_clipIndex]) {
        m_pDriver->SkiaCanvas()->save();
      } else {
        ASSERT(Clip::kPath == m_commands[m_clipIndex]);
        m_pDriver->SkiaCanvas()->clipPath(m_clips[m_clipIndex],
                                          SkClipOp::kIntersect, true);
      }
      ++m_clipIndex;
    }
  }

  void Flush() {
    if (m_debugDisable)
      return;
    Dump(__func__);
    if (Accumulator::kPath == m_type || Accumulator::kText == m_type) {
      AdjustClip(std::min(m_drawIndex, m_commands.count()));
      Accumulator::kPath == m_type ? FlushPath() : FlushText();
    }
  }

  void FlushForDraw() {
    if (m_debugDisable)
      return;
    Flush();                     // draw any pending text or path
    AdjustClip(m_commandIndex);  // set up clip stack with any pending state
  }

#if SHOW_SKIA_PATH
  void DumpPrefix(int index) const {
    if (index != m_commandIndex && index != m_drawIndex &&
        index != m_clipIndex) {
      printf("     ");
      return;
    }
    printf("%c%c%c> ", index == m_commandIndex ? 'x' : '-',
           index == m_drawIndex ? 'd' : '-', index == m_clipIndex ? 'c' : '-');
  }

  void DumpEndPrefix() const {
    int index = m_commands.count();
    if (index != m_commandIndex && index > m_drawIndex && index != m_clipIndex)
      return;
    printf("%c%c%c>\n", index == m_commandIndex ? 'x' : '-',
           index <= m_drawIndex ? 'd' : '-', index == m_clipIndex ? 'c' : '-');
  }
#endif  // SHOW_SKIA_PATH

  void Dump(const char* where) const {
#if SHOW_SKIA_PATH
    if (m_debugDisable)
      return;
    printf(
        "\n%s\nSkia Save Count %d  Agg Save Stack/Count %d/%d"
        "  Cache Save Index/Count %d/%d\n",
        where, m_pDriver->SkiaCanvas()->getSaveCount(),
        (int)m_pDriver->stack().size(), AggSaveCount(m_pDriver), m_commandIndex,
        CacheSaveCount(m_commands, m_commandIndex));
    printf("Cache:\n");
#if SHOW_SKIA_PATH_SHORTHAND
    bool dumpedPath = false;
#endif
    for (int index = 0; index < m_commands.count(); ++index) {
#if SHOW_SKIA_PATH_SHORTHAND
      if (Clip::kSave == m_commands[index] && dumpedPath) {
        printf("\n");
        dumpedPath = false;
      }
#endif
      DumpPrefix(index);
      switch (m_commands[index]) {
        case Clip::kSave:
          printf("Save %d\n", ++m_debugSaveCounter);
          break;
        case Clip::kPath:
#if SHOW_SKIA_PATH_SHORTHAND
          printf("*");
          dumpedPath = true;
#else
          m_clips[index].dump();
#endif
          break;
        default:
          printf("unknown\n");
      }
    }
#if SHOW_SKIA_PATH_SHORTHAND
    if (dumpedPath)
      printf("\n");
#endif
    DumpEndPrefix();
    int skCanvasSaveCount = m_pDriver->SkiaCanvas()->getSaveCount();
    int cacheSaveCount = 1;
    ASSERT(m_clipIndex <= m_commands.count());
    for (int index = 0; index < m_clipIndex; ++index)
      cacheSaveCount += Clip::kSave == m_commands[index];
    ASSERT(skCanvasSaveCount == cacheSaveCount);
#endif  // SHOW_SKIA_PATH
  }

#if SHOW_SKIA_PATH
  static int AggSaveCount(const UnownedPtr<CFX_SkiaDeviceDriver> driver) {
    FX_RECT last;
    int aggSaveCount = 0;
    bool foundLast = false;
    for (int index = 0; index < (int)driver->stack().size(); ++index) {
      if (!driver->stack()[index]) {
        continue;
      }
      if (driver->stack()[index]->GetType() != CFX_ClipRgn::RectI) {
        aggSaveCount += 1;
        foundLast = false;
        continue;
      }
      if (!foundLast ||
          memcmp(&last, &driver->stack()[index]->GetBox(), sizeof(FX_RECT))) {
        aggSaveCount += 1;
        foundLast = true;
        last = driver->stack()[index]->GetBox();
      }
    }
    if (driver->clip_region()) {
      CFX_ClipRgn::ClipType clipType = driver->clip_region()->GetType();
      if (clipType != CFX_ClipRgn::RectI || !foundLast ||
          memcmp(&last, &driver->clip_region()->GetBox(), sizeof(FX_RECT))) {
        aggSaveCount += 1;
      }
    }
    return aggSaveCount;
  }

  static int CacheSaveCount(const SkTDArray<SkiaState::Clip>& commands,
                            int commandIndex) {
    int cacheSaveCount = 0;
    bool newPath = false;
    for (int index = 0; index < commandIndex; ++index) {
      if (Clip::kSave == commands[index]) {
        newPath = true;
      } else if (newPath) {
        ++cacheSaveCount;
        newPath = false;
      }
    }
    return cacheSaveCount;
  }
#endif

  void DebugCheckClip() {
#if SHOW_SKIA_PATH
    if (m_debugDisable)
      return;
    int aggSaveCount = AggSaveCount(m_pDriver);
    int cacheSaveCount = CacheSaveCount(m_commands, m_commandIndex);
    ASSERT(m_clipIndex <= m_commands.count());
    if (aggSaveCount != cacheSaveCount) {
      // may not signify a bug if counts don't match
      printf("aggSaveCount %d != cacheSaveCount %d\n", aggSaveCount,
             cacheSaveCount);
      DumpClipStacks();
    }
    for (int aggIndex = 0; aggIndex < (int)m_pDriver->stack().size();
         ++aggIndex) {
      if (!m_pDriver->stack()[aggIndex])
        continue;
      if (m_pDriver->stack()[aggIndex]->GetType() != CFX_ClipRgn::RectI)
        continue;
      const FX_RECT& aggRect = m_pDriver->stack()[aggIndex]->GetBox();
      SkRect skRect = SkRect::MakeLTRB(aggRect.left, aggRect.top, aggRect.right,
                                       aggRect.bottom);
      bool foundMatch = false;
      for (int skIndex = 0; skIndex < m_commandIndex; ++skIndex) {
        if (Clip::kPath != m_commands[skIndex])
          continue;
        const SkPath& clip = m_clips[skIndex];
        SkRect bounds;
        if (!clip.isRect(&bounds))
          continue;
        bounds.roundOut(&bounds);
        if (skRect == bounds) {
          foundMatch = true;
          break;
        }
      }
      if (!foundMatch) {
        DumpClipStacks();
        NOTREACHED();
      }
    }
#endif  // SHOW_SKIA_PATH
  }

#if SHOW_SKIA_PATH
  void DumpClipStacks() const {
    if (m_debugDisable)
      return;
    printf("\ncache\n");
    for (int index = 0; index < m_commandIndex; ++index) {
      DumpPrefix(index);
      switch (m_commands[index]) {
        case Clip::kSave:
          printf("Save\n");
          break;
        case Clip::kPath:
          m_clips[index].dump();
          break;
        default:
          printf("unknown\n");
      }
    }
    printf("\nagg\n");
    for (int index = 0; index < (int)m_pDriver->stack().size(); ++index) {
      if (!m_pDriver->stack()[index]) {
        printf("null\n");
        continue;
      }
      CFX_ClipRgn::ClipType clipType = m_pDriver->stack()[index]->GetType();
      const FX_RECT& box = m_pDriver->stack()[index]->GetBox();
      printf("stack rect: %d,%d,%d,%d mask=%s\n", box.left, box.top, box.right,
             box.bottom,
             CFX_ClipRgn::MaskF == clipType
                 ? "1"
                 : CFX_ClipRgn::RectI == clipType ? "0" : "?");
    }
    if (m_pDriver->clip_region()) {
      const FX_RECT& box = m_pDriver->clip_region()->GetBox();
      CFX_ClipRgn::ClipType clipType = m_pDriver->clip_region()->GetType();
      printf("clip rect: %d,%d,%d,%d mask=%s\n", box.left, box.top, box.right,
             box.bottom,
             CFX_ClipRgn::MaskF == clipType
                 ? "1"
                 : CFX_ClipRgn::RectI == clipType ? "0" : "?");
    }
  }
#endif  // SHOW_SKIA_PATH

 private:
  class CharDetail {
   public:
    CharDetail() = default;
    ~CharDetail() = default;

    const SkTDArray<SkPoint>& GetPositions() const { return m_positions; }
    void SetPositionAt(int index, const SkPoint& position) {
      m_positions[index] = position;
    }
    void OffsetPositionAt(int index, SkScalar dx, SkScalar dy) {
      m_positions[index].offset(dx, dy);
    }
    const SkTDArray<uint16_t>& GetGlyphs() const { return m_glyphs; }
    void SetGlyphAt(int index, uint16_t glyph) { m_glyphs[index] = glyph; }
    const SkTDArray<uint32_t>& GetFontCharWidths() const {
      return m_fontCharWidths;
    }
    void SetFontCharWidthAt(int index, uint32_t width) {
      m_fontCharWidths[index] = width;
    }
    int Count() const {
      ASSERT(m_positions.count() == m_glyphs.count());
      return m_glyphs.count();
    }
    void SetCount(int count) {
      ASSERT(count >= 0);
      m_positions.setCount(count);
      m_glyphs.setCount(count);
      m_fontCharWidths.setCount(count);
    }

   private:
    SkTDArray<SkPoint> m_positions;  // accumulator for text positions
    SkTDArray<uint16_t> m_glyphs;    // accumulator for text glyphs
    // accumulator for glyphs' width defined in pdf
    SkTDArray<uint32_t> m_fontCharWidths;
  };

  SkTArray<SkPath> m_clips;        // stack of clips that may be reused
  SkTDArray<Clip> m_commands;      // stack of clip-related commands
  CharDetail m_charDetails;
  SkTDArray<SkRSXform> m_rsxform;  // accumulator for txt rotate/scale/translate
  SkPath m_skPath;                 // accumulator for path contours
  SkPath m_skEmptyPath;            // used as placehold in the clips array
  UnownedPtr<CFX_Font> m_pFont;
  CFX_Matrix m_drawMatrix;
  CFX_GraphStateData m_clipState;
  CFX_GraphStateData m_drawState;
  CFX_Matrix m_clipMatrix;
  UnownedPtr<CFX_SkiaDeviceDriver> const m_pDriver;
  sk_sp<CFX_TypeFace> m_pTypeFace;
  float m_fontSize = 0;
  float m_scaleX = 0;
  uint32_t m_fillColor = 0;
  uint32_t m_strokeColor = 0;
  BlendMode m_blendType = BlendMode::kNormal;
  int m_commandIndex = 0;     // active position in clip command stack
  int m_drawIndex = INT_MAX;  // position of the pending path or text draw
  int m_clipIndex = 0;        // position reflecting depth of canvas clip stacck
  int m_italicAngle = 0;
  Accumulator m_type = Accumulator::kNone;  // type of pending draw
  bool m_fillFullCover = false;
  bool m_fillPath = false;
  bool m_groupKnockout = false;
  bool m_isSubstFontBold = false;
  bool m_debugDisable = false;  // turn off cache for debugging
#if SHOW_SKIA_PATH
 public:
  mutable int m_debugSaveCounter = 0;
  static int m_debugInitCounter;
#endif
};

#if SHOW_SKIA_PATH
int SkiaState::m_debugInitCounter;
#endif

// convert a stroking path to scanlines
void CFX_SkiaDeviceDriver::PaintStroke(SkPaint* spaint,
                                       const CFX_GraphStateData* pGraphState,
                                       const SkMatrix& matrix) {
  SkPaint::Cap cap;
  switch (pGraphState->m_LineCap) {
    case CFX_GraphStateData::LineCapRound:
      cap = SkPaint::kRound_Cap;
      break;
    case CFX_GraphStateData::LineCapSquare:
      cap = SkPaint::kSquare_Cap;
      break;
    default:
      cap = SkPaint::kButt_Cap;
      break;
  }
  SkPaint::Join join;
  switch (pGraphState->m_LineJoin) {
    case CFX_GraphStateData::LineJoinRound:
      join = SkPaint::kRound_Join;
      break;
    case CFX_GraphStateData::LineJoinBevel:
      join = SkPaint::kBevel_Join;
      break;
    default:
      join = SkPaint::kMiter_Join;
      break;
  }
  SkMatrix inverse;
  if (!matrix.invert(&inverse))
    return;  // give up if the matrix is degenerate, and not invertable
  inverse.set(SkMatrix::kMTransX, 0);
  inverse.set(SkMatrix::kMTransY, 0);
  SkVector deviceUnits[2] = {{0, 1}, {1, 0}};
  inverse.mapPoints(deviceUnits, SK_ARRAY_COUNT(deviceUnits));
  float width =
      std::max(pGraphState->m_LineWidth,
               std::min(deviceUnits[0].length(), deviceUnits[1].length()));
  if (!pGraphState->m_DashArray.empty()) {
    size_t count = (pGraphState->m_DashArray.size() + 1) / 2;
    std::unique_ptr<SkScalar, FxFreeDeleter> intervals(
        FX_Alloc2D(SkScalar, count, sizeof(SkScalar)));
    // Set dash pattern
    for (size_t i = 0; i < count; i++) {
      float on = pGraphState->m_DashArray[i * 2];
      if (on <= 0.000001f)
        on = 1.f / 10;
      float off = i * 2 + 1 == pGraphState->m_DashArray.size()
                      ? on
                      : pGraphState->m_DashArray[i * 2 + 1];
      if (off < 0)
        off = 0;
      intervals.get()[i * 2] = on;
      intervals.get()[i * 2 + 1] = off;
    }
    spaint->setPathEffect(SkDashPathEffect::Make(intervals.get(), count * 2,
                                                 pGraphState->m_DashPhase));
  }
  spaint->setStyle(SkPaint::kStroke_Style);
  spaint->setAntiAlias(true);
  spaint->setStrokeWidth(width);
  spaint->setStrokeMiter(pGraphState->m_MiterLimit);
  spaint->setStrokeCap(cap);
  spaint->setStrokeJoin(join);
}

CFX_SkiaDeviceDriver::CFX_SkiaDeviceDriver(
    const RetainPtr<CFX_DIBitmap>& pBitmap,
    bool bRgbByteOrder,
    const RetainPtr<CFX_DIBitmap>& pBackdropBitmap,
    bool bGroupKnockout)
    : m_pBitmap(pBitmap),
      m_pBackdropBitmap(pBackdropBitmap),
      m_pRecorder(nullptr),
      m_pCache(new SkiaState(this)),
#ifdef _SKIA_SUPPORT_PATHS_
      m_pClipRgn(nullptr),
      m_FillFlags(0),
#endif  // _SKIA_SUPPORT_PATHS_
      m_bRgbByteOrder(bRgbByteOrder),
      m_bGroupKnockout(bGroupKnockout) {
  SkBitmap skBitmap;
  SkColorType color_type;
  const int bpp = pBitmap->GetBPP();
  if (bpp == 8) {
    color_type = GetIsAlphaFromFormat(pBitmap->GetFormat())
                     ? kAlpha_8_SkColorType
                     : kGray_8_SkColorType;
  } else {
    ASSERT(bpp == 32);
    color_type = Get32BitSkColorType(bRgbByteOrder);
  }

  SkImageInfo imageInfo =
      SkImageInfo::Make(pBitmap->GetWidth(), pBitmap->GetHeight(), color_type,
                        kOpaque_SkAlphaType);
  skBitmap.installPixels(imageInfo, pBitmap->GetBuffer(), pBitmap->GetPitch());
  m_pCanvas = new SkCanvas(skBitmap);
}

#ifdef _SKIA_SUPPORT_
CFX_SkiaDeviceDriver::CFX_SkiaDeviceDriver(int size_x, int size_y)
    : m_pBitmap(nullptr),
      m_pBackdropBitmap(nullptr),
      m_pRecorder(new SkPictureRecorder),
      m_pCache(new SkiaState(this)),
      m_bGroupKnockout(false) {
  m_pRecorder->beginRecording(SkIntToScalar(size_x), SkIntToScalar(size_y));
  m_pCanvas = m_pRecorder->getRecordingCanvas();
}

CFX_SkiaDeviceDriver::CFX_SkiaDeviceDriver(SkPictureRecorder* recorder)
    : m_pBitmap(nullptr),
      m_pBackdropBitmap(nullptr),
      m_pRecorder(recorder),
      m_pCache(new SkiaState(this)),
      m_bGroupKnockout(false) {
  m_pCanvas = m_pRecorder->getRecordingCanvas();
}
#endif  // _SKIA_SUPPORT_

CFX_SkiaDeviceDriver::~CFX_SkiaDeviceDriver() {
  Flush();
  if (!m_pRecorder)
    delete m_pCanvas;
}

void CFX_SkiaDeviceDriver::Flush() {
  m_pCache->Flush();
}

void CFX_SkiaDeviceDriver::PreMultiply() {
  m_pBitmap->PreMultiply();
}

bool CFX_SkiaDeviceDriver::DrawDeviceText(int nChars,
                                          const TextCharPos* pCharPos,
                                          CFX_Font* pFont,
                                          const CFX_Matrix& mtObject2Device,
                                          float font_size,
                                          uint32_t color) {
  if (m_pCache->DrawText(nChars, pCharPos, pFont, mtObject2Device, font_size,
                         color)) {
    return true;
  }
  sk_sp<SkTypeface> typeface(SkSafeRef(pFont->GetDeviceCache()));
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor(color);

  SkFont font;
  font.setTypeface(typeface);
  font.setEmbolden(pFont->IsSubstFontBold());
  font.setHinting(SkFontHinting::kNone);
  font.setSize(SkTAbs(font_size));
  font.setSubpixel(true);
  font.setSkewX(tanf(pFont->GetSubstFontItalicAngle() * FX_PI / 180.0));

  SkAutoCanvasRestore scoped_save_restore(m_pCanvas, /*doSave=*/true);
  SkScalar flip = font_size < 0 ? -1 : 1;
  SkScalar vFlip = flip;
  if (pFont->IsVertical())
    vFlip *= -1;
  SkMatrix skMatrix = ToFlippedSkMatrix(mtObject2Device, flip);
  m_pCanvas->concat(skMatrix);
  SkTDArray<SkPoint> positions;
  positions.setCount(nChars);
  SkTDArray<uint16_t> glyphs;
  glyphs.setCount(nChars);
  bool useRSXform = false;
  bool oneAtATime = false;
  for (int index = 0; index < nChars; ++index) {
    const TextCharPos& cp = pCharPos[index];
    positions[index] = {cp.m_Origin.x * flip, cp.m_Origin.y * vFlip};
    if (cp.m_bGlyphAdjust) {
      useRSXform = true;
      if (cp.m_AdjustMatrix[0] != cp.m_AdjustMatrix[3] ||
          cp.m_AdjustMatrix[1] != -cp.m_AdjustMatrix[2]) {
        oneAtATime = true;
      }
    }
    glyphs[index] = static_cast<uint16_t>(cp.m_GlyphIndex);
#if defined(OS_MACOSX)
    if (cp.m_ExtGID)
      glyphs[index] = static_cast<uint16_t>(cp.m_ExtGID);
#endif
  }
  if (oneAtATime)
    useRSXform = false;
#if SHOW_TEXT_GLYPHS
  SkTDArray<SkUnichar> text;
  text.setCount(glyphs.count());
  paint.glyphsToUnichars(glyphs.begin(), glyphs.count(), text.begin());
  for (int i = 0; i < glyphs.count(); ++i)
    printf("%lc", text[i]);
  printf("\n");
#endif
#ifdef _SKIA_SUPPORT_PATHS_
  m_pBitmap->PreMultiply();
#endif  // _SKIA_SUPPORT_PATHS_
  if (useRSXform) {
    SkTDArray<SkRSXform> xforms;
    xforms.setCount(nChars);
    for (int index = 0; index < nChars; ++index) {
      const TextCharPos& cp = pCharPos[index];
      SkRSXform* rsxform = &xforms[index];
      if (cp.m_bGlyphAdjust) {
        rsxform->fSCos = cp.m_AdjustMatrix[0];
        rsxform->fSSin = cp.m_AdjustMatrix[1];
        rsxform->fTx = cp.m_AdjustMatrix[0] * positions[index].fX;
        rsxform->fTy = cp.m_AdjustMatrix[1] * positions[index].fY;
      } else {
        rsxform->fSCos = 1;
        rsxform->fSSin = 0;
        rsxform->fTx = positions[index].fX;
        rsxform->fTy = positions[index].fY;
      }
    }
    m_pCanvas->drawTextBlob(
        SkTextBlob::MakeFromRSXform(glyphs.begin(), nChars * 2, xforms.begin(),
                                    font, SkTextEncoding::kGlyphID),
        0, 0, paint);
  } else if (oneAtATime) {
    for (int index = 0; index < nChars; ++index) {
      const TextCharPos& cp = pCharPos[index];
      if (cp.m_bGlyphAdjust) {
        if (0 == cp.m_AdjustMatrix[1] && 0 == cp.m_AdjustMatrix[2] &&
            1 == cp.m_AdjustMatrix[3]) {
          font.setScaleX(cp.m_AdjustMatrix[0]);
          auto blob =
              SkTextBlob::MakeFromText(&glyphs[index], sizeof(glyphs[index]),
                                       font, SkTextEncoding::kGlyphID);
          m_pCanvas->drawTextBlob(blob, positions[index].fX,
                                  positions[index].fY, paint);
          font.setScaleX(SkIntToScalar(1));
        } else {
          SkAutoCanvasRestore scoped_save_restore2(m_pCanvas, /*doSave=*/true);
          SkMatrix adjust;
          adjust.reset();
          adjust.setScaleX(cp.m_AdjustMatrix[0]);
          adjust.setSkewX(cp.m_AdjustMatrix[1]);
          adjust.setSkewY(cp.m_AdjustMatrix[2]);
          adjust.setScaleY(cp.m_AdjustMatrix[3]);
          adjust.preTranslate(positions[index].fX, positions[index].fY);
          m_pCanvas->concat(adjust);
          auto blob =
              SkTextBlob::MakeFromText(&glyphs[index], sizeof(glyphs[index]),
                                       font, SkTextEncoding::kGlyphID);
          m_pCanvas->drawTextBlob(blob, 0, 0, paint);
        }
      } else {
        auto blob =
            SkTextBlob::MakeFromText(&glyphs[index], sizeof(glyphs[index]),
                                     font, SkTextEncoding::kGlyphID);
        m_pCanvas->drawTextBlob(blob, positions[index].fX, positions[index].fY,
                                paint);
      }
    }
  } else {
    for (int index = 0; index < nChars; ++index) {
      const TextCharPos& cp = pCharPos[index];
      uint32_t font_glyph_width =
          pFont ? pFont->GetGlyphWidth(cp.m_GlyphIndex) : 0;
      uint32_t pdf_glyph_width = cp.m_FontCharWidth;
      if (font_glyph_width && pdf_glyph_width &&
          font_glyph_width > pdf_glyph_width) {
        font.setScaleX(SkIntToScalar(pdf_glyph_width) / font_glyph_width);
      } else {
        font.setScaleX(SkIntToScalar(1));
      }
      auto blob =
          SkTextBlob::MakeFromText(&glyphs[index], sizeof(glyphs[index]), font,
                                   SkTextEncoding::kGlyphID);
      m_pCanvas->drawTextBlob(blob, positions[index].fX, positions[index].fY,
                              paint);
    }
  }

  return true;
}

int CFX_SkiaDeviceDriver::GetDriverType() const {
  return 1;
}

DeviceType CFX_SkiaDeviceDriver::GetDeviceType() const {
  return DeviceType::kDisplay;
}

int CFX_SkiaDeviceDriver::GetDeviceCaps(int caps_id) const {
  switch (caps_id) {
#ifdef _SKIA_SUPPORT_
    case FXDC_PIXEL_WIDTH:
      return m_pCanvas->imageInfo().width();
    case FXDC_PIXEL_HEIGHT:
      return m_pCanvas->imageInfo().height();
    case FXDC_BITS_PIXEL:
      return 32;
    case FXDC_HORZ_SIZE:
    case FXDC_VERT_SIZE:
      return 0;
    case FXDC_RENDER_CAPS:
      return FXRC_GET_BITS | FXRC_ALPHA_PATH | FXRC_ALPHA_IMAGE |
             FXRC_BLEND_MODE | FXRC_SOFT_CLIP | FXRC_ALPHA_OUTPUT |
             FXRC_FILLSTROKE_PATH | FXRC_SHADING;
#endif  // _SKIA_SUPPORT_

#ifdef _SKIA_SUPPORT_PATHS_
    case FXDC_PIXEL_WIDTH:
      return m_pBitmap->GetWidth();
    case FXDC_PIXEL_HEIGHT:
      return m_pBitmap->GetHeight();
    case FXDC_BITS_PIXEL:
      return m_pBitmap->GetBPP();
    case FXDC_HORZ_SIZE:
    case FXDC_VERT_SIZE:
      return 0;
    case FXDC_RENDER_CAPS: {
      int flags = FXRC_GET_BITS | FXRC_ALPHA_PATH | FXRC_ALPHA_IMAGE |
                  FXRC_BLEND_MODE | FXRC_SOFT_CLIP | FXRC_SHADING;
      if (m_pBitmap->HasAlpha()) {
        flags |= FXRC_ALPHA_OUTPUT;
      } else if (m_pBitmap->IsAlphaMask()) {
        if (m_pBitmap->GetBPP() == 1) {
          flags |= FXRC_BITMASK_OUTPUT;
        } else {
          flags |= FXRC_BYTEMASK_OUTPUT;
        }
      }
      if (m_pBitmap->IsCmykImage()) {
        flags |= FXRC_CMYK_OUTPUT;
      }
      return flags;
    }
#endif  // _SKIA_SUPPORT_PATHS_

    default:
      NOTREACHED();
      return 0;
  }
}

void CFX_SkiaDeviceDriver::SaveState() {
  m_pCache->DebugCheckClip();
  if (!m_pCache->ClipSave())
    m_pCanvas->save();

#ifdef _SKIA_SUPPORT_PATHS_
#if SHOW_SKIA_PATH
  printf("SaveState %zd\n", stack().size());
#endif
  std::unique_ptr<CFX_ClipRgn> pClip;
  if (m_pClipRgn)
    pClip = pdfium::MakeUnique<CFX_ClipRgn>(*m_pClipRgn);
  m_StateStack.push_back(std::move(pClip));
#endif  // _SKIA_SUPPORT_PATHS_
}

void CFX_SkiaDeviceDriver::RestoreState(bool bKeepSaved) {
#ifdef _SKIA_SUPPORT_PATHS_
  m_pClipRgn.reset();

  if (m_StateStack.empty())
    return;
#else
  if (m_pCache->IsEmpty())
    return;
#endif
  if (!m_pCache->ClipRestore())
    m_pCanvas->restore();
  if (bKeepSaved && !m_pCache->ClipSave())
    m_pCanvas->save();
#ifdef _SKIA_SUPPORT_PATHS_
#if SHOW_SKIA_PATH
  printf("RestoreState %zd %s\n", m_StateStack.size(),
         bKeepSaved ? "bKeepSaved" : "");
#endif
  if (bKeepSaved) {
    if (m_StateStack.back())
      m_pClipRgn = pdfium::MakeUnique<CFX_ClipRgn>(*m_StateStack.back());
  } else {
    m_pClipRgn = std::move(m_StateStack.back());
    m_StateStack.pop_back();
  }
  m_pCache->DebugCheckClip();
#endif  // _SKIA_SUPPORT_PATHS_
}

#ifdef _SKIA_SUPPORT_PATHS_
void CFX_SkiaDeviceDriver::SetClipMask(const FX_RECT& clipBox,
                                       const SkPath& path) {
  FX_RECT path_rect(clipBox.left, clipBox.top, clipBox.right + 1,
                    clipBox.bottom + 1);
  path_rect.Intersect(m_pClipRgn->GetBox());
  auto pThisLayer = pdfium::MakeRetain<CFX_DIBitmap>();
  pThisLayer->Create(path_rect.Width(), path_rect.Height(), FXDIB_8bppMask);
  pThisLayer->Clear(0);

  SkImageInfo imageInfo =
      SkImageInfo::Make(pThisLayer->GetWidth(), pThisLayer->GetHeight(),
                        SkColorType::kAlpha_8_SkColorType, kOpaque_SkAlphaType);
  SkBitmap bitmap;
  bitmap.installPixels(imageInfo, pThisLayer->GetBuffer(),
                       pThisLayer->GetPitch());
  auto canvas = pdfium::MakeUnique<SkCanvas>(bitmap);
  canvas->translate(
      -path_rect.left,
      -path_rect.top);  // FIXME(caryclark) wrong sign(s)? upside down?
  SkPaint paint;
  paint.setAntiAlias((m_FillFlags & FXFILL_NOPATHSMOOTH) == 0);
  canvas->drawPath(path, paint);
  m_pClipRgn->IntersectMaskF(path_rect.left, path_rect.top, pThisLayer);
}
#endif  // _SKIA_SUPPORT_PATHS_

bool CFX_SkiaDeviceDriver::SetClip_PathFill(
    const CFX_PathData* pPathData,     // path info
    const CFX_Matrix* pObject2Device,  // flips object's y-axis
    int fill_mode                      // fill mode, WINDING or ALTERNATE
    ) {
  CFX_Matrix identity;
  const CFX_Matrix* deviceMatrix = pObject2Device ? pObject2Device : &identity;
  bool cached = m_pCache->SetClipFill(pPathData, deviceMatrix, fill_mode);

#ifdef _SKIA_SUPPORT_PATHS_
  m_FillFlags = fill_mode;
  if (!m_pClipRgn) {
    m_pClipRgn = pdfium::MakeUnique<CFX_ClipRgn>(
        GetDeviceCaps(FXDC_PIXEL_WIDTH), GetDeviceCaps(FXDC_PIXEL_HEIGHT));
  }
#endif  // _SKIA_SUPPORT_PATHS_
  if (pPathData->GetPoints().size() == 5 ||
      pPathData->GetPoints().size() == 4) {
    Optional<CFX_FloatRect> maybe_rectf = pPathData->GetRect(deviceMatrix);
    if (maybe_rectf.has_value()) {
      CFX_FloatRect& rectf = maybe_rectf.value();
      rectf.Intersect(CFX_FloatRect(0, 0,
                                    (float)GetDeviceCaps(FXDC_PIXEL_WIDTH),
                                    (float)GetDeviceCaps(FXDC_PIXEL_HEIGHT)));
      // note that PDF's y-axis goes up; Skia's y-axis goes down
      if (!cached) {
        SkRect skClipRect =
            SkRect::MakeLTRB(rectf.left, rectf.bottom, rectf.right, rectf.top);
        DebugDrawSkiaClipRect(m_pCanvas, skClipRect);
        m_pCanvas->clipRect(skClipRect, SkClipOp::kIntersect, true);
      }

#ifdef _SKIA_SUPPORT_PATHS_
      FX_RECT rect = rectf.GetOuterRect();
      m_pClipRgn->IntersectRect(rect);
#endif  // _SKIA_SUPPORT_PATHS_
      DebugShowCanvasClip(this, m_pCanvas);
      return true;
    }
  }
  SkPath skClipPath = BuildPath(pPathData);
  skClipPath.setFillType(GetAlternateOrWindingFillType(fill_mode));
  SkMatrix skMatrix = ToSkMatrix(*deviceMatrix);
  skClipPath.transform(skMatrix);
  DebugShowSkiaPath(skClipPath);
  if (!cached) {
    DebugDrawSkiaClipPath(m_pCanvas, skClipPath);
    m_pCanvas->clipPath(skClipPath, SkClipOp::kIntersect, true);
  }
#ifdef _SKIA_SUPPORT_PATHS_
  FX_RECT clipBox(0, 0, GetDeviceCaps(FXDC_PIXEL_WIDTH),
                  GetDeviceCaps(FXDC_PIXEL_HEIGHT));
  SetClipMask(clipBox, skClipPath);
#endif  // _SKIA_SUPPORT_PATHS_
  DebugShowCanvasClip(this, m_pCanvas);
  return true;
}

bool CFX_SkiaDeviceDriver::SetClip_PathStroke(
    const CFX_PathData* pPathData,         // path info
    const CFX_Matrix* pObject2Device,      // required transformation
    const CFX_GraphStateData* pGraphState  // graphic state, for pen attributes
    ) {
  bool cached = m_pCache->SetClipStroke(pPathData, pObject2Device, pGraphState);

#ifdef _SKIA_SUPPORT_PATHS_
  if (!m_pClipRgn) {
    m_pClipRgn = pdfium::MakeUnique<CFX_ClipRgn>(
        GetDeviceCaps(FXDC_PIXEL_WIDTH), GetDeviceCaps(FXDC_PIXEL_HEIGHT));
  }
#endif  // _SKIA_SUPPORT_PATHS_
  // build path data
  SkPath skPath = BuildPath(pPathData);
  SkMatrix skMatrix = ToSkMatrix(*pObject2Device);
  SkPaint skPaint;
  PaintStroke(&skPaint, pGraphState, skMatrix);
  SkPath dst_path;
  skPaint.getFillPath(skPath, &dst_path);
  dst_path.transform(skMatrix);
  if (!cached) {
    DebugDrawSkiaClipPath(m_pCanvas, dst_path);
    m_pCanvas->clipPath(dst_path, SkClipOp::kIntersect, true);
  }
#ifdef _SKIA_SUPPORT_PATHS_
  FX_RECT clipBox(0, 0, GetDeviceCaps(FXDC_PIXEL_WIDTH),
                  GetDeviceCaps(FXDC_PIXEL_HEIGHT));
  SetClipMask(clipBox, dst_path);
#endif  // _SKIA_SUPPORT_PATHS_
  DebugShowCanvasClip(this, m_pCanvas);
  return true;
}

bool CFX_SkiaDeviceDriver::DrawPath(
    const CFX_PathData* pPathData,          // path info
    const CFX_Matrix* pObject2Device,       // optional transformation
    const CFX_GraphStateData* pGraphState,  // graphic state, for pen attributes
    uint32_t fill_color,                    // fill color
    uint32_t stroke_color,                  // stroke color
    int fill_mode,  // fill mode, WINDING or ALTERNATE. 0 for not filled
    BlendMode blend_type) {
  ASSERT(GetAlternateOrWindingFillMode(fill_mode) !=
         kAlternateOrWindingFillModeMask);
  if (m_pCache->DrawPath(pPathData, pObject2Device, pGraphState, fill_color,
                         stroke_color, fill_mode, blend_type)) {
    return true;
  }
  SkMatrix skMatrix;
  if (pObject2Device)
    skMatrix = ToSkMatrix(*pObject2Device);
  else
    skMatrix.setIdentity();
  SkPaint skPaint;
  skPaint.setAntiAlias(true);
  if (fill_mode & FXFILL_FULLCOVER)
    skPaint.setBlendMode(SkBlendMode::kPlus);
  int stroke_alpha = FXARGB_A(stroke_color);
  bool is_paint_stroke = pGraphState && stroke_alpha;
  if (is_paint_stroke)
    PaintStroke(&skPaint, pGraphState, skMatrix);
  SkPath skPath = BuildPath(pPathData);
  SkAutoCanvasRestore scoped_save_restore(m_pCanvas, /*doSave=*/true);
  m_pCanvas->concat(skMatrix);
  bool do_stroke = true;
  if (GetAlternateOrWindingFillMode(fill_mode) && fill_color) {
    skPath.setFillType(GetAlternateOrWindingFillType(fill_mode));
    SkPath strokePath;
    const SkPath* fillPath = &skPath;
    if (is_paint_stroke) {
      if (m_bGroupKnockout) {
        skPaint.getFillPath(skPath, &strokePath);
        if (stroke_color == fill_color &&
            Op(skPath, strokePath, SkPathOp::kUnion_SkPathOp, &strokePath)) {
          fillPath = &strokePath;
          do_stroke = false;
        } else if (Op(skPath, strokePath, SkPathOp::kDifference_SkPathOp,
                      &strokePath)) {
          fillPath = &strokePath;
        }
      }
    }
    skPaint.setStyle(SkPaint::kFill_Style);
    skPaint.setColor(fill_color);
#ifdef _SKIA_SUPPORT_PATHS_
    m_pBitmap->PreMultiply();
#endif  // _SKIA_SUPPORT_PATHS_
    DebugShowSkiaDrawPath(this, m_pCanvas, skPaint, *fillPath);
    m_pCanvas->drawPath(*fillPath, skPaint);
  }
  if (is_paint_stroke && do_stroke) {
    skPaint.setStyle(SkPaint::kStroke_Style);
    skPaint.setColor(stroke_color);
#ifdef _SKIA_SUPPORT_PATHS_
    m_pBitmap->PreMultiply();
#endif  // _SKIA_SUPPORT_PATHS_
    DebugShowSkiaDrawPath(this, m_pCanvas, skPaint, skPath);
    m_pCanvas->drawPath(skPath, skPaint);
  }
  return true;
}

bool CFX_SkiaDeviceDriver::DrawCosmeticLine(const CFX_PointF& ptMoveTo,
                                            const CFX_PointF& ptLineTo,
                                            uint32_t color,
                                            BlendMode blend_type) {
  return false;
}

bool CFX_SkiaDeviceDriver::FillRectWithBlend(const FX_RECT& rect,
                                             uint32_t fill_color,
                                             BlendMode blend_type) {
  m_pCache->FlushForDraw();
  SkPaint spaint;
  spaint.setAntiAlias(true);
  spaint.setColor(fill_color);
  spaint.setBlendMode(GetSkiaBlendMode(blend_type));
  SkRect srect = SkRect::MakeLTRB(rect.left, std::min(rect.top, rect.bottom),
                                  rect.right, std::max(rect.bottom, rect.top));
  DebugShowSkiaDrawRect(this, m_pCanvas, spaint, srect);
  m_pCanvas->drawRect(srect, spaint);
  return true;
}

bool CFX_SkiaDeviceDriver::DrawShading(const CPDF_ShadingPattern* pPattern,
                                       const CFX_Matrix* pMatrix,
                                       const FX_RECT& clip_rect,
                                       int alpha,
                                       bool bAlphaMode) {
  m_pCache->FlushForDraw();
  ShadingType shadingType = pPattern->GetShadingType();
  if (kAxialShading != shadingType && kRadialShading != shadingType &&
      kCoonsPatchMeshShading != shadingType) {
    // TODO(caryclark) more types
    return false;
  }
  int csFamily = pPattern->GetCS()->GetFamily();
  if (PDFCS_DEVICERGB != csFamily && PDFCS_DEVICEGRAY != csFamily)
    return false;
  const std::vector<std::unique_ptr<CPDF_Function>>& pFuncs =
      pPattern->GetFuncs();
  int nFuncs = pFuncs.size();
  if (nFuncs > 1)  // TODO(caryclark) remove this restriction
    return false;
  const CPDF_Dictionary* pDict = pPattern->GetShadingObject()->GetDict();
  const CPDF_Array* pCoords = pDict->GetArrayFor("Coords");
  if (!pCoords && kCoonsPatchMeshShading != shadingType)
    return false;
  // TODO(caryclark) Respect Domain[0], Domain[1]. (Don't know what they do
  // yet.)
  SkTDArray<SkColor> skColors;
  SkTDArray<SkScalar> skPos;
  for (int j = 0; j < nFuncs; j++) {
    if (!pFuncs[j])
      continue;

    if (const CPDF_SampledFunc* pSampledFunc = pFuncs[j]->ToSampledFunc()) {
      /* TODO(caryclark)
         Type 0 Sampled Functions in PostScript can also have an Order integer
         in the dictionary. PDFium doesn't appear to check for this anywhere.
       */
      if (!AddSamples(pSampledFunc, &skColors, &skPos))
        return false;
    } else if (const CPDF_ExpIntFunc* pExpIntFuc = pFuncs[j]->ToExpIntFunc()) {
      if (!AddColors(pExpIntFuc, &skColors, /*is_encode_reversed=*/false))
        return false;
      skPos.push_back(0);
      skPos.push_back(1);
    } else if (const CPDF_StitchFunc* pStitchFunc = pFuncs[j]->ToStitchFunc()) {
      if (!AddStitching(pStitchFunc, &skColors, &skPos))
        return false;
    } else {
      return false;
    }
  }
  const CPDF_Array* pArray = pDict->GetArrayFor("Extend");
  bool clipStart = !pArray || !pArray->GetIntegerAt(0);
  bool clipEnd = !pArray || !pArray->GetIntegerAt(1);
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setAlpha(alpha);
  SkMatrix skMatrix = ToSkMatrix(*pMatrix);
  SkRect skRect = SkRect::MakeLTRB(clip_rect.left, clip_rect.top,
                                   clip_rect.right, clip_rect.bottom);
  SkPath skClip;
  SkPath skPath;
  if (kAxialShading == shadingType) {
    float start_x = pCoords->GetNumberAt(0);
    float start_y = pCoords->GetNumberAt(1);
    float end_x = pCoords->GetNumberAt(2);
    float end_y = pCoords->GetNumberAt(3);
    SkPoint pts[] = {{start_x, start_y}, {end_x, end_y}};
    skMatrix.mapPoints(pts, SK_ARRAY_COUNT(pts));
    paint.setShader(
        SkGradientShader::MakeLinear(pts, skColors.begin(), skPos.begin(),
                                     skColors.count(), SkTileMode::kClamp));
    if (clipStart || clipEnd) {
      // if the gradient is horizontal or vertical, modify the draw rectangle
      if (pts[0].fX == pts[1].fX) {  // vertical
        if (pts[0].fY > pts[1].fY) {
          std::swap(pts[0].fY, pts[1].fY);
          std::swap(clipStart, clipEnd);
        }
        if (clipStart)
          skRect.fTop = std::max(skRect.fTop, pts[0].fY);
        if (clipEnd)
          skRect.fBottom = std::min(skRect.fBottom, pts[1].fY);
      } else if (pts[0].fY == pts[1].fY) {  // horizontal
        if (pts[0].fX > pts[1].fX) {
          std::swap(pts[0].fX, pts[1].fX);
          std::swap(clipStart, clipEnd);
        }
        if (clipStart)
          skRect.fLeft = std::max(skRect.fLeft, pts[0].fX);
        if (clipEnd)
          skRect.fRight = std::min(skRect.fRight, pts[1].fX);
      } else {  // if the gradient is angled and contained by the rect, clip
        SkPoint rectPts[4] = {{skRect.fLeft, skRect.fTop},
                              {skRect.fRight, skRect.fTop},
                              {skRect.fRight, skRect.fBottom},
                              {skRect.fLeft, skRect.fBottom}};
        ClipAngledGradient(pts, rectPts, clipStart, clipEnd, &skClip);
      }
    }
    skPath.addRect(skRect);
    skMatrix.setIdentity();
  } else if (kRadialShading == shadingType) {
    float start_x = pCoords->GetNumberAt(0);
    float start_y = pCoords->GetNumberAt(1);
    float start_r = pCoords->GetNumberAt(2);
    float end_x = pCoords->GetNumberAt(3);
    float end_y = pCoords->GetNumberAt(4);
    float end_r = pCoords->GetNumberAt(5);
    SkPoint pts[] = {{start_x, start_y}, {end_x, end_y}};

    paint.setShader(SkGradientShader::MakeTwoPointConical(
        pts[0], start_r, pts[1], end_r, skColors.begin(), skPos.begin(),
        skColors.count(), SkTileMode::kClamp));
    if (clipStart || clipEnd) {
      if (clipStart && start_r)
        skClip.addCircle(pts[0].fX, pts[0].fY, start_r);
      if (clipEnd)
        skClip.addCircle(pts[1].fX, pts[1].fY, end_r, SkPathDirection::kCCW);
      else
        skClip.setFillType(SkPathFillType::kInverseWinding);
      skClip.transform(skMatrix);
    }
    SkMatrix inverse;
    if (!skMatrix.invert(&inverse))
      return false;
    skPath.addRect(skRect);
    skPath.transform(inverse);
  } else {
    ASSERT(kCoonsPatchMeshShading == shadingType);
    const CPDF_Stream* pStream = ToStream(pPattern->GetShadingObject());
    if (!pStream)
      return false;
    CPDF_MeshStream stream(shadingType, pPattern->GetFuncs(), pStream,
                           pPattern->GetCS());
    if (!stream.Load())
      return false;
    SkPoint cubics[12];
    SkColor colors[4];
    SkAutoCanvasRestore scoped_save_restore(m_pCanvas, /*doSave=*/true);
    if (!skClip.isEmpty())
      m_pCanvas->clipPath(skClip, SkClipOp::kIntersect, true);
    m_pCanvas->concat(skMatrix);
    while (!stream.BitStream()->IsEOF()) {
      uint32_t flag = stream.ReadFlag();
      int iStartPoint = flag ? 4 : 0;
      int iStartColor = flag ? 2 : 0;
      if (flag) {
        SkPoint tempCubics[4];
        for (int i = 0; i < (int)SK_ARRAY_COUNT(tempCubics); i++)
          tempCubics[i] = cubics[(flag * 3 + i) % 12];
        memcpy(cubics, tempCubics, sizeof(tempCubics));
        SkColor tempColors[2];
        tempColors[0] = colors[flag];
        tempColors[1] = colors[(flag + 1) % 4];
        memcpy(colors, tempColors, sizeof(tempColors));
      }
      for (int i = iStartPoint; i < (int)SK_ARRAY_COUNT(cubics); i++) {
        CFX_PointF point = stream.ReadCoords();
        cubics[i].fX = point.x;
        cubics[i].fY = point.y;
      }
      for (int i = iStartColor; i < (int)SK_ARRAY_COUNT(colors); i++) {
        float r;
        float g;
        float b;
        std::tie(r, g, b) = stream.ReadColor();
        colors[i] = SkColorSetARGB(0xFF, (U8CPU)(r * 255), (U8CPU)(g * 255),
                                   (U8CPU)(b * 255));
      }
      m_pCanvas->drawPatch(cubics, colors, nullptr, paint);
    }
    return true;
  }
  SkAutoCanvasRestore scoped_save_restore(m_pCanvas, /*doSave=*/true);
  if (!skClip.isEmpty())
    m_pCanvas->clipPath(skClip, SkClipOp::kIntersect, true);
  m_pCanvas->concat(skMatrix);
  m_pCanvas->drawPath(skPath, paint);
  return true;
}

uint8_t* CFX_SkiaDeviceDriver::GetBuffer() const {
  return m_pBitmap->GetBuffer();
}

bool CFX_SkiaDeviceDriver::GetClipBox(FX_RECT* pRect) {
#ifdef _SKIA_SUPPORT_PATHS_
  if (!m_pClipRgn) {
    pRect->left = pRect->top = 0;
    pRect->right = GetDeviceCaps(FXDC_PIXEL_WIDTH);
    pRect->bottom = GetDeviceCaps(FXDC_PIXEL_HEIGHT);
    return true;
  }
  *pRect = m_pClipRgn->GetBox();
#else
  // TODO(caryclark) call m_canvas->getClipDeviceBounds() instead
  pRect->left = 0;
  pRect->top = 0;
  const SkImageInfo& canvasSize = m_pCanvas->imageInfo();
  pRect->right = canvasSize.width();
  pRect->bottom = canvasSize.height();
#endif
  return true;
}

bool CFX_SkiaDeviceDriver::GetDIBits(const RetainPtr<CFX_DIBitmap>& pBitmap,
                                     int left,
                                     int top) {
  if (!m_pBitmap)
    return true;
  uint8_t* srcBuffer = m_pBitmap->GetBuffer();
  if (!srcBuffer)
    return true;
#ifdef _SKIA_SUPPORT_
  m_pCache->FlushForDraw();
  int srcWidth = m_pBitmap->GetWidth();
  int srcHeight = m_pBitmap->GetHeight();
  int srcRowBytes = srcWidth * sizeof(uint32_t);
  SkImageInfo srcImageInfo = SkImageInfo::Make(
      srcWidth, srcHeight, SkColorType::kN32_SkColorType, kPremul_SkAlphaType);
  SkBitmap skSrcBitmap;
  skSrcBitmap.installPixels(srcImageInfo, srcBuffer, srcRowBytes);
  uint8_t* dstBuffer = pBitmap->GetBuffer();
  ASSERT(dstBuffer);
  int dstWidth = pBitmap->GetWidth();
  int dstHeight = pBitmap->GetHeight();
  int dstRowBytes = dstWidth * sizeof(uint32_t);
  SkImageInfo dstImageInfo = SkImageInfo::Make(
      dstWidth, dstHeight, Get32BitSkColorType(m_bRgbByteOrder),
      kPremul_SkAlphaType);
  SkBitmap skDstBitmap;
  skDstBitmap.installPixels(dstImageInfo, dstBuffer, dstRowBytes);
  SkCanvas canvas(skDstBitmap);
  canvas.drawBitmap(skSrcBitmap, left, top, nullptr);
  return true;
#endif  // _SKIA_SUPPORT_

#ifdef _SKIA_SUPPORT_PATHS_
  Flush();
  m_pBitmap->UnPreMultiply();
  FX_RECT rect(left, top, left + pBitmap->GetWidth(),
               top + pBitmap->GetHeight());
  RetainPtr<CFX_DIBitmap> pBack;
  if (m_pBackdropBitmap) {
    pBack = m_pBackdropBitmap->Clone(&rect);
    if (!pBack)
      return true;

    pBack->CompositeBitmap(0, 0, pBack->GetWidth(), pBack->GetHeight(),
                           m_pBitmap, 0, 0, BlendMode::kNormal, nullptr, false);
  } else {
    pBack = m_pBitmap->Clone(&rect);
    if (!pBack)
      return true;
  }

  bool bRet = true;
  left = std::min(left, 0);
  top = std::min(top, 0);
  if (m_bRgbByteOrder) {
    RgbByteOrderTransferBitmap(pBitmap, 0, 0, rect.Width(), rect.Height(),
                               pBack, left, top);
  } else {
    bRet = pBitmap->TransferBitmap(0, 0, rect.Width(), rect.Height(), pBack,
                                   left, top);
  }
  return bRet;
#endif  // _SKIA_SUPPORT_PATHS_
}

RetainPtr<CFX_DIBitmap> CFX_SkiaDeviceDriver::GetBackDrop() {
  return m_pBackdropBitmap;
}

bool CFX_SkiaDeviceDriver::SetDIBits(const RetainPtr<CFX_DIBBase>& pBitmap,
                                     uint32_t argb,
                                     const FX_RECT& src_rect,
                                     int left,
                                     int top,
                                     BlendMode blend_type) {
  if (!m_pBitmap || !m_pBitmap->GetBuffer())
    return true;

#ifdef _SKIA_SUPPORT_
  CFX_Matrix m = CFX_RenderDevice::GetFlipMatrix(
      pBitmap->GetWidth(), pBitmap->GetHeight(), left, top);
  std::unique_ptr<CFX_ImageRenderer> dummy;
  return StartDIBits(pBitmap, 0xFF, argb, m, FXDIB_ResampleOptions(), &dummy,
                     blend_type);
#endif  // _SKIA_SUPPORT_

#ifdef _SKIA_SUPPORT_PATHS_
  Flush();
  if (pBitmap->IsAlphaMask()) {
    return m_pBitmap->CompositeMask(left, top, src_rect.Width(),
                                    src_rect.Height(), pBitmap, argb,
                                    src_rect.left, src_rect.top, blend_type,
                                    m_pClipRgn.get(), m_bRgbByteOrder);
  }
  return m_pBitmap->CompositeBitmap(
      left, top, src_rect.Width(), src_rect.Height(), pBitmap, src_rect.left,
      src_rect.top, blend_type, m_pClipRgn.get(), m_bRgbByteOrder);
#endif  // _SKIA_SUPPORT_PATHS_
}

bool CFX_SkiaDeviceDriver::StretchDIBits(const RetainPtr<CFX_DIBBase>& pSource,
                                         uint32_t argb,
                                         int dest_left,
                                         int dest_top,
                                         int dest_width,
                                         int dest_height,
                                         const FX_RECT* pClipRect,
                                         const FXDIB_ResampleOptions& options,
                                         BlendMode blend_type) {
#ifdef _SKIA_SUPPORT_
  m_pCache->FlushForDraw();
  if (!m_pBitmap->GetBuffer())
    return true;

  CFX_Matrix m = CFX_RenderDevice::GetFlipMatrix(dest_width, dest_height,
                                                 dest_left, dest_top);
  SkAutoCanvasRestore scoped_save_restore(m_pCanvas, /*doSave=*/true);
  SkRect skClipRect = SkRect::MakeLTRB(pClipRect->left, pClipRect->bottom,
                                       pClipRect->right, pClipRect->top);
  m_pCanvas->clipRect(skClipRect, SkClipOp::kIntersect, true);
  std::unique_ptr<CFX_ImageRenderer> dummy;
  return StartDIBits(pSource, 0xFF, argb, m, FXDIB_ResampleOptions(), &dummy,
                     blend_type);
#endif  // _SKIA_SUPPORT_

#ifdef _SKIA_SUPPORT_PATHS_
  if (dest_width == pSource->GetWidth() &&
      dest_height == pSource->GetHeight()) {
    FX_RECT rect(0, 0, dest_width, dest_height);
    return SetDIBits(pSource, argb, rect, dest_left, dest_top, blend_type);
  }
  Flush();
  FX_RECT dest_rect(dest_left, dest_top, dest_left + dest_width,
                    dest_top + dest_height);
  dest_rect.Normalize();
  FX_RECT dest_clip = dest_rect;
  dest_clip.Intersect(*pClipRect);
  CFX_BitmapComposer composer;
  composer.Compose(m_pBitmap, m_pClipRgn.get(), 255, argb, dest_clip, false,
                   false, false, m_bRgbByteOrder, blend_type);
  dest_clip.Offset(-dest_rect.left, -dest_rect.top);
  CFX_ImageStretcher stretcher(&composer, pSource, dest_width, dest_height,
                               dest_clip, options);
  if (stretcher.Start())
    stretcher.Continue(nullptr);
  return true;
#endif  // _SKIA_SUPPORT_PATHS_
}

bool CFX_SkiaDeviceDriver::StartDIBits(
    const RetainPtr<CFX_DIBBase>& pSource,
    int bitmap_alpha,
    uint32_t argb,
    const CFX_Matrix& matrix,
    const FXDIB_ResampleOptions& options,
    std::unique_ptr<CFX_ImageRenderer>* handle,
    BlendMode blend_type) {
#ifdef _SKIA_SUPPORT_
  m_pCache->FlushForDraw();
  DebugValidate(m_pBitmap, m_pBackdropBitmap);
  std::unique_ptr<uint8_t, FxFreeDeleter> dst8Storage;
  std::unique_ptr<uint32_t, FxFreeDeleter> dst32Storage;
  SkBitmap skBitmap;
  int width, height;
  if (!Upsample(pSource, dst8Storage, dst32Storage, &skBitmap, &width, &height,
                false, m_bRgbByteOrder)) {
    return false;
  }
  {
    SkAutoCanvasRestore scoped_save_restore(m_pCanvas, /*doSave=*/true);
    SkMatrix skMatrix;
    SetBitmapMatrix(matrix, width, height, &skMatrix);
    m_pCanvas->concat(skMatrix);
    SkPaint paint;
    SetBitmapPaint(pSource->IsAlphaMask(), argb, bitmap_alpha, blend_type,
                   &paint);
    // TODO(caryclark) Once Skia supports 8 bit src to 8 bit dst remove this
    if (m_pBitmap && m_pBitmap->GetBPP() == 8 && pSource->GetBPP() == 8) {
      SkMatrix inv;
      SkAssertResult(skMatrix.invert(&inv));
      for (int y = 0; y < m_pBitmap->GetHeight(); ++y) {
        for (int x = 0; x < m_pBitmap->GetWidth(); ++x) {
          SkPoint src = {x + 0.5f, y + 0.5f};
          inv.mapPoints(&src, 1);
          // TODO(caryclark) Why does the matrix map require clamping?
          src.fX = pdfium::clamp(src.fX, 0.5f, width - 0.5f);
          src.fY = pdfium::clamp(src.fY, 0.5f, height - 0.5f);
          m_pBitmap->SetPixel(x, y, skBitmap.getColor(src.fX, src.fY));
        }
      }
    } else {
      m_pCanvas->drawBitmap(skBitmap, 0, 0, &paint);
    }
  }
  DebugValidate(m_pBitmap, m_pBackdropBitmap);
#endif  // _SKIA_SUPPORT_

#ifdef _SKIA_SUPPORT_PATHS_
  Flush();
  if (!m_pBitmap->GetBuffer())
    return true;
  m_pBitmap->UnPreMultiply();
  *handle = pdfium::MakeUnique<CFX_ImageRenderer>(
      m_pBitmap, m_pClipRgn.get(), pSource, bitmap_alpha, argb, matrix, options,
      m_bRgbByteOrder);
#endif  // _SKIA_SUPPORT_PATHS_
  return true;
}

bool CFX_SkiaDeviceDriver::ContinueDIBits(CFX_ImageRenderer* handle,
                                          PauseIndicatorIface* pPause) {
#ifdef _SKIA_SUPPORT_
  m_pCache->FlushForDraw();
  return false;
#endif  // _SKIA_SUPPORT_

#ifdef _SKIA_SUPPORT_PATHS_
  Flush();
  if (!m_pBitmap->GetBuffer()) {
    return true;
  }
  return handle->Continue(pPause);
#endif  // _SKIA_SUPPORT_PATHS_
}

#if defined _SKIA_SUPPORT_
void CFX_SkiaDeviceDriver::PreMultiply(
    const RetainPtr<CFX_DIBitmap>& pDIBitmap) {
  pDIBitmap->PreMultiply();
}
#endif  // _SKIA_SUPPORT_

void CFX_DIBitmap::PreMultiply() {
  if (this->GetBPP() != 32)
    return;
  void* buffer = this->GetBuffer();
  if (!buffer)
    return;
#if defined _SKIA_SUPPORT_PATHS_
  Format priorFormat = m_nFormat;
  m_nFormat = Format::kPreMultiplied;
  if (priorFormat != Format::kUnPreMultiplied)
    return;
#endif
  int height = this->GetHeight();
  int width = this->GetWidth();
  int rowBytes = this->GetPitch();
  SkImageInfo unpremultipliedInfo =
      SkImageInfo::Make(width, height, kN32_SkColorType, kUnpremul_SkAlphaType);
  SkPixmap unpremultiplied(unpremultipliedInfo, buffer, rowBytes);
  SkImageInfo premultipliedInfo =
      SkImageInfo::Make(width, height, kN32_SkColorType, kPremul_SkAlphaType);
  SkPixmap premultiplied(premultipliedInfo, buffer, rowBytes);
  unpremultiplied.readPixels(premultiplied);
  this->DebugVerifyBitmapIsPreMultiplied(nullptr);
}

#ifdef _SKIA_SUPPORT_PATHS_
void CFX_DIBitmap::UnPreMultiply() {
  if (this->GetBPP() != 32)
    return;
  void* buffer = this->GetBuffer();
  if (!buffer)
    return;
  Format priorFormat = m_nFormat;
  m_nFormat = Format::kUnPreMultiplied;
  if (priorFormat != Format::kPreMultiplied)
    return;
  this->DebugVerifyBitmapIsPreMultiplied(nullptr);
  int height = this->GetHeight();
  int width = this->GetWidth();
  int rowBytes = this->GetPitch();
  SkImageInfo premultipliedInfo =
      SkImageInfo::Make(width, height, kN32_SkColorType, kPremul_SkAlphaType);
  SkPixmap premultiplied(premultipliedInfo, buffer, rowBytes);
  SkImageInfo unpremultipliedInfo =
      SkImageInfo::Make(width, height, kN32_SkColorType, kUnpremul_SkAlphaType);
  SkPixmap unpremultiplied(unpremultipliedInfo, buffer, rowBytes);
  premultiplied.readPixels(unpremultiplied);
}
#endif  // _SKIA_SUPPORT_PATHS_

#ifdef _SKIA_SUPPORT_
bool CFX_SkiaDeviceDriver::DrawBitsWithMask(
    const RetainPtr<CFX_DIBBase>& pSource,
    const RetainPtr<CFX_DIBBase>& pMask,
    int bitmap_alpha,
    const CFX_Matrix& matrix,
    BlendMode blend_type) {
  DebugValidate(m_pBitmap, m_pBackdropBitmap);
  std::unique_ptr<uint8_t, FxFreeDeleter> src8Storage, mask8Storage;
  std::unique_ptr<uint32_t, FxFreeDeleter> src32Storage, mask32Storage;
  SkBitmap skBitmap, skMask;
  int srcWidth, srcHeight, maskWidth, maskHeight;
  if (!Upsample(pSource, src8Storage, src32Storage, &skBitmap, &srcWidth,
                &srcHeight, false, m_bRgbByteOrder)) {
    return false;
  }
  if (!Upsample(pMask, mask8Storage, mask32Storage, &skMask, &maskWidth,
                &maskHeight, true, m_bRgbByteOrder)) {
    return false;
  }
  {
    SkAutoCanvasRestore scoped_save_restore(m_pCanvas, /*doSave=*/true);
    SkMatrix skMatrix;
    SetBitmapMatrix(matrix, srcWidth, srcHeight, &skMatrix);
    m_pCanvas->concat(skMatrix);
    SkPaint paint;
    SetBitmapPaint(pSource->IsAlphaMask(), 0xFFFFFFFF, bitmap_alpha, blend_type,
                   &paint);
    sk_sp<SkImage> skSrc = SkImage::MakeFromBitmap(skBitmap);
    sk_sp<SkShader> skSrcShader =
        skSrc->makeShader(SkTileMode::kClamp, SkTileMode::kClamp);
    sk_sp<SkImage> skMaskImage = SkImage::MakeFromBitmap(skMask);
    sk_sp<SkShader> skMaskShader =
        skMaskImage->makeShader(SkTileMode::kClamp, SkTileMode::kClamp);
    paint.setShader(
        SkShaders::Blend(SkBlendMode::kSrcIn, skMaskShader, skSrcShader));
    SkRect r = {0, 0, SkIntToScalar(srcWidth), SkIntToScalar(srcHeight)};
    m_pCanvas->drawRect(r, paint);
  }
  DebugValidate(m_pBitmap, m_pBackdropBitmap);
  return true;
}

bool CFX_SkiaDeviceDriver::SetBitsWithMask(
    const RetainPtr<CFX_DIBBase>& pBitmap,
    const RetainPtr<CFX_DIBBase>& pMask,
    int dest_left,
    int dest_top,
    int bitmap_alpha,
    BlendMode blend_type) {
  if (!m_pBitmap || !m_pBitmap->GetBuffer())
    return true;

  CFX_Matrix m = CFX_RenderDevice::GetFlipMatrix(
      pBitmap->GetWidth(), pBitmap->GetHeight(), dest_left, dest_top);
  return DrawBitsWithMask(pBitmap, pMask, bitmap_alpha, m, blend_type);
}

void CFX_SkiaDeviceDriver::Clear(uint32_t color) {
  m_pCanvas->clear(color);
}
#endif  // _SKIA_SUPPORT_

void CFX_SkiaDeviceDriver::Dump() const {
#if SHOW_SKIA_PATH && defined _SKIA_SUPPORT_
  if (m_pCache)
    m_pCache->Dump(this);
#endif  // SHOW_SKIA_PATH && defined _SKIA_SUPPORT_
}

#ifdef _SKIA_SUPPORT_
void CFX_SkiaDeviceDriver::DebugVerifyBitmapIsPreMultiplied() const {
  if (m_pBackdropBitmap)
    m_pBackdropBitmap->DebugVerifyBitmapIsPreMultiplied(nullptr);
}
#endif  // _SKIA_SUPPORT_

CFX_DefaultRenderDevice::CFX_DefaultRenderDevice() {}

#ifdef _SKIA_SUPPORT_
void CFX_DefaultRenderDevice::Clear(uint32_t color) {
  CFX_SkiaDeviceDriver* skDriver =
      static_cast<CFX_SkiaDeviceDriver*>(GetDeviceDriver());
  skDriver->Clear(color);
}

SkPictureRecorder* CFX_DefaultRenderDevice::CreateRecorder(int size_x,
                                                           int size_y) {
  CFX_SkiaDeviceDriver* skDriver = new CFX_SkiaDeviceDriver(size_x, size_y);
  SetDeviceDriver(pdfium::WrapUnique(skDriver));
  return skDriver->GetRecorder();
}
#endif  // _SKIA_SUPPORT_

bool CFX_DefaultRenderDevice::Attach(
    const RetainPtr<CFX_DIBitmap>& pBitmap,
    bool bRgbByteOrder,
    const RetainPtr<CFX_DIBitmap>& pBackdropBitmap,
    bool bGroupKnockout) {
  if (!pBitmap)
    return false;
  SetBitmap(pBitmap);
  SetDeviceDriver(pdfium::MakeUnique<CFX_SkiaDeviceDriver>(
      pBitmap, bRgbByteOrder, pBackdropBitmap, bGroupKnockout));
  return true;
}

#ifdef _SKIA_SUPPORT_
bool CFX_DefaultRenderDevice::AttachRecorder(SkPictureRecorder* recorder) {
  if (!recorder)
    return false;
  SetDeviceDriver(pdfium::MakeUnique<CFX_SkiaDeviceDriver>(recorder));
  return true;
}
#endif  // _SKIA_SUPPORT_

bool CFX_DefaultRenderDevice::Create(
    int width,
    int height,
    FXDIB_Format format,
    const RetainPtr<CFX_DIBitmap>& pBackdropBitmap) {
  auto pBitmap = pdfium::MakeRetain<CFX_DIBitmap>();
  if (!pBitmap->Create(width, height, format)) {
    return false;
  }
  SetBitmap(pBitmap);
  SetDeviceDriver(pdfium::MakeUnique<CFX_SkiaDeviceDriver>(
      pBitmap, false, pBackdropBitmap, false));
  return true;
}

CFX_DefaultRenderDevice::~CFX_DefaultRenderDevice() {
  Flush(true);
}

#ifdef _SKIA_SUPPORT_
void CFX_DefaultRenderDevice::DebugVerifyBitmapIsPreMultiplied() const {
#ifdef SK_DEBUG
  CFX_SkiaDeviceDriver* skDriver =
      static_cast<CFX_SkiaDeviceDriver*>(GetDeviceDriver());
  if (skDriver)
    skDriver->DebugVerifyBitmapIsPreMultiplied();
#endif  // SK_DEBUG
}

bool CFX_DefaultRenderDevice::SetBitsWithMask(
    const RetainPtr<CFX_DIBBase>& pBitmap,
    const RetainPtr<CFX_DIBBase>& pMask,
    int left,
    int top,
    int bitmap_alpha,
    BlendMode blend_type) {
  CFX_SkiaDeviceDriver* skDriver =
      static_cast<CFX_SkiaDeviceDriver*>(GetDeviceDriver());
  if (skDriver)
    return skDriver->SetBitsWithMask(pBitmap, pMask, left, top, bitmap_alpha,
                                     blend_type);
  return false;
}
#endif  // _SKIA_SUPPORT_

void CFX_DIBBase::DebugVerifyBitmapIsPreMultiplied(void* opt) const {
#ifdef SK_DEBUG
  ASSERT(GetBPP() == 32);
  const uint32_t* buffer = (const uint32_t*)(opt ? opt : GetBuffer());
  int width = GetWidth();
  int height = GetHeight();
  // verify that input is really premultiplied
  for (int y = 0; y < height; ++y) {
    const uint32_t* srcRow = buffer + y * width;
    for (int x = 0; x < width; ++x) {
      uint8_t a = SkGetPackedA32(srcRow[x]);
      uint8_t r = SkGetPackedR32(srcRow[x]);
      uint8_t g = SkGetPackedG32(srcRow[x]);
      uint8_t b = SkGetPackedB32(srcRow[x]);
      SkA32Assert(a);
      ASSERT(r <= a);
      ASSERT(g <= a);
      ASSERT(b <= a);
    }
  }
#endif  // SK_DEBUG
}
