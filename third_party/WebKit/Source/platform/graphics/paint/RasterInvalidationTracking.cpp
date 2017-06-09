// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/RasterInvalidationTracking.h"

#include "SkImageFilter.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/Color.h"
#include "platform/graphics/paint/PaintCanvas.h"
#include "platform/graphics/paint/PaintRecorder.h"

namespace blink {

static bool CompareRasterInvalidationInfo(const RasterInvalidationInfo& a,
                                          const RasterInvalidationInfo& b) {
  // Sort by rect first, bigger rects before smaller ones.
  if (a.rect.Width() != b.rect.Width())
    return a.rect.Width() > b.rect.Width();
  if (a.rect.Height() != b.rect.Height())
    return a.rect.Height() > b.rect.Height();
  if (a.rect.X() != b.rect.X())
    return a.rect.X() > b.rect.X();
  if (a.rect.Y() != b.rect.Y())
    return a.rect.Y() > b.rect.Y();

  // Then compare clientDebugName, in alphabetic order.
  int name_compare_result =
      CodePointCompare(a.client_debug_name, b.client_debug_name);
  if (name_compare_result != 0)
    return name_compare_result < 0;

  return a.reason < b.reason;
}

template <typename T>
static std::unique_ptr<JSONArray> RectAsJSONArray(const T& rect) {
  std::unique_ptr<JSONArray> array = JSONArray::Create();
  array->PushDouble(rect.X());
  array->PushDouble(rect.Y());
  array->PushDouble(rect.Width());
  array->PushDouble(rect.Height());
  return array;
}

void RasterInvalidationTracking::AsJSON(JSONObject* json) {
  if (!invalidations.IsEmpty()) {
    std::sort(invalidations.begin(), invalidations.end(),
              &CompareRasterInvalidationInfo);
    std::unique_ptr<JSONArray> paint_invalidations_json = JSONArray::Create();
    for (auto& info : invalidations) {
      std::unique_ptr<JSONObject> info_json = JSONObject::Create();
      info_json->SetString("object", info.client_debug_name);
      if (!info.rect.IsEmpty()) {
        if (info.rect == LayoutRect::InfiniteIntRect())
          info_json->SetString("rect", "infinite");
        else
          info_json->SetArray("rect", RectAsJSONArray(info.rect));
      }
      info_json->SetString("reason",
                           PaintInvalidationReasonToString(info.reason));
      paint_invalidations_json->PushObject(std::move(info_json));
    }
    json->SetArray("paintInvalidations", std::move(paint_invalidations_json));
  }

  if (!under_invalidations.IsEmpty()) {
    std::unique_ptr<JSONArray> under_paint_invalidations_json =
        JSONArray::Create();
    for (auto& under_paint_invalidation : under_invalidations) {
      std::unique_ptr<JSONObject> under_paint_invalidation_json =
          JSONObject::Create();
      under_paint_invalidation_json->SetDouble("x", under_paint_invalidation.x);
      under_paint_invalidation_json->SetDouble("y", under_paint_invalidation.y);
      under_paint_invalidation_json->SetString(
          "oldPixel",
          Color(under_paint_invalidation.old_pixel).NameForLayoutTreeAsText());
      under_paint_invalidation_json->SetString(
          "newPixel",
          Color(under_paint_invalidation.new_pixel).NameForLayoutTreeAsText());
      under_paint_invalidations_json->PushObject(
          std::move(under_paint_invalidation_json));
    }
    json->SetArray("underPaintInvalidations",
                   std::move(under_paint_invalidations_json));
  }
}

static bool PixelComponentsDiffer(int c1, int c2) {
  // Compare strictly for saturated values.
  if (c1 == 0 || c1 == 255 || c2 == 0 || c2 == 255)
    return c1 != c2;
  // Tolerate invisible differences that may occur in gradients etc.
  return abs(c1 - c2) > 2;
}

static bool PixelsDiffer(SkColor p1, SkColor p2) {
  return PixelComponentsDiffer(SkColorGetA(p1), SkColorGetA(p2)) ||
         PixelComponentsDiffer(SkColorGetR(p1), SkColorGetR(p2)) ||
         PixelComponentsDiffer(SkColorGetG(p1), SkColorGetG(p2)) ||
         PixelComponentsDiffer(SkColorGetB(p1), SkColorGetB(p2));
}

void RasterInvalidationTracking::CheckUnderInvalidations(
    const String& layer_debug_name,
    sk_sp<PaintRecord> new_record,
    const IntRect& new_interest_rect) {
  auto old_interest_rect = last_interest_rect;
  auto invalidation_region = invalidation_region_since_last_paint;
  auto old_record = std::move(last_painted_record);

  last_painted_record = new_record;
  last_interest_rect = new_interest_rect;
  invalidation_region_since_last_paint = Region();

  if (!old_record)
    return;

  IntRect rect = Intersection(old_interest_rect, new_interest_rect);
  // Avoid too big area as the following code is slow.
  rect.Intersect(IntRect(0, 0, 1200, 1200));
  if (rect.IsEmpty())
    return;

  SkBitmap old_bitmap;
  old_bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(rect.Width(), rect.Height()));
  {
    SkiaPaintCanvas canvas(old_bitmap);
    canvas.clear(SK_ColorTRANSPARENT);
    canvas.translate(-rect.X(), -rect.Y());
    canvas.drawPicture(std::move(old_record));
  }

