// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FragmentainerIterator_h
#define FragmentainerIterator_h

#include "core/layout/LayoutFlowThread.h"

namespace blink {

class MultiColumnFragmentainerGroup;

// Used to find the fragmentainers that intersect with a given portion of the flow thread. The
// portion typically corresponds to the bounds of some descendant layout object. The iterator walks
// in block direction order.
class FragmentainerIterator {
public:
    FragmentainerIterator(const LayoutFlowThread&, const LayoutRect& physicalBoundingBoxInFlowThread, const LayoutRect& clipRectInMulticolContainer);

    // Advance to the next fragmentainer. Not allowed to call this if atEnd() is true.
    void advance();

    // Return true if we have walked through all relevant fragmentainers.
    bool atEnd() const { return !m_currentColumnSet; }

    // The physical translation to apply to shift the box when converting from flowthread to visual
    // coordinates.
    LayoutSize paginationOffset() const { DCHECK(!atEnd()); return m_paginationOffset; }

    // Return the physical clip rectangle of the current fragmentainer, relative to the flow thread.
    LayoutRect clipRectInFlowThread() const { DCHECK(!atEnd()); return m_clipRectInFlowThread; }

private:
    const LayoutFlowThread& m_flowThread;
    const LayoutRect m_clipRectInMulticolContainer;

    const LayoutMultiColumnSet* m_currentColumnSet;
    unsigned m_currentFragmentainerGroupIndex;
    unsigned m_currentFragmentainerIndex;
    unsigned m_endFragmentainerIndex;

    LayoutUnit m_logicalTopInFlowThread;
    LayoutUnit m_logicalBottomInFlowThread;

    LayoutSize m_paginationOffset;
    LayoutRect m_clipRectInFlowThread;

    const MultiColumnFragmentainerGroup& currentGroup() const;
    void moveToNextFragmentainerGroup();
    bool setFragmentainersOfInterest();
    void updateOutput();
    void setAtEnd() { m_currentColumnSet = nullptr; }
};

} // namespace blink

#endif // FragmentainerIterator_h

