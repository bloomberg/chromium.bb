/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef VisibleSelection_h
#define VisibleSelection_h

#include "core/CoreExport.h"
#include "core/editing/EditingStrategy.h"
#include "core/editing/EphemeralRange.h"
#include "core/editing/SelectionType.h"
#include "core/editing/TextAffinity.h"
#include "core/editing/TextGranularity.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/VisibleUnits.h"
#include "wtf/Allocator.h"

namespace blink {

class LayoutPoint;

// TODO(yosin) We should use capitalized name instead of |SEL_DEFAULT_AFFINITY|.
const TextAffinity SEL_DEFAULT_AFFINITY = TextAffinity::Downstream; // NOLINT
enum SelectionDirection { DirectionForward, DirectionBackward, DirectionRight, DirectionLeft };

// Listener of |VisibleSelection| modification. |didChangeVisibleSelection()|
// will be invoked when base, extent, start or end is moved to a different
// position.
//
// Objects implementing |VisibleSelectionChangeObserver| interface must outlive
// the |VisibleSelection| object.
class CORE_EXPORT VisibleSelectionChangeObserver : public WillBeGarbageCollectedMixin {
    WTF_MAKE_NONCOPYABLE(VisibleSelectionChangeObserver);
public:
    VisibleSelectionChangeObserver();
    virtual ~VisibleSelectionChangeObserver();
    virtual void didChangeVisibleSelection() = 0;
    DEFINE_INLINE_VIRTUAL_TRACE() { }
};

class CORE_EXPORT VisibleSelection {
    DISALLOW_ALLOCATION();
    DECLARE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(VisibleSelection);
public:
    class InDOMTree {
        STATIC_ONLY(InDOMTree);
    public:
        using PositionType = Position;
        using Strategy = EditingStrategy;

        static EphemeralRange asRange(const VisibleSelection&);
        static bool equalSelections(const VisibleSelection&, const VisibleSelection&);
        static PositionType selectionBase(const VisibleSelection& selection) { return selection.base(); }
        static PositionType selectionExtent(const VisibleSelection& selection) { return selection.extent(); }
        static PositionType selectionStart(const VisibleSelection& selection) { return selection.start(); }
        static PositionType selectionEnd(const VisibleSelection& selection) { return selection.end(); }
        static SelectionType selectionType(const VisibleSelection& selection) { return selection.selectionType(); }
        static VisiblePosition selectionVisibleStart(const VisibleSelection& selection) { return selection.visibleStart(); }
        static VisiblePosition selectionVisibleEnd(const VisibleSelection& selection) { return selection.visibleEnd(); }
    };

    class InComposedTree {
        STATIC_ONLY(InComposedTree);
    public:
        using PositionType = PositionInComposedTree;
        using Strategy = EditingInComposedTreeStrategy;

        static EphemeralRangeInComposedTree asRange(const VisibleSelection&);
        static bool equalSelections(const VisibleSelection&, const VisibleSelection&);
        static bool isRange(const VisibleSelection& selection) { return selectionType(selection) == RangeSelection; }
        static PositionType selectionBase(const VisibleSelection& selection) { return selection.baseInComposedTree(); }
        static PositionType selectionExtent(const VisibleSelection& selection) { return selection.extentInComposedTree(); }
        static PositionType selectionStart(const VisibleSelection& selection) { return selection.startInComposedTree(); }
        static PositionType selectionEnd(const VisibleSelection& selection) { return selection.endInComposedTree(); }
        static SelectionType selectionType(const VisibleSelection& selection) { return selection.selectionTypeInComposedTree(); }
        static VisiblePosition selectionVisibleStart(const VisibleSelection& selection)
        {
            return createVisiblePositionInDOMTree(selectionStart(selection), isRange(selection) ? TextAffinity::Downstream : selection.affinity());
        }
        static VisiblePosition selectionVisibleEnd(const VisibleSelection& selection)
        {
            return createVisiblePositionInDOMTree(selectionEnd(selection), isRange(selection) ? TextAffinity::Upstream : selection.affinity());
        }
    };

    VisibleSelection();

    VisibleSelection(const Position&, TextAffinity, bool isDirectional = false);
    VisibleSelection(const Position& base, const Position& extent, TextAffinity = SEL_DEFAULT_AFFINITY, bool isDirectional = false);
    VisibleSelection(const PositionInComposedTree& base, const PositionInComposedTree& extent, TextAffinity = SEL_DEFAULT_AFFINITY, bool isDirectional = false);

    explicit VisibleSelection(const EphemeralRange&, TextAffinity = SEL_DEFAULT_AFFINITY, bool isDirectional = false);

