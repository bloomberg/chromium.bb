// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_ATTRIBUTION_GROUP_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_ATTRIBUTION_GROUP_H_

#include "third_party/blink/public/common/input/web_input_event_attribution.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"

namespace blink {
namespace scheduler {

// A hashable wrapper around a WebInputEventAttribution that can be used to
// group pending events together.
struct AttributionGroup {
  explicit AttributionGroup(WebInputEventAttribution attribution)
      : attribution(attribution) {}

  explicit AttributionGroup(WTF::HashTableDeletedValueType)
      : is_deleted_value(true) {}

  AttributionGroup() = default;

  bool IsHashTableDeletedValue() const { return is_deleted_value; }

  bool operator==(const AttributionGroup& other) const {
    return attribution == other.attribution &&
           is_deleted_value == other.is_deleted_value;
  }

  WebInputEventAttribution attribution = {};
  bool is_deleted_value = false;
};

struct AttributionGroupHash {
  STATIC_ONLY(AttributionGroupHash);

  static unsigned GetHash(const AttributionGroup& group) {
    return static_cast<unsigned>(group.attribution.GetHash());
  }

  static bool Equal(const AttributionGroup& a, const AttributionGroup& b) {
    return a == b;
  }

  static const bool safe_to_compare_to_empty_or_deleted = true;
};

}  // namespace scheduler
}  // namespace blink

namespace WTF {

template <>
struct DefaultHash<blink::scheduler::AttributionGroup> {
  using Hash = blink::scheduler::AttributionGroupHash;
};

template <>
struct HashTraits<blink::scheduler::AttributionGroup>
    : SimpleClassHashTraits<blink::scheduler::AttributionGroup> {
  static const bool kEmptyValueIsZero = false;
};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_ATTRIBUTION_GROUP_H_
