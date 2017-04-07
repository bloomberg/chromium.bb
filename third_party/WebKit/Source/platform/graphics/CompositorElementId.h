// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorElementId_h
#define CompositorElementId_h

#include "cc/trees/element_id.h"
#include "platform/PlatformExport.h"
#include "wtf/HashFunctions.h"
#include "wtf/HashSet.h"
#include "wtf/HashTraits.h"

namespace blink {

enum class CompositorSubElementId { Primary, Scroll, Viewport, LinkHighlight };

using CompositorElementId = cc::ElementId;

CompositorElementId PLATFORM_EXPORT
createCompositorElementId(int domNodeId, CompositorSubElementId);

// Note cc::ElementId has a hash function already implemented via
// ElementIdHash::operator(). However for consistency's sake we choose to use
// Blink's hash functions with Blink specific data structures.
struct CompositorElementIdHash {
  static unsigned hash(const CompositorElementId& p) {
    return WTF::hashInts(p.primaryId, p.secondaryId);
  }
  static bool equal(const CompositorElementId& a,
                    const CompositorElementId& b) {
    return a.primaryId == b.primaryId && a.secondaryId == b.secondaryId;
  }
  static const bool safeToCompareToEmptyOrDeleted = true;
};

struct CompositorElementIdHashTraits
    : WTF::GenericHashTraits<CompositorElementId> {
  static CompositorElementId emptyValue() { return CompositorElementId(); }
  static void constructDeletedValue(CompositorElementId& slot, bool) {
    slot = CompositorElementId(-1, -1);
  }
  static bool isDeletedValue(CompositorElementId value) {
    return value == CompositorElementId(-1, -1);
  }
};

using CompositorElementIdSet = HashSet<CompositorElementId,
                                       CompositorElementIdHash,
                                       CompositorElementIdHashTraits>;

}  // namespace blink

#endif  // CompositorElementId_h
