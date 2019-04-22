// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_DISPLAY_LOCK_UTILITIES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_DISPLAY_LOCK_UTILITIES_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"

namespace blink {

// Static utility class for display-locking related helpers.
class CORE_EXPORT DisplayLockUtilities {
  STATIC_ONLY(DisplayLockUtilities);

 public:
  // This class forces updates on display locks from the given node up the
  // ancestor chain until the root.
  class ScopedChainForcedUpdate {
    STACK_ALLOCATED();
    DISALLOW_COPY_AND_ASSIGN(ScopedChainForcedUpdate);

   public:
    explicit ScopedChainForcedUpdate(const Node* node);
    ~ScopedChainForcedUpdate() = default;

   private:
    Vector<DisplayLockContext::ScopedForcedUpdate> scoped_update_forced_list_;
  };
  // Activates all the nodes within a find-in-page match |range|.
  // Returns true if at least one node gets activated.
  // See: http://bit.ly/2RXULVi, "beforeactivate Event" part.
  static bool ActivateFindInPageMatchRangeIfNeeded(
      const EphemeralRangeInFlatTree& range);

  // Returns activatable-locked inclusive ancestors of |element|.
  // Note that this function will have failing DCHECKs if |element| is inside a
  // non-activatable locked subtree (e.g. at least one ancestor is not
  // activatable-locked).
  static const HeapVector<Member<Element>> ActivatableLockedInclusiveAncestors(
      Element& element);

  // Returns the nearest inclusive ancestor of |node| that is display locked.
  static Element* NearestLockedInclusiveAncestor(const Node& node);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_DISPLAY_LOCK_UTILITIES_H_
