// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DrawingDisplayItem_h
#define DrawingDisplayItem_h

#include "base/compiler_specific.h"
#include "platform/PlatformExport.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "platform/graphics/paint/PaintRecord.h"
#include "platform/runtime_enabled_features.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

// DrawingDisplayItem contains recorded painting operations which can be
// replayed to produce a rastered output.
//
// This class has two notions of the bounds around the content that was recorded
// and will be produced by the item. The first is the |record_bounds| which
// describes the bounds of all content in the |record| in the space of the
// record. The second is the |visual_rect| which should describe the same thing,
// but takes into account transforms and clips that would apply to the
// PaintRecord, and is in the space of the DisplayItemList. This allows the
// visual_rect to be compared between DrawingDisplayItems, and to give bounds
// around what the user can actually see from the PaintRecord.
class PLATFORM_EXPORT DrawingDisplayItem final : public DisplayItem {
 public:
  DISABLE_CFI_PERF
  DrawingDisplayItem(const DisplayItemClient& client,
                     Type type,
                     sk_sp<const PaintRecord> record,
                     bool known_to_be_opaque);

  void Replay(GraphicsContext&) const override;
  void AppendToWebDisplayItemList(const LayoutSize& visual_rect_offset,
                                  WebDisplayItemList*) const override;
  bool DrawsContent() const override;

  const sk_sp<const PaintRecord>& GetPaintRecord() const { return record_; }

  bool KnownToBeOpaque() const {
    DCHECK(RuntimeEnabledFeatures::SlimmingPaintV2Enabled());
    return known_to_be_opaque_;
  }

  bool Equals(const DisplayItem& other) const final;

 private:
#if DCHECK_IS_ON()
  void PropertiesAsJSON(JSONObject&) const override;
#endif

  sk_sp<const PaintRecord> record_;

  // True if there are no transparent areas. Only used for SlimmingPaintV2.
  const bool known_to_be_opaque_;
};

// TODO(dcheng): Move this ctor back inline once the clang plugin is fixed.
DISABLE_CFI_PERF
inline DrawingDisplayItem::DrawingDisplayItem(const DisplayItemClient& client,
                                              Type type,
                                              sk_sp<const PaintRecord> record,
                                              bool known_to_be_opaque = false)
    : DisplayItem(client, type, sizeof(*this)),
      record_(record && record->size() ? std::move(record) : nullptr),
      known_to_be_opaque_(known_to_be_opaque) {
  DCHECK(IsDrawingType(type));
}

}  // namespace blink

#endif  // DrawingDisplayItem_h
