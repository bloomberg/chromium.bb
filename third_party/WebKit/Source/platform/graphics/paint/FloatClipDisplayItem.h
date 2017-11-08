// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FloatClipDisplayItem_h
#define FloatClipDisplayItem_h

#include "platform/PlatformExport.h"
#include "platform/geometry/FloatRect.h"
#include "platform/graphics/paint/DisplayItem.h"

namespace blink {

class PLATFORM_EXPORT FloatClipDisplayItem final
    : public PairedBeginDisplayItem {
 public:
  FloatClipDisplayItem(const DisplayItemClient& client,
                       Type type,
                       const FloatRect& clip_rect)
      : PairedBeginDisplayItem(client, type, sizeof(*this)),
        clip_rect_(clip_rect) {
    DCHECK(IsFloatClipType(type));
  }

  void Replay(GraphicsContext&) const override;
  void AppendToWebDisplayItemList(const LayoutSize&,
                                  WebDisplayItemList*) const override;

 private:
#if DCHECK_IS_ON()
  void PropertiesAsJSON(JSONObject&) const override;
#endif
  bool Equals(const DisplayItem& other) const final {
    return DisplayItem::Equals(other) &&
           clip_rect_ ==
               static_cast<const FloatClipDisplayItem&>(other).clip_rect_;
  }

  const FloatRect clip_rect_;
};

class PLATFORM_EXPORT EndFloatClipDisplayItem final
    : public PairedEndDisplayItem {
 public:
  EndFloatClipDisplayItem(const DisplayItemClient& client, Type type)
      : PairedEndDisplayItem(client, type, sizeof(*this)) {
    DCHECK(IsEndFloatClipType(type));
  }

  void Replay(GraphicsContext&) const override;
  void AppendToWebDisplayItemList(const LayoutSize&,
                                  WebDisplayItemList*) const override;

 private:
#if DCHECK_IS_ON()
  bool IsEndAndPairedWith(DisplayItem::Type other_type) const final {
    return DisplayItem::IsFloatClipType(other_type);
  }
#endif
};

}  // namespace blink

#endif  // FloatClipDisplayItem_h
