// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SCROLL_TIMELINE_OFFSET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SCROLL_TIMELINE_OFFSET_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_timeline_element_based_offset.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"

namespace blink {

class StringOrScrollTimelineElementBasedOffset;

// Represent a scroll timeline start/end offset which can be an
// scroll offset or an element based offset
class CORE_EXPORT ScrollTimelineOffset final
    : public GarbageCollected<ScrollTimelineOffset> {
 public:
  static ScrollTimelineOffset* Create(
      const StringOrScrollTimelineElementBasedOffset& offset,
      const CSSParserContext& context);

  // Create a default offset representing 'auto'.
  ScrollTimelineOffset() = default;
  // Create a scroll based offset.
  explicit ScrollTimelineOffset(CSSPrimitiveValue*);
  // Create an element based offset.
  explicit ScrollTimelineOffset(ScrollTimelineElementBasedOffset*);

  void Trace(blink::Visitor*);

  // Resolves this offset against the scroll source and in the given orientation
  // returning eqiuvalent concrete scroll offset.
  //
  //  - Length-based values are converted into concrete length values resolving
  //    percentages and zoom factor.
  //  - Element-based values are resolved to the equivalent scroll offset that
  //    satisfy the requirement.
  //  - Auto value simply returns the |detfault_offset|.
  //
  // max offset is expected to be the maximum scroll offset in the scroll
  // orientation.
  double ResolveOffset(Node* scroll_source,
                       ScrollOrientation,
                       double max_offset,
                       double default_offset);

  StringOrScrollTimelineElementBasedOffset
  ToStringOrScrollTimelineElementBasedOffset() const;

 private:
  // We either have an scroll or element based offset so at any time one of
  // these is null. If both are null, it represents the default value of
  // 'auto'.
  Member<CSSPrimitiveValue> length_based_offset_;
  Member<ScrollTimelineElementBasedOffset> element_based_offset_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SCROLL_TIMELINE_OFFSET_H_
