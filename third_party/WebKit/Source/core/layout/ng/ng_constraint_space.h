// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGConstraintSpace_h
#define NGConstraintSpace_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_physical_constraint_space.h"
#include "core/layout/ng/ng_writing_mode.h"
#include "platform/heap/Handle.h"

namespace blink {

class LayoutBox;
class NGDerivedConstraintSpace;
class NGFragment;
class NGLayoutOpportunityIterator;

// The NGConstraintSpace represents a set of constraints and available space
// which a layout algorithm may produce a NGFragment within. It is a view on
// top of a NGPhysicalConstraintSpace and provides accessor methods in the
// logical coordinate system defined by the writing mode given.
class CORE_EXPORT NGConstraintSpace
    : public GarbageCollected<NGConstraintSpace> {
 public:
  // Constructs a constraint space with a new backing NGPhysicalConstraintSpace.
  NGConstraintSpace(NGWritingMode writing_mode, NGLogicalSize container_size);

  // Constructs a constraint space based on an existing backing
  // NGPhysicalConstraintSpace.
  NGConstraintSpace(NGWritingMode writing_mode, NGPhysicalConstraintSpace*);

  // Constructs a constraint space with a different NGWritingMode.
  NGConstraintSpace(NGWritingMode writing_mode,
                    const NGConstraintSpace* constraint_space)
      : physical_space_(constraint_space->PhysicalSpace()),
        writing_mode_(writing_mode) {}

  // TODO: This should either be removed or also take an offset (if we merge
  // this with NGDerivedConstraintSpace).
  NGConstraintSpace(const NGConstraintSpace& other,
                    NGLogicalSize container_size);

  NGPhysicalConstraintSpace* PhysicalSpace() const { return physical_space_; }

  NGWritingMode WritingMode() const {
    return static_cast<NGWritingMode>(writing_mode_);
  }

  // Size of the container. Used for the following three cases:
  // 1) Percentage resolution.
  // 2) Resolving absolute positions of children.
  // 3) Defining the threashold that triggers the presence of a scrollbar. Only
  //    applies if the corresponding scrollbarTrigger flag has been set for the
  //    direction.
  NGLogicalSize ContainerSize() const;

  // Returns the effective size of the constraint space. Defaults to
  // ContainerSize() for the root constraint space but derived constraint spaces
  // overrides it to return the size of the layout opportunity.
  virtual NGLogicalSize Size() const { return ContainerSize(); }

  // Whether exceeding the containerSize triggers the presence of a scrollbar
  // for the indicated direction.
  // If exceeded the current layout should be aborted and invoked again with a
  // constraint space modified to reserve space for a scrollbar.
  bool InlineTriggersScrollbar() const;
  bool BlockTriggersScrollbar() const;

  // Some layout modes “stretch” their children to a fixed size (e.g. flex,
  // grid). These flags represented whether a layout needs to produce a
  // fragment that satisfies a fixed constraint in the inline and block
  // direction respectively.
  bool FixedInlineSize() const;
  bool FixedBlockSize() const;

  // If specified a layout should produce a Fragment which fragments at the
  // blockSize if possible.
  NGFragmentationType BlockFragmentationType() const;

  // Modifies constraint space to account for a placed fragment. Depending on
  // the shape of the fragment this will either modify the inline or block
  // size, or add an exclusion.
  void Subtract(const NGFragment*);

  NGLayoutOpportunityIterator LayoutOpportunities(
      unsigned clear = NGClearNone,
      bool for_inline_or_bfc = false);

  DEFINE_INLINE_VIRTUAL_TRACE() { visitor->trace(physical_space_); }

 protected:
  // The setters for the NGConstraintSpace should only be used when constructing
  // via the NGDerivedConstraintSpace.
  void SetContainerSize(NGLogicalSize);
  void SetOverflowTriggersScrollbar(bool inlineTriggers, bool blockTriggers);
  void SetFixedSize(bool inlineFixed, bool blockFixed);
  void SetFragmentationType(NGFragmentationType);

 private:
  Member<NGPhysicalConstraintSpace> physical_space_;
  unsigned writing_mode_ : 3;
};

class CORE_EXPORT NGLayoutOpportunityIterator final {
 public:
  NGLayoutOpportunityIterator(NGConstraintSpace* space,
                              unsigned clear,
                              bool for_inline_or_bfc)
      : constraint_space_(space),
        clear_(clear),
        for_inline_or_bfc_(for_inline_or_bfc) {}
  ~NGLayoutOpportunityIterator() {}

  NGConstraintSpace* Next();

 private:
  Persistent<NGConstraintSpace> constraint_space_;
  unsigned clear_;
  bool for_inline_or_bfc_;
};

}  // namespace blink

#endif  // NGConstraintSpace_h
