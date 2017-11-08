// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollDisplayItem_h
#define ScrollDisplayItem_h

#include "platform/geometry/IntSize.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class PLATFORM_EXPORT BeginScrollDisplayItem final
    : public PairedBeginDisplayItem {
 public:
  BeginScrollDisplayItem(const DisplayItemClient& client,
                         Type type,
                         const IntSize& current_offset)
      : PairedBeginDisplayItem(client, type, sizeof(*this)),
        current_offset_(current_offset) {
    DCHECK(IsScrollType(type));
  }

  void Replay(GraphicsContext&) const override;
  void AppendToWebDisplayItemList(const LayoutSize&,
                                  WebDisplayItemList*) const override;

  const IntSize& CurrentOffset() const { return current_offset_; }

 private:
#if DCHECK_IS_ON()
  void PropertiesAsJSON(JSONObject&) const final;
#endif
  bool Equals(const DisplayItem& other) const final {
    return DisplayItem::Equals(other) &&
           current_offset_ == static_cast<const BeginScrollDisplayItem&>(other)
                                  .current_offset_;
  }

  const IntSize current_offset_;
};

class PLATFORM_EXPORT EndScrollDisplayItem final : public PairedEndDisplayItem {
 public:
  EndScrollDisplayItem(const DisplayItemClient& client, Type type)
      : PairedEndDisplayItem(client, type, sizeof(*this)) {
    DCHECK(IsEndScrollType(type));
  }

  void Replay(GraphicsContext&) const override;
  void AppendToWebDisplayItemList(const LayoutSize&,
                                  WebDisplayItemList*) const override;

 private:
#if DCHECK_IS_ON()
  bool IsEndAndPairedWith(DisplayItem::Type other_type) const final {
    return DisplayItem::IsScrollType(other_type);
  }
#endif
};

}  // namespace blink

#endif  // ScrollDisplayItem_h