  SkBitmap new_bitmap;
  new_bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(rect.Width(), rect.Height()));
  {
    SkiaPaintCanvas canvas(new_bitmap);
    canvas.clear(SK_ColorTRANSPARENT);
    canvas.translate(-rect.X(), -rect.Y());
    canvas.drawPicture(std::move(new_record));
  }

  int mismatching_pixels = 0;
  static const int kMaxMismatchesToReport = 50;
  for (int bitmap_y = 0; bitmap_y < rect.Height(); ++bitmap_y) {
    int layer_y = bitmap_y + rect.Y();
    for (int bitmap_x = 0; bitmap_x < rect.Width(); ++bitmap_x) {
      int layer_x = bitmap_x + rect.X();
      SkColor old_pixel = old_bitmap.getColor(bitmap_x, bitmap_y);
      SkColor new_pixel = new_bitmap.getColor(bitmap_x, bitmap_y);
      if (PixelsDiffer(old_pixel, new_pixel) &&
          !invalidation_region.Contains(IntPoint(layer_x, layer_y))) {
        if (mismatching_pixels < kMaxMismatchesToReport) {
          RasterUnderInvalidation under_invalidation = {layer_x, layer_y,
                                                        old_pixel, new_pixel};
          under_invalidations.push_back(under_invalidation);
          LOG(ERROR) << layer_debug_name
                     << " Uninvalidated old/new pixels mismatch at " << layer_x
                     << "," << layer_y << " old:" << std::hex << old_pixel
                     << " new:" << new_pixel;
        } else if (mismatching_pixels == kMaxMismatchesToReport) {
          LOG(ERROR) << "and more...";
        }
        ++mismatching_pixels;
        *new_bitmap.getAddr32(bitmap_x, bitmap_y) =
            SkColorSetARGB(0xFF, 0xA0, 0, 0);  // Dark red.
      } else {
        *new_bitmap.getAddr32(bitmap_x, bitmap_y) = SK_ColorTRANSPARENT;
      }
    }
  }

  if (!mismatching_pixels)
    return;

  PaintRecorder recorder;
  recorder.beginRecording(rect);
  auto* canvas = recorder.getRecordingCanvas();
  if (under_invalidation_record)
    canvas->drawPicture(std::move(under_invalidation_record));
  canvas->drawBitmap(new_bitmap, rect.X(), rect.Y());
  under_invalidation_record = recorder.finishRecordingAsPicture();
}

}  // namespace blink
