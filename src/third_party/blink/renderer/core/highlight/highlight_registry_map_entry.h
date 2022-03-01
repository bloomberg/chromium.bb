// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HIGHLIGHT_HIGHLIGHT_REGISTRY_MAP_ENTRY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HIGHLIGHT_HIGHLIGHT_REGISTRY_MAP_ENTRY_H_

#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

class Highlight;

struct HighlightRegistryMapEntry final
    : public GarbageCollected<HighlightRegistryMapEntry> {
  explicit HighlightRegistryMapEntry(const AtomicString& highlight_name)
      : highlight_name(highlight_name) {}
  HighlightRegistryMapEntry(const AtomicString& highlight_name,
                            Member<Highlight> highlight)
      : highlight(highlight), highlight_name(highlight_name) {}
  explicit HighlightRegistryMapEntry(const HighlightRegistryMapEntry* entry)
      : HighlightRegistryMapEntry(entry->highlight_name, entry->highlight) {}

  void Trace(blink::Visitor* visitor) const { visitor->Trace(highlight); }

  Member<Highlight> highlight = nullptr;
  AtomicString highlight_name = g_null_atom;
};

}  // namespace blink

namespace WTF {

template <>
struct DefaultHash<blink::Member<blink::HighlightRegistryMapEntry>> {
  struct Hash {
    STATIC_ONLY(Hash);

    // Note that GetHash and Equal only take into account the |highlight_name|
    // because |HighlightRegistryMapEntry| is used for storing map entries
    // inside a set (i.e. there can only be one map entry in the set with the
    // same key which is |highlight_name|).
    static inline unsigned GetHash(
        const blink::Member<blink::HighlightRegistryMapEntry>& key) {
      DCHECK(key);
      return AtomicStringHash::GetHash(key->highlight_name);
    }
    static inline bool Equal(
        const blink::Member<blink::HighlightRegistryMapEntry>& a,
        const blink::Member<blink::HighlightRegistryMapEntry>& b) {
      DCHECK(a && b);
      return AtomicStringHash::Equal(a->highlight_name, b->highlight_name);
    }

    static const bool safe_to_compare_to_empty_or_deleted = false;
  };
};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HIGHLIGHT_HIGHLIGHT_REGISTRY_MAP_ENTRY_H_
