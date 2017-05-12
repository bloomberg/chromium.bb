// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/RasterInvalidationTracking.h"

#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/Color.h"

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

}  // namespace blink
