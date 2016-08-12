// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGConstraintSpace_h
#define NGConstraintSpace_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_units.h"
#include "wtf/DoublyLinkedList.h"

namespace blink {

class NGConstraintSpace;
class NGDerivedConstraintSpace;
class NGExclusion;
class NGFragment;
class NGLayoutOpportunityIterator;
class LayoutBox;

enum NGExclusionType {
  NGClearNone = 0,
  NGClearFloatLeft = 1,
  NGClearFloatRight = 2,
  NGClearFragment = 4
};

enum NGFragmentationType {
  FragmentNone,
  FragmentPage,
  FragmentColumn,
  FragmentRegion
};

enum NGExclusionFlowType {
  ExcludeNone,
  ExcludeInlineFlow,
  ExcludeInlineStart,
  ExcludeInlineEnd,
  ExcludeInlineBoth
};

enum NGWritingMode {
  HorizontalTopBottom = 0,
  VerticalRightLeft = 1,
  VerticalLeftRight = 2,
  SidewaysRightLeft = 3,
  SidewaysLeftRight = 4
};

enum NGDirection { LeftToRight = 0, RightToLeft = 1 };

class NGExclusion {
 public:
  NGExclusion();
  ~NGExclusion() {}
};

class CORE_EXPORT NGConstraintSpace {
 public:
  NGConstraintSpace(NGLogicalSize container_size);
  virtual ~NGConstraintSpace() {}

  // Constructs Layout NG constraint space from legacy layout object.
  static NGConstraintSpace fromLayoutObject(const LayoutBox&);

  void addExclusion(const NGExclusion, unsigned options = 0);
  void setOverflowTriggersScrollbar(bool inlineTriggers, bool blockTriggers);
  void setFixedSize(bool inlineFixed, bool blockFixed);
  void setFragmentationType(NGFragmentationType);

  // Size of the container. Used for the following three cases:
  // 1) Percentage resolution.
  // 2) Resolving absolute positions of children.
  // 3) Defining the threashold that triggers the presence of a scrollbar. Only
  //    applies if the corresponding scrollbarTrigger flag has been set for the
  //    direction.
  NGLogicalSize ContainerSize() const { return container_size_; }

  // Returns the effective size of the constraint space. Defaults to
  // ContainerSize() for the root constraint space but derived constraint spaces
  // overrides it to return the size of the layout opportunity.
  virtual NGLogicalSize Size() const { return ContainerSize(); }

  // Whether exceeding the containerSize triggers the presence of a scrollbar
  // for the indicated direction.
  // If exceeded the current layout should be aborted and invoked again with a
  // constraint space modified to reserve space for a scrollbar.
  bool inlineTriggersScrollbar() const { return m_inlineTriggersScrollbar; }
  bool blockTriggersScrollbar() const { return m_blockTriggersScrollbar; }

  // Some layout modes “stretch” their children to a fixed size (e.g. flex,
  // grid). These flags represented whether a layout needs to produce a
  // fragment that satisfies a fixed constraint in the inline and block
  // direction respectively.
  bool fixedInlineSize() const { return m_fixedInlineSize; }
  bool fixedBlockSize() const { return m_fixedBlockSize; }

  // If specified a layout should produce a Fragment which fragments at the
  // blockSize if possible.
  NGFragmentationType blockFragmentationType() const {
    return static_cast<NGFragmentationType>(m_blockFragmentationType);
  }

  DoublyLinkedList<const NGExclusion> exclusions(unsigned options = 0) const;

  NGLayoutOpportunityIterator layoutOpportunities(
      unsigned clear = NGClearNone,
      NGExclusionFlowType avoid = ExcludeNone) const;

  // Modifies constraint space to account for a placed fragment. Depending on
  // the shape of the fragment this will either modify the inline or block
  // size, or add an exclusion.
  void subtract(const NGFragment);

 private:
  NGLogicalSize container_size_;

  unsigned m_fixedInlineSize : 1;
  unsigned m_fixedBlockSize : 1;
  unsigned m_inlineTriggersScrollbar : 1;
  unsigned m_blockTriggersScrollbar : 1;
  unsigned m_blockFragmentationType : 2;

  DoublyLinkedList<const NGExclusion> m_exclusions;
};

class CORE_EXPORT NGLayoutOpportunityIterator final {
 public:
  NGLayoutOpportunityIterator(const NGConstraintSpace* space,
                              unsigned clear,
                              NGExclusionFlowType avoid)
      : m_constraintSpace(space), m_clear(clear), m_avoid(avoid) {}
  ~NGLayoutOpportunityIterator() {}

  const NGDerivedConstraintSpace* next();

 private:
  const NGConstraintSpace* m_constraintSpace;
  unsigned m_clear;
  NGExclusionFlowType m_avoid;
};

}  // namespace blink

#endif  // NGConstraintSpace_h
