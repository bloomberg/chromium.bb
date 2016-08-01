// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGConstraintSpace_h
#define NGConstraintSpace_h

#include "core/CoreExport.h"
#include "platform/LayoutUnit.h"
#include "wtf/DoublyLinkedList.h"

namespace blink {

class NGDerivedConstraintSpace;
class NGExclusion;
class NGFragment;
class NGLayoutOpportunityIterator;

enum NGExclusionType {
    ClearNone = 0,
    ClearFloatLeft = 1,
    ClearFloatRight = 2,
    ClearFragment = 4
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

class NGExclusion {
public:
    NGExclusion();
    ~NGExclusion() { }
};

class CORE_EXPORT NGConstraintSpace {
public:
    NGConstraintSpace(LayoutUnit inlineSize, LayoutUnit blockSize);
    ~NGConstraintSpace() { }

    void addExclusion(const NGExclusion, unsigned options = 0);
    void setOverflowSize(LayoutUnit inlineSize, LayoutUnit blockSize);
    void setFixedSize(bool inlineFixed, bool blockFixed);
    void setFragmentationType(NGFragmentationType blockFragmentationType);

    // Available space in each direction, -1 indicates infinite space.
    LayoutUnit inlineSize() const { return m_inlineSize; }
    LayoutUnit blockSize() const { return m_blockSize; }

    // The size threashold in the inline and block directions respectively that
    // triggers the presence of a scrollbar. If exceeded the current layout
    // should be aborted and invoked again with a constraint space modified to
    // reserve space for a scrollbar. -1 indicates no limit.
    LayoutUnit inlineOverflowSize() const { return m_inlineOverflowSize; }
    LayoutUnit blockOverflowSize() const { return m_blockOverflowSize; }

    // Some layout modes “stretch” their children to a fixed size (e.g. flex,
    // grid). These flags represented whether a layout needs to produce a
    // fragment that satisfies a fixed constraint in the inline and block
    // direction respectively.
    bool fixedInlineSize() const { return m_fixedInlineSize; }
    bool fixedBlockSize() const { return m_fixedBlockSize; }

    // If specified a layout should produce a Fragment which fragments at the
    // blockSize if possible.
    NGFragmentationType blockFragmentationType() const
    {
        return static_cast<NGFragmentationType>(m_blockFragmentationType);
    }

    DoublyLinkedList<const NGExclusion> exclusions(unsigned options = 0) const;

    NGLayoutOpportunityIterator layoutOpportunities(
        unsigned clear = ClearNone,
        NGExclusionFlowType avoid = ExcludeNone) const;

    // Modifies constraint space to account for a placed fragment. Depending on
    // the shape of the fragment this will either modify the inline or block
    // size, or add an exclusion.
    void subtract(const NGFragment);

private:
    LayoutUnit m_inlineSize;
    LayoutUnit m_blockSize;
    LayoutUnit m_inlineOverflowSize;
    LayoutUnit m_blockOverflowSize;

    unsigned m_fixedInlineSize : 1;
    unsigned m_fixedBlockSize : 1;
    unsigned m_blockFragmentationType : 2;

    DoublyLinkedList<const NGExclusion> m_exclusions;
};


class CORE_EXPORT NGLayoutOpportunityIterator final {
public:
    NGLayoutOpportunityIterator(const NGConstraintSpace* space,
        unsigned clear, NGExclusionFlowType avoid)
        : m_constraintSpace(space)
        , m_clear(clear)
        , m_avoid(avoid) { }
    ~NGLayoutOpportunityIterator() { }

    const NGDerivedConstraintSpace* next();

private:
    const NGConstraintSpace* m_constraintSpace;
    unsigned m_clear;
    NGExclusionFlowType m_avoid;
};


class CORE_EXPORT NGDerivedConstraintSpace final : NGConstraintSpace {
public:
    ~NGDerivedConstraintSpace();

    LayoutUnit inlineOffset() const;
    LayoutUnit blockOffset() const;

    // When creating a derived NGConstraintSpace for the next inline level
    // layout opportunity, inlineSize and blockSize of the NGConstraintSpace
    // may be different from its parent. These properties hold the inline and
    // block size of the original NGConstraintSpaces, allowing percentage
    // resolution to work correctly.
    LayoutUnit inlineSizeForPercentageResolution() const;
    LayoutUnit blockSizeForPercentageResolution() const;

private:
    NGDerivedConstraintSpace();

    LayoutUnit m_inlineOffset;
    LayoutUnit m_blockOffset;
    NGConstraintSpace* m_original;
};

} // namespace blink

#endif // NGConstraintSpace_h
