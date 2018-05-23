// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_INVALIDATION_STYLE_INVALIDATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_INVALIDATION_STYLE_INVALIDATOR_H_

#include <memory>

#include "base/macros.h"
#include "third_party/blink/renderer/core/css/invalidation/pending_invalidations.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/noncopyable.h"

namespace blink {

class ContainerNode;
class Document;
class Element;
class HTMLSlotElement;
class InvalidationSet;

// Applies deferred style invalidation for DOM subtrees.
//
// See https://goo.gl/3ane6s and https://goo.gl/z0Z9gn
// for more detailed overviews of style invalidation.
class CORE_EXPORT StyleInvalidator {
  STACK_ALLOCATED();

 public:
  StyleInvalidator(PendingInvalidationMap&);

  ~StyleInvalidator();
  void Invalidate(Document&);

 private:
  class SiblingData;

  void PushInvalidationSetsForContainerNode(ContainerNode&, SiblingData&);
  void PushInvalidationSet(const InvalidationSet&);
  bool WholeSubtreeInvalid() const { return whole_subtree_invalid_; }

  void Invalidate(Element&, SiblingData&);
  void InvalidateShadowRootChildren(Element&);
  void InvalidateChildren(Element&);
  void InvalidateSlotDistributedElements(HTMLSlotElement&) const;
  bool CheckInvalidationSetsAgainstElement(Element&, SiblingData&);

  bool MatchesCurrentInvalidationSets(Element&) const;
  bool MatchesCurrentInvalidationSetsAsSlotted(Element&) const;

  bool HasInvalidationSets() const {
    return !WholeSubtreeInvalid() && invalidation_sets_.size();
  }

  void SetWholeSubtreeInvalid() { whole_subtree_invalid_ = true; }

  bool TreeBoundaryCrossing() const { return tree_boundary_crossing_; }
  bool InsertionPointCrossing() const { return insertion_point_crossing_; }
  bool InvalidatesSlotted() const { return invalidates_slotted_; }

  PendingInvalidationMap& pending_invalidation_map_;
  using DescendantInvalidationSets = Vector<const InvalidationSet*, 16>;
  DescendantInvalidationSets invalidation_sets_;
  bool invalidate_custom_pseudo_ = false;
  bool whole_subtree_invalid_ = false;
  bool tree_boundary_crossing_ = false;
  bool insertion_point_crossing_ = false;
  bool invalidates_slotted_ = false;

  class SiblingData {
    STACK_ALLOCATED();

   public:
    SiblingData() : element_index_(0) {}

    void PushInvalidationSet(const SiblingInvalidationSet&);
    bool MatchCurrentInvalidationSets(Element&, StyleInvalidator&);

    bool IsEmpty() const { return invalidation_entries_.IsEmpty(); }
    void Advance() { element_index_++; }

   private:
    struct Entry {
      DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
      Entry(const SiblingInvalidationSet* invalidation_set,
            unsigned invalidation_limit)
          : invalidation_set_(invalidation_set),
            invalidation_limit_(invalidation_limit) {}

      const SiblingInvalidationSet* invalidation_set_;
      unsigned invalidation_limit_;
    };

    Vector<Entry, 16> invalidation_entries_;
    unsigned element_index_;
  };

  // Saves the state of a StyleInvalidator and automatically restores it when
  // this object is destroyed.
  class RecursionCheckpoint {
    STACK_ALLOCATED();

   public:
    RecursionCheckpoint(StyleInvalidator* invalidator)
        : prev_invalidation_sets_size_(invalidator->invalidation_sets_.size()),
          prev_invalidate_custom_pseudo_(
              invalidator->invalidate_custom_pseudo_),
          prev_whole_subtree_invalid_(invalidator->whole_subtree_invalid_),
          tree_boundary_crossing_(invalidator->tree_boundary_crossing_),
          insertion_point_crossing_(invalidator->insertion_point_crossing_),
          invalidates_slotted_(invalidator->invalidates_slotted_),
          invalidator_(invalidator) {}
    ~RecursionCheckpoint() {
      invalidator_->invalidation_sets_.Shrink(prev_invalidation_sets_size_);
      invalidator_->invalidate_custom_pseudo_ = prev_invalidate_custom_pseudo_;
      invalidator_->whole_subtree_invalid_ = prev_whole_subtree_invalid_;
      invalidator_->tree_boundary_crossing_ = tree_boundary_crossing_;
      invalidator_->insertion_point_crossing_ = insertion_point_crossing_;
      invalidator_->invalidates_slotted_ = invalidates_slotted_;
    }

   private:
    int prev_invalidation_sets_size_;
    bool prev_invalidate_custom_pseudo_;
    bool prev_whole_subtree_invalid_;
    bool tree_boundary_crossing_;
    bool insertion_point_crossing_;
    bool invalidates_slotted_;
    StyleInvalidator* invalidator_;
  };
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_INVALIDATION_STYLE_INVALIDATOR_H_
