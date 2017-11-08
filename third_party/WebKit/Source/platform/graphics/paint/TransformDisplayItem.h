// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TransformDisplayItem_h
#define TransformDisplayItem_h

#include "platform/graphics/paint/DisplayItem.h"
#include "platform/transforms/AffineTransform.h"

namespace blink {

class PLATFORM_EXPORT BeginTransformDisplayItem final
    : public PairedBeginDisplayItem {
 public:
  BeginTransformDisplayItem(const DisplayItemClient& client,
                            const AffineTransform& transform)
      : PairedBeginDisplayItem(client, kBeginTransform, sizeof(*this)),
        transform_(transform) {}

  void Replay(GraphicsContext&) const override;
  void AppendToWebDisplayItemList(const LayoutSize&,
                                  WebDisplayItemList*) const override;

  const AffineTransform& Transform() const { return transform_; }

 private:
#if DCHECK_IS_ON()
  void PropertiesAsJSON(JSONObject&) const final;
#endif
  bool Equals(const DisplayItem& other) const final {
    return DisplayItem::Equals(other) &&
           transform_ ==
               static_cast<const BeginTransformDisplayItem&>(other).transform_;
  }

  const AffineTransform transform_;
};

class PLATFORM_EXPORT EndTransformDisplayItem final
    : public PairedEndDisplayItem {
 public:
  EndTransformDisplayItem(const DisplayItemClient& client)
      : PairedEndDisplayItem(client, kEndTransform, sizeof(*this)) {}

  void Replay(GraphicsContext&) const override;
  void AppendToWebDisplayItemList(const LayoutSize&,
                                  WebDisplayItemList*) const override;

 private:
#if DCHECK_IS_ON()
  bool IsEndAndPairedWith(DisplayItem::Type other_type) const final {
    return other_type == kBeginTransform;
  }
#endif
};

}  // namespace blink

#endif  // TransformDisplayItem_h
