// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebScrollIntoViewParams_h
#define WebScrollIntoViewParams_h

#include "public/platform/WebCommon.h"

#if INSIDE_BLINK
#include "platform/scroll/ScrollAlignment.h"
#include "platform/scroll/ScrollTypes.h"
#endif

namespace blink {
// This struct contains the required information for propagating some stages
// of scrolling process to cross process frames. Specifically for various types
// of programmatic scrolling such as scrolling an element into view, recursive
// scrolling across multiple processes is accommodated through passing the state
// using this struct to the browser and then to the target (parent) process.
struct WebScrollIntoViewParams {
  // Public variant of ScrollAlignmentBehavior.
  enum AlignmentBehavior {
    kNoScroll = 0,
    kCenter,
    kTop,
    kBottom,
    kLeft,
    kRight,
    kClosestEdge,
    kLastAlignmentBehavior = kClosestEdge
  };

  // A public wrapper around ScrollAlignment. The default value matches
  // kAlignCenterIfNeeded.
  struct Alignment {
    Alignment() = default;
#if INSIDE_BLINK
    Alignment(const ScrollAlignment&);
#endif
    AlignmentBehavior rect_visible = kNoScroll;
    AlignmentBehavior rect_hidden = kCenter;
    AlignmentBehavior rect_partial = kClosestEdge;
  };

  // Scroll type set through 'scroll-type' CSS property.
  enum Type {
    kUser = 0,
    kProgrammatic,
    kClamping,
    kCompositor,
    kAnchoring,
    kSequenced,
    kLastType = kSequenced,
  };

  // The scroll behavior set through 'scroll-behavior' CSS property.
  enum Behavior {
    kAuto = 0,
    kInstant,
    kSmooth,
    kLastBehavior = kSmooth,
  };

  Alignment align_x;
  Alignment align_y;
  Type type = kProgrammatic;
  bool make_visible_in_visual_viewport = true;
  Behavior behavior = kAuto;
  bool is_for_scroll_sequence = false;

  WebScrollIntoViewParams() = default;
#if INSIDE_BLINK
  BLINK_PLATFORM_EXPORT WebScrollIntoViewParams(
      ScrollAlignment,
      ScrollAlignment,
      ScrollType scroll_type = kProgrammaticScroll,
      bool make_visible_in_visual_viewport = true,
      ScrollBehavior scroll_behavior = kScrollBehaviorAuto,
      bool is_for_scroll_sequence = false);

  BLINK_PLATFORM_EXPORT ScrollAlignment GetScrollAlignmentX() const;

  BLINK_PLATFORM_EXPORT ScrollAlignment GetScrollAlignmentY() const;

  BLINK_PLATFORM_EXPORT ScrollType GetScrollType() const;

  BLINK_PLATFORM_EXPORT ScrollBehavior GetScrollBehavior() const;
#endif
};

}  // namespace blink

#endif  // WebScrollIntoViewParams_h