    explicit VisibleSelection(const PositionWithAffinity&, bool isDirectional = false);
    explicit VisibleSelection(const VisiblePosition&, bool isDirectional = false);
    VisibleSelection(const VisiblePosition&, const VisiblePosition&, bool isDirectional = false);

    VisibleSelection(const VisibleSelection&);
    VisibleSelection& operator=(const VisibleSelection&);

    static VisibleSelection selectionFromContentsOfNode(Node*);

    SelectionType selectionType() const { return m_selectionType; }
    SelectionType selectionTypeInComposedTree() const;

    void setAffinity(TextAffinity affinity) { m_affinity = affinity; }
    TextAffinity affinity() const { return m_affinity; }

    void setBase(const Position&);
    void setBase(const PositionInComposedTree&);
    void setBase(const VisiblePosition&);
    void setExtent(const Position&);
    void setExtent(const PositionInComposedTree&);
    void setExtent(const VisiblePosition&);

    Position base() const { return m_base; }
    Position extent() const { return m_extent; }
    Position start() const { return m_start; }
    Position end() const { return m_end; }
    PositionInComposedTree baseInComposedTree() const;
    PositionInComposedTree extentInComposedTree() const;
    PositionInComposedTree startInComposedTree() const;
    PositionInComposedTree endInComposedTree() const;

    VisiblePosition visibleStart() const { return createVisiblePosition(m_start, isRange() ? TextAffinity::Downstream : affinity()); }
    VisiblePosition visibleEnd() const { return createVisiblePosition(m_end, isRange() ? TextAffinity::Upstream : affinity()); }
    VisiblePosition visibleBase() const { return createVisiblePosition(m_base, isRange() ? (isBaseFirst() ? TextAffinity::Upstream : TextAffinity::Downstream) : affinity()); }
    VisiblePosition visibleExtent() const { return createVisiblePosition(m_extent, isRange() ? (isBaseFirst() ? TextAffinity::Downstream : TextAffinity::Upstream) : affinity()); }

    bool isNone() const { return selectionType() == NoSelection; }
    bool isCaret() const { return selectionType() == CaretSelection; }
    bool isRange() const { return selectionType() == RangeSelection; }
    bool isCaretOrRange() const { return selectionType() != NoSelection; }
    bool isNonOrphanedRange() const { return isRange() && !start().isOrphan() && !end().isOrphan(); }
    bool isNonOrphanedCaretOrRange() const { return isCaretOrRange() && !start().isOrphan() && !end().isOrphan(); }
    static SelectionType selectionType(const Position& start, const Position& end);
    static SelectionType selectionType(const PositionInComposedTree& start, const PositionInComposedTree& end);

    bool isBaseFirst() const { return m_baseIsFirst; }
    bool isDirectional() const { return m_isDirectional; }
    void setIsDirectional(bool isDirectional) { m_isDirectional = isDirectional; }

    void appendTrailingWhitespace();

    bool expandUsingGranularity(TextGranularity);
    bool expandUsingGranularityInComposedTree(TextGranularity);

    // TODO(yosin) Most callers probably don't want these functions, but
    // are using them for historical reasons. |toNormalizedEphemeralRange()|
    // contracts the range around text, and moves the caret most backward
    // visually equivalent position before returning the range/positions.
    EphemeralRange toNormalizedEphemeralRange() const;

    Element* rootEditableElement() const;
    bool isContentEditable() const;
    bool hasEditableStyle() const;
    bool isContentRichlyEditable() const;
    // Returns a shadow tree node for legacy shadow trees, a child of the
    // ShadowRoot node for new shadow trees, or 0 for non-shadow trees.
    Node* nonBoundaryShadowTreeRootNode() const;

    VisiblePosition visiblePositionRespectingEditingBoundary(const LayoutPoint& localPoint, Node* targetNode) const;
    PositionWithAffinity positionRespectingEditingBoundary(const LayoutPoint& localPoint, Node* targetNode) const;

    bool isValidFor(const Document&) const;
    void setWithoutValidation(const Position&, const Position&);
    void setWithoutValidation(const PositionInComposedTree&, const PositionInComposedTree&);

    void setChangeObserver(VisibleSelectionChangeObserver&);
    void clearChangeObserver();
    void didChange(); // Fire the change observer, if any.

    DECLARE_TRACE();

    void validatePositionsIfNeeded();

#ifndef NDEBUG
    void debugPosition(const char* message) const;
    void formatForDebugger(char* buffer, unsigned length) const;
    void showTreeForThis() const;
#endif

    void setStartRespectingGranularity(TextGranularity, EWordSide = RightWordIfOnBoundary);
    void setEndRespectingGranularity(TextGranularity, EWordSide = RightWordIfOnBoundary);

private:
    void validate(TextGranularity = CharacterGranularity);
    void resetPositionsInComposedTree();

