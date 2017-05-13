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

const int kCompositorNamespaceBitCount = 3;

enum class CompositorElementIdNamespace {
  kPrimary,
  kScroll,
  kViewport,
  kLinkHighlight,
  kScrollbar,
  // A sentinel to indicate the maximum representable namespace id
  // (the maximum is one less than this value).
  kMaxRepresentableNamespaceId = 1 << kCompositorNamespaceBitCount
};

using CompositorElementId = cc::ElementId;
using DOMNodeId = uint64_t;
using ScrollbarId = uint64_t;

CompositorElementId PLATFORM_EXPORT
    CompositorElementIdFromDOMNodeId(DOMNodeId, CompositorElementIdNamespace);

CompositorElementId PLATFORM_EXPORT
    CompositorElementIdFromScrollbarId(ScrollbarId,
                                       CompositorElementIdNamespace);

// Note cc::ElementId has a hash function already implemented via
// ElementIdHash::operator(). However for consistency's sake we choose to use
// Blink's hash functions with Blink specific data structures.
struct CompositorElementIdHash {
  static unsigned GetHash(const CompositorElementId& key) {
    return WTF::HashInt(static_cast<cc::ElementIdType>(key.id_));
  }
  static bool Equal(const CompositorElementId& a,
                    const CompositorElementId& b) {
    return a.id_ == b.id_;
  }
  static const bool safe_to_compare_to_empty_or_deleted = true;
};

uint64_t PLATFORM_EXPORT IdFromCompositorElementId(CompositorElementId);
CompositorElementIdNamespace PLATFORM_EXPORT
    NamespaceFromCompositorElementId(CompositorElementId);

struct CompositorElementIdHashTraits
    : WTF::GenericHashTraits<CompositorElementId> {
  static CompositorElementId EmptyValue() { return CompositorElementId(1); }
  static void ConstructDeletedValue(CompositorElementId& slot, bool) {
    slot = CompositorElementId();
  }
  static bool IsDeletedValue(CompositorElementId value) {
    return value == CompositorElementId();
  }
};

using CompositorElementIdSet = HashSet<CompositorElementId,
                                       CompositorElementIdHash,
                                       CompositorElementIdHashTraits>;

}  // namespace blink

#endif  // CompositorElementId_h
