// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorElementId_h
#define CompositorElementId_h

#include "cc/trees/element_id.h"
#include "platform/PlatformExport.h"
#include "platform/wtf/HashFunctions.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/HashTraits.h"

namespace blink {

enum class CompositorSubElementId {
  kPrimary,
  kScroll,
  kViewport,
  kLinkHighlight
};

using CompositorElementId = cc::ElementId;

CompositorElementId PLATFORM_EXPORT
CreateCompositorElementId(int dom_node_id, CompositorSubElementId);

// Note cc::ElementId has a hash function already implemented via
// ElementIdHash::operator(). However for consistency's sake we choose to use
// Blink's hash functions with Blink specific data structures.
struct CompositorElementIdHash {
  static unsigned GetHash(const CompositorElementId& p) {
    return WTF::HashInts(p.primaryId, p.secondaryId);
  }
  static bool Equal(const CompositorElementId& a,
                    const CompositorElementId& b) {
    return a.primaryId == b.primaryId && a.secondaryId == b.secondaryId;
  }
  static const bool safe_to_compare_to_empty_or_deleted = true;
};

struct CompositorElementIdHashTraits
    : WTF::GenericHashTraits<CompositorElementId> {
  static CompositorElementId EmptyValue() { return CompositorElementId(); }
  static void ConstructDeletedValue(CompositorElementId& slot, bool) {
    slot = CompositorElementId(-1, -1);
  }
  static bool IsDeletedValue(CompositorElementId value) {
    return value == CompositorElementId(-1, -1);
  }
};

using CompositorElementIdSet = HashSet<CompositorElementId,
                                       CompositorElementIdHash,
                                       CompositorElementIdHashTraits>;

}  // namespace blink

#endif  // CompositorElementId_h
