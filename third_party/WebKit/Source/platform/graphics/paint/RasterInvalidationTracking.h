// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RasterInvalidationTracking_h
#define RasterInvalidationTracking_h

#include "platform/RuntimeEnabledFeatures.h"
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
  IntRect rect;
  PaintInvalidationReason reason = kPaintInvalidationFull;
};

inline bool operator==(const RasterInvalidationInfo& a,
                       const RasterInvalidationInfo& b) {
  return a.rect == b.rect;
}

struct UnderRasterInvalidation {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
  int x;
  int y;
  SkColor old_pixel;
  SkColor new_pixel;
};

struct PLATFORM_EXPORT RasterInvalidationTracking {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
  Vector<RasterInvalidationInfo> invalidations;

  // The following fields are for under-raster-invalidation detection.
  sk_sp<PaintRecord> last_painted_record;
  IntRect last_interest_rect;
  Region invalidation_region_since_last_paint;
  Vector<UnderRasterInvalidation> under_invalidations;

  void AsJSON(JSONObject*);
};

template <class TargetClass>
class PLATFORM_EXPORT RasterInvalidationTrackingMap {
 public:
  void AsJSON(TargetClass* key, JSONObject* json) {
    auto it = map_.find(key);
    if (it != map_.end())
      it->value.AsJSON(json);
  }

  void Remove(TargetClass* key) {
    auto it = map_.find(key);
    if (it != map_.end())
      map_.erase(it);
  }

  RasterInvalidationTracking& Add(TargetClass* key) {
    return map_.insert(key, RasterInvalidationTracking()).stored_value->value;
  }

  RasterInvalidationTracking* Find(TargetClass* key) {
    auto it = map_.find(key);
    if (it == map_.end())
      return nullptr;
    return &it->value;
  }

 private:
  typedef HashMap<TargetClass*, RasterInvalidationTracking>
      InvalidationTrackingMap;
  InvalidationTrackingMap map_;
};

}  // namespace blink

#endif  // RasterInvalidationTracking_h
