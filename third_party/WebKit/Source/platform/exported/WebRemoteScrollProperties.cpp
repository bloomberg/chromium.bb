// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebRemoteScrollProperties.h"

#include "platform/wtf/SizeAssertions.h"

namespace blink {
namespace {
using Alignment = WebRemoteScrollProperties::Alignment;
using Type = WebRemoteScrollProperties::Type;
using Behavior = WebRemoteScrollProperties::Behavior;

// Make sure we keep the public enums in sync with the internal ones.

ASSERT_SIZE(Type, ScrollType)
STATIC_ASSERT_ENUM(Type::kUser, ScrollType::kUserScroll);
STATIC_ASSERT_ENUM(Type::kProgrammatic, ScrollType::kProgrammaticScroll);
STATIC_ASSERT_ENUM(Type::kClamping, ScrollType::kClampingScroll);
STATIC_ASSERT_ENUM(Type::kAnchoring, ScrollType::kAnchoringScroll);
STATIC_ASSERT_ENUM(Type::kSequenced, ScrollType::kSequencedScroll);

ASSERT_SIZE(Behavior, ScrollBehavior)
STATIC_ASSERT_ENUM(Behavior::kAuto, ScrollBehavior::kScrollBehaviorAuto);
STATIC_ASSERT_ENUM(Behavior::kInstant, ScrollBehavior::kScrollBehaviorInstant);
STATIC_ASSERT_ENUM(Behavior::kSmooth, ScrollBehavior::kScrollBehaviorSmooth);

Type FromScrollType(ScrollType type) {
  return static_cast<Type>(static_cast<int>(type));
}

ScrollType ToScrollType(Type type) {
  return static_cast<ScrollType>(static_cast<int>(type));
}

Behavior FromScrollBehavior(ScrollBehavior behavior) {
  return static_cast<Behavior>(static_cast<int>(behavior));
}

ScrollBehavior ToScrollBehavior(Behavior behavior) {
  return static_cast<ScrollBehavior>(static_cast<int>(behavior));
}

}  // namespace

WebRemoteScrollProperties::WebRemoteScrollProperties(
    Alignment alignment_x,
    Alignment alignment_y,
    ScrollType scroll_type,
    bool visible_in_visual_viewport,
    ScrollBehavior scroll_behavior,
    bool for_scroll_sequence)
    : align_x(alignment_x),
      align_y(alignment_y),
      type(FromScrollType(scroll_type)),
      make_visible_in_visual_viewport(visible_in_visual_viewport),
      behavior(FromScrollBehavior(scroll_behavior)),
      is_for_scroll_sequence(for_scroll_sequence) {}

ScrollType WebRemoteScrollProperties::GetScrollType() const {
  return ToScrollType(type);
}

ScrollBehavior WebRemoteScrollProperties::GetScrollBehavior() const {
  return ToScrollBehavior(behavior);
}

}  // namespace blink
