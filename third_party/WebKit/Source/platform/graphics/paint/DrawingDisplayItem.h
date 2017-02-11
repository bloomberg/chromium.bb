// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DrawingDisplayItem_h
#define DrawingDisplayItem_h

#include "base/compiler_specific.h"
#include "platform/PlatformExport.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "platform/graphics/paint/PaintRecord.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

class PLATFORM_EXPORT DrawingDisplayItem final : public DisplayItem {
 public:
  DISABLE_CFI_PERF
  DrawingDisplayItem(const DisplayItemClient& client,
                     Type type,
                     sk_sp<const PaintRecord> record,
                     bool knownToBeOpaque = false)
      : DisplayItem(client, type, sizeof(*this)),
        m_record(record && record->approximateOpCount() ? std::move(record)
                                                        : nullptr),
        m_knownToBeOpaque(knownToBeOpaque) {
    DCHECK(isDrawingType(type));
  }

  void replay(GraphicsContext&) const override;
  void appendToWebDisplayItemList(const IntRect&,
                                  WebDisplayItemList*) const override;
  bool drawsContent() const override;

  const PaintRecord* GetPaintRecord() const { return m_record.get(); }

  bool knownToBeOpaque() const {
    DCHECK(RuntimeEnabledFeatures::slimmingPaintV2Enabled());
    return m_knownToBeOpaque;
  }

  void analyzeForGpuRasterization(SkPictureGpuAnalyzer&) const override;

 private:
#ifndef NDEBUG
  void dumpPropertiesAsDebugString(WTF::StringBuilder&) const override;
#endif
  bool equals(const DisplayItem& other) const final;

  sk_sp<const PaintRecord> m_record;

  // True if there are no transparent areas. Only used for SlimmingPaintV2.
  const bool m_knownToBeOpaque;
};

}  // namespace blink

#endif  // DrawingDisplayItem_h
