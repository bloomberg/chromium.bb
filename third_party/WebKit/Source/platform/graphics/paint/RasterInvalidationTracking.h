// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RasterInvalidationTracking_h
#define RasterInvalidationTracking_h

#include "platform/geometry/IntRect.h"
#include "platform/geometry/Region.h"
#include "platform/graphics/PaintInvalidationReason.h"
#include "platform/graphics/paint/PaintRecord.h"
#include "platform/json/JSONValues.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/text/WTFString.h"
#include "third_party/skia/include/core/SkColor.h"

namespace blink {

class DisplayItemClient;

struct RasterInvalidationInfo {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
  // This is for comparison only. Don't dereference because the client may have
  // died.
  const DisplayItemClient* client = nullptr;
  String client_debug_name;
  // For SPv2, this is set in PaintArtifactCompositor when converting chunk
  // raster invalidations to cc raster invalidations.
  IntRect rect;
  PaintInvalidationReason reason = PaintInvalidationReason::kFull;
};

inline bool operator==(const RasterInvalidationInfo& a,
                       const RasterInvalidationInfo& b) {
  return a.rect == b.rect;
}

struct RasterUnderInvalidation {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
  int x;
  int y;
  SkColor old_pixel;
  SkColor new_pixel;
};

class PLATFORM_EXPORT RasterInvalidationTracking {
 public:
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

  // When RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() and
  // simulateRasterUnderInvalidation(true) is called, all changed pixels will
  // be reported as raster under-invalidations. Used to visually test raster
  // under-invalidation checking feature.
  static void SimulateRasterUnderInvalidations(bool enable);

  void AddInvalidation(const DisplayItemClient*,
                       const String& debug_name,
                       const IntRect&,
                       PaintInvalidationReason);
  bool HasInvalidations() const { return !invalidations_.IsEmpty(); }
  const Vector<RasterInvalidationInfo>& Invalidations() const {
    return invalidations_;
  }
  void ClearInvalidations() { invalidations_.clear(); }

  // Compares the last recording against |new_record|, by rastering both into
  // bitmaps. If there are any differences outside of invalidated regions,
  // the corresponding pixels in UnderInvalidationRecord() will be drawn in
  // dark red. The caller can overlay UnderInvalidationRecord() onto the
  // original drawings to show the under raster invalidations.
  void CheckUnderInvalidations(const String& layer_debug_name,
                               sk_sp<PaintRecord> new_record,
                               const IntRect& new_interest_rect);

  void AsJSON(JSONObject*);

  // The record containing under-invalidated pixels in dark red.
  sk_sp<const PaintRecord> UnderInvalidationRecord() const {
    return under_invalidation_record_;
  }

 private:
  Vector<RasterInvalidationInfo> invalidations_;

  // The following fields are for raster under-invalidation detection.
  sk_sp<PaintRecord> last_painted_record_;
  IntRect last_interest_rect_;
  Region invalidation_region_since_last_paint_;
  Vector<RasterUnderInvalidation> under_invalidations_;
  sk_sp<PaintRecord> under_invalidation_record_;
};

}  // namespace blink

#endif  // RasterInvalidationTracking_h
