// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LiveNodeListRegistry_h
#define LiveNodeListRegistry_h

#include <algorithm>

#include "core/CoreExport.h"
#include "platform/heap/Heap.h"
#include "platform/heap/HeapAllocator.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/Vector.h"

namespace blink {

class LiveNodeListBase;
enum NodeListInvalidationType : int;

// Weakly holds (node list, invalidation type) pairs, and allows efficient
// queries of whether nodes matching particular invalidation types are present.
// Entries are automatically removed when a node list is collected by the GC.
//
// Adding elements and querying are both efficient, and the data structure is
// reasonably compact (and attempts to remain so). Removal (especially manual
// removal) is somewhat expensive, but expected to be infrequent.
//
// It is invalid to add a (list, type) pair that is already present, or to
// remove one which is not.
class CORE_EXPORT LiveNodeListRegistry {
  DISALLOW_NEW();
  WTF_MAKE_NONCOPYABLE(LiveNodeListRegistry);

  using Entry = std::pair<UntracedMember<const LiveNodeListBase>, unsigned>;

 public:
  LiveNodeListRegistry() {}
  void Add(const LiveNodeListBase*, NodeListInvalidationType);
  void Remove(const LiveNodeListBase*, NodeListInvalidationType);

  bool IsEmpty() const { return mask_ == 0; }

  bool ContainsInvalidationType(NodeListInvalidationType type) const {
    return mask_ & MaskForInvalidationType(type);
  }

  void Trace(blink::Visitor*);

 private:
  static inline unsigned MaskForInvalidationType(
      NodeListInvalidationType type) {
    return 1u << type;
  }

  void RecomputeMask();

  // Removes any entries corresponding to node lists which have been collected
  // by the GC, and updates the mask accordingly.
  void ClearWeakMembers(Visitor*);

  Vector<Entry> data_;
  unsigned mask_ = 0;
};

}  // namespace blink

#endif  // LiveNodeListRegistry_h
