// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Transform3DDisplayItem_h
#define Transform3DDisplayItem_h

#include "platform/geometry/FloatPoint3D.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "platform/transforms/TransformationMatrix.h"
#include "platform/wtf/Assertions.h"

namespace blink {

class PLATFORM_EXPORT BeginTransform3DDisplayItem final
    : public PairedBeginDisplayItem {
 public:
  BeginTransform3DDisplayItem(const DisplayItemClient& client,
                              Type type,
                              const TransformationMatrix& transform,
                              const FloatPoint3D& transform_origin)
      : PairedBeginDisplayItem(client, type, sizeof(*this)),
        transform_(transform),
        transform_origin_(transform_origin) {
    DCHECK(DisplayItem::IsTransform3DType(type));
  }

  void Replay(GraphicsContext&) const override;
  void AppendToWebDisplayItemList(const LayoutSize&,
                                  WebDisplayItemList*) const override;

  const TransformationMatrix& Transform() const { return transform_; }
  const FloatPoint3D& TransformOrigin() const { return transform_origin_; }

 private:
#if DCHECK_IS_ON()
  void PropertiesAsJSON(JSONObject&) const final;
#endif
  bool Equals(const DisplayItem& other) const final {
    return DisplayItem::Equals(other) &&
           transform_ == static_cast<const BeginTransform3DDisplayItem&>(other)
                             .transform_ &&
           transform_origin_ ==
               static_cast<const BeginTransform3DDisplayItem&>(other)
                   .transform_origin_;
  }

  const TransformationMatrix transform_;
  const FloatPoint3D transform_origin_;
};

class PLATFORM_EXPORT EndTransform3DDisplayItem final
    : public PairedEndDisplayItem {
 public:
  EndTransform3DDisplayItem(const DisplayItemClient& client, Type type)
      : PairedEndDisplayItem(client, type, sizeof(*this)) {
    DCHECK(DisplayItem::IsEndTransform3DType(type));
  }

  void Replay(GraphicsContext&) const override;
  void AppendToWebDisplayItemList(const LayoutSize&,
                                  WebDisplayItemList*) const override;

 private:
#if DCHECK_IS_ON()
  bool IsEndAndPairedWith(DisplayItem::Type other_type) const final {
    return DisplayItem::Transform3DTypeToEndTransform3DType(other_type) ==
           GetType();
  }
#endif
};

}  // namespace blink

#endif  // Transform3DDisplayItem_h