    // Support methods for validate()
    void setBaseAndExtentToDeepEquivalents();
    void adjustSelectionToAvoidCrossingShadowBoundaries();
    void adjustSelectionToAvoidCrossingSelectionBoundaryInComposedTree();
    bool isBaseFirstInComposedTree() const;
    void adjustSelectionToAvoidCrossingEditingBoundaries();
    void adjustStartAndEndInComposedTree();
    void updateSelectionType();

    // We need to store these as Positions because VisibleSelection is
    // used to store values in editing commands for use when
    // undoing the command. We need to be able to create a selection that, while
    // currently invalid, will be valid once the changes are undone.

    Position m_base; // Where the first click happened
    Position m_extent; // Where the end click happened
    Position m_start; // Leftmost position when expanded to respect granularity
    Position m_end; // Rightmost position when expanded to respect granularity

    // TODO(hajimehoshi, yosin): The members m_*InComposedTree are now always
    // computed from the respective positions at validate(). To have selections
    // work on the composed tree more accurately, we need to compute the DOM
    // positions from the composed tree positions. To do this, we need to add
    // considable amount of fixes (including EditingUtilities.cpp,
    // VisibleUnit.cpp, and VisiblePosition.cpp). We'll do that in the future.
    PositionInComposedTree m_baseInComposedTree;
    PositionInComposedTree m_extentInComposedTree;
    PositionInComposedTree m_startInComposedTree;
    PositionInComposedTree m_endInComposedTree;

    TextAffinity m_affinity; // the upstream/downstream affinity of the caret

    // Oilpan: this reference has a lifetime that is at least as long
    // as this object.
    RawPtrWillBeMember<VisibleSelectionChangeObserver> m_changeObserver;

    // these are cached, can be recalculated by validate()
    SelectionType m_selectionType; // None, Caret, Range
    bool m_baseIsFirst : 1; // True if base is before the extent
    bool m_isDirectional : 1; // Non-directional ignores m_baseIsFirst and selection always extends on shift + arrow key.
};

// TODO(yosin) This is transition version |VisibleSelectionTemplate| for
// getting rid of |VisibleSelection::InDOMTree| and |InComposedTree|.
// Final version of |VisibleSelectionTemplate| will replace |VisibleSelection|.
template <typename Strategy>
class CORE_TEMPLATE_CLASS_EXPORT VisibleSelectionTemplate final {
    STACK_ALLOCATED();
public:
    VisibleSelectionTemplate(const PositionAlgorithm<Strategy>& base, const PositionAlgorithm<Strategy>& extent, TextAffinity = TextAffinity::Downstream);
    explicit VisibleSelectionTemplate(const VisibleSelection&);
    explicit VisibleSelectionTemplate(const VisiblePositionTemplate<Strategy>&);

    operator const VisibleSelection&() const { return m_visibleSelection; }

    PositionAlgorithm<Strategy> base() const;
    PositionAlgorithm<Strategy> extent() const;
    PositionAlgorithm<Strategy> start() const;
    PositionAlgorithm<Strategy> end() const;

    bool isCaretOrRange() const { return m_visibleSelection.isCaretOrRange(); }
    bool isRange() const { return m_visibleSelection.isRange(); }
    bool isValidFor(const Document& document) const { return m_visibleSelection.isValidFor(document); }

    void setBase(const PositionAlgorithm<Strategy>&);
    void setBase(const VisiblePositionTemplate<Strategy>&);
    void setExtent(const PositionAlgorithm<Strategy>&);
    void setExtent(const VisiblePositionTemplate<Strategy>&);

    bool expandUsingGranularity(TextGranularity);

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_visibleSelection);
    }

private:
    VisibleSelection m_visibleSelection;
};

extern template class CORE_TEMPLATE_CLASS_EXPORT VisiblePositionTemplate<EditingStrategy>;
extern template class CORE_TEMPLATE_CLASS_EXPORT VisiblePositionTemplate<EditingInComposedTreeStrategy>;

inline bool equalSelectionsInDOMTree(const VisibleSelection& selection1, const VisibleSelection& selection2)
{
    return VisibleSelection::InDOMTree::equalSelections(selection1, selection2);
}

// We don't yet support multi-range selections, so we only ever have one range
// to return.
CORE_EXPORT EphemeralRange firstEphemeralRangeOf(const VisibleSelection&);

// TODO(sof): move more firstRangeOf() uses to be over EphemeralRange instead.
CORE_EXPORT PassRefPtrWillBeRawPtr<Range> firstRangeOf(const VisibleSelection&);

} // namespace blink

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
void showTree(const blink::VisibleSelection&);
void showTree(const blink::VisibleSelection*);
#endif

#endif // VisibleSelection_h
