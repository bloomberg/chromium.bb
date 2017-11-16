// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FilterDisplayItem_h
#define FilterDisplayItem_h

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "platform/geometry/FloatRect.h"
#include "platform/graphics/CompositorFilterOperations.h"
#include "platform/graphics/paint/DisplayItem.h"

namespace blink {

class PLATFORM_EXPORT BeginFilterDisplayItem final
    : public PairedBeginDisplayItem {
 public:
  BeginFilterDisplayItem(const DisplayItemClient& client,
                         sk_sp<SkImageFilter> image_filter,
                         const FloatRect& bounds,
                         const FloatPoint& origin,
                         CompositorFilterOperations filter_operations)
      : PairedBeginDisplayItem(client, kBeginFilter, sizeof(*this)),
        image_filter_(std::move(image_filter)),
        compositor_filter_operations_(std::move(filter_operations)),
        bounds_(bounds),
        origin_(origin) {}

  void Replay(GraphicsContext&) const override;
  void AppendToWebDisplayItemList(const LayoutSize&,
                                  WebDisplayItemList*) const override;
  bool DrawsContent() const override;

 private:
#if DCHECK_IS_ON()
  void PropertiesAsJSON(JSONObject&) const override;
#endif
  bool Equals(const DisplayItem& other) const final {
    if (!DisplayItem::Equals(other))
      return false;
    const auto& other_item = static_cast<const BeginFilterDisplayItem&>(other);
    // Ignores changes of reference filters because SkImageFilter doesn't have
    // an equality operator.
    return bounds_ == other_item.bounds_ && origin_ == other_item.origin_ &&
           compositor_filter_operations_.EqualsIgnoringReferenceFilters(
               other_item.compositor_filter_operations_);
  }

  // FIXME: m_imageFilter should be replaced with m_webFilterOperations when
  // copying data to the compositor.
  sk_sp<SkImageFilter> image_filter_;
  CompositorFilterOperations compositor_filter_operations_;
  const FloatRect bounds_;
  const FloatPoint origin_;
};

class PLATFORM_EXPORT EndFilterDisplayItem final : public PairedEndDisplayItem {
 public:
  EndFilterDisplayItem(const DisplayItemClient& client)
      : PairedEndDisplayItem(client, kEndFilter, sizeof(*this)) {}

  void Replay(GraphicsContext&) const override;
  void AppendToWebDisplayItemList(const LayoutSize&,
                                  WebDisplayItemList*) const override;

 private:
#if DCHECK_IS_ON()
  bool IsEndAndPairedWith(DisplayItem::Type other_type) const final {
    return other_type == kBeginFilter;
  }
#endif
};

}  // namespace blink

#endif  // FilterDisplayItem_h
