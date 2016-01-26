/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#ifndef FrameSelection_h
#define FrameSelection_h

#include "core/CoreExport.h"
#include "core/dom/Range.h"
#include "core/editing/CaretBase.h"
#include "core/editing/EditingStyle.h"
#include "core/editing/EphemeralRange.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/VisibleSelection.h"
#include "core/editing/iterators/TextIteratorFlags.h"
#include "core/layout/ScrollAlignment.h"
#include "platform/Timer.h"
#include "platform/geometry/IntRect.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"

namespace blink {

class CharacterData;
class CullRect;
class LocalFrame;
class GranularityStrategy;
class GraphicsContext;
class HTMLFormElement;
class SelectionEditor;
class PendingSelection;
class Text;

enum class CursorAlignOnScroll { IfNeeded, Always };

enum EUserTriggered { NotUserTriggered = 0, UserTriggered = 1 };

enum RevealExtentOption {
    RevealExtent,
    DoNotRevealExtent
};

enum class SelectionDirectionalMode { NonDirectional, Directional };

class CORE_EXPORT FrameSelection final : public NoBaseWillBeGarbageCollectedFinalized<FrameSelection>, private CaretBase {
    WTF_MAKE_NONCOPYABLE(FrameSelection);
    USING_FAST_MALLOC_WILL_BE_REMOVED(FrameSelection);
public:
    static PassOwnPtrWillBeRawPtr<FrameSelection> create(LocalFrame* frame = nullptr)
    {
        return adoptPtrWillBeNoop(new FrameSelection(frame));
    }
    ~FrameSelection();

    enum EAlteration { AlterationMove, AlterationExtend };
    enum SetSelectionOption {
        // 1 << 0 is reserved for EUserTriggered
        CloseTyping = 1 << 1,
        ClearTypingStyle = 1 << 2,
        SpellCorrectionTriggered = 1 << 3,
        DoNotSetFocus = 1 << 4,
        DoNotUpdateAppearance = 1 << 5,
        DoNotClearStrategy = 1 << 6,
        DoNotAdjustInComposedTree = 1 << 7,
    };
    typedef unsigned SetSelectionOptions; // Union of values in SetSelectionOption and EUserTriggered
    static inline EUserTriggered selectionOptionsToUserTriggered(SetSelectionOptions options)
    {
        return static_cast<EUserTriggered>(options & UserTriggered);
    }

    LocalFrame* frame() const { return m_frame; }
    Element* rootEditableElement() const { return selection().rootEditableElement(); }
    Element* rootEditableElementOrDocumentElement() const;
    ContainerNode* rootEditableElementOrTreeScopeRootNode() const;

    bool hasEditableStyle() const { return selection().hasEditableStyle(); }
    bool isContentEditable() const { return selection().isContentEditable(); }
    bool isContentRichlyEditable() const { return selection().isContentRichlyEditable(); }

    void moveTo(const VisiblePosition&, EUserTriggered = NotUserTriggered, CursorAlignOnScroll = CursorAlignOnScroll::IfNeeded);
    void moveTo(const VisiblePosition&, const VisiblePosition&, EUserTriggered = NotUserTriggered);
    void moveTo(const Position&, TextAffinity, EUserTriggered = NotUserTriggered);

    template <typename Strategy>
    const VisibleSelectionTemplate<Strategy>& visibleSelection() const;

    const VisibleSelection& selection() const;
    void setSelection(const VisibleSelection&, SetSelectionOptions = CloseTyping | ClearTypingStyle, CursorAlignOnScroll = CursorAlignOnScroll::IfNeeded, TextGranularity = CharacterGranularity);
    void setSelection(const VisibleSelectionInComposedTree&, SetSelectionOptions = CloseTyping | ClearTypingStyle, CursorAlignOnScroll = CursorAlignOnScroll::IfNeeded, TextGranularity = CharacterGranularity);
    // TODO(yosin) We should get rid of two parameters version of
    // |setSelection()| to avoid conflict of four parameters version.
    void setSelection(const VisibleSelection& selection, TextGranularity granularity) { setSelection(selection, CloseTyping | ClearTypingStyle, CursorAlignOnScroll::IfNeeded, granularity); }
    void setSelection(const VisibleSelectionInComposedTree& selection, TextGranularity granularity) { setSelection(selection, CloseTyping | ClearTypingStyle, CursorAlignOnScroll::IfNeeded, granularity); }
    // TODO(yosin) We should get rid of |Range| version of |setSelectedRagne()|
    // for Oilpan.
    bool setSelectedRange(Range*, TextAffinity, SelectionDirectionalMode = SelectionDirectionalMode::NonDirectional, SetSelectionOptions = CloseTyping | ClearTypingStyle);
    bool setSelectedRange(const EphemeralRange&, TextAffinity, SelectionDirectionalMode = SelectionDirectionalMode::NonDirectional, FrameSelection::SetSelectionOptions = CloseTyping | ClearTypingStyle);
    void selectAll();
    void clear();
    void prepareForDestruction();

    // Call this after doing user-triggered selections to make it easy to delete the frame you entirely selected.
    void selectFrameElementInParentIfFullySelected();

    bool contains(const LayoutPoint&);

    SelectionType selectionType() const { return selection().selectionType(); }

    TextAffinity affinity() const { return selection().affinity(); }

    bool modify(EAlteration, SelectionDirection, TextGranularity, EUserTriggered = NotUserTriggered);
    enum VerticalDirection { DirectionUp, DirectionDown };
    bool modify(EAlteration, unsigned verticalDistance, VerticalDirection, EUserTriggered = NotUserTriggered, CursorAlignOnScroll = CursorAlignOnScroll::IfNeeded);

    // Moves the selection extent based on the selection granularity strategy.
    // This function does not allow the selection to collapse. If the new
    // extent is resolved to the same position as the current base, this
    // function will do nothing.
    void moveRangeSelectionExtent(const IntPoint&);
    void moveRangeSelection(const VisiblePosition& base, const VisiblePosition& extent, TextGranularity);

    TextGranularity granularity() const { return m_granularity; }

    void setStart(const VisiblePosition &, EUserTriggered = NotUserTriggered);
    void setEnd(const VisiblePosition &, EUserTriggered = NotUserTriggered);

    void setBase(const VisiblePosition&, EUserTriggered = NotUserTriggered);
    void setExtent(const VisiblePosition&, EUserTriggered = NotUserTriggered);

    Position base() const { return selection().base(); }
    Position extent() const { return selection().extent(); }
    Position start() const { return selection().start(); }
    Position end() const { return selection().end(); }

    // Return the layoutObject that is responsible for painting the caret (in the selection start node)
    LayoutBlock* caretLayoutObject() const;

    // Bounds of (possibly transformed) caret in absolute coords
    IntRect absoluteCaretBounds();

    void didChangeFocus();

    bool isNone() const { return selection().isNone(); }
    bool isCaret() const { return selection().isCaret(); }
    bool isRange() const { return selection().isRange(); }
    bool isCaretOrRange() const { return selection().isCaretOrRange(); }
    bool isInPasswordField() const;
    bool isDirectional() const { return selection().isDirectional(); }

    // If this FrameSelection has a logical range which is still valid, this function return its clone. Otherwise,
    // the return value from underlying VisibleSelection's firstRange() is returned.
    PassRefPtrWillBeRawPtr<Range> firstRange() const;

    void nodeWillBeRemoved(Node&);
    void didUpdateCharacterData(CharacterData*, unsigned offset, unsigned oldLength, unsigned newLength);
    void didMergeTextNodes(const Text& oldNode, unsigned offset);
    void didSplitTextNode(const Text& oldNode);

    bool isAppearanceDirty() const;
    void commitAppearanceIfNeeded(LayoutView&);
    void updateAppearance();
    void setCaretVisible(bool caretIsVisible) { setCaretVisibility(caretIsVisible ? Visible : Hidden); }
    bool isCaretBoundsDirty() const { return m_caretRectDirty; }
    void setCaretRectNeedsUpdate();
    void scheduleVisualUpdate() const;
    void invalidateCaretRect();
    void paintCaret(GraphicsContext&, const LayoutPoint&);
    bool ShouldPaintCaretForTesting() const { return m_shouldPaintCaret; }

    // Used to suspend caret blinking while the mouse is down.
    void setCaretBlinkingSuspended(bool suspended) { m_isCaretBlinkingSuspended = suspended; }
    bool isCaretBlinkingSuspended() const { return m_isCaretBlinkingSuspended; }

    // Focus
    void setFocused(bool);
    bool isFocused() const { return m_focused; }
    bool isFocusedAndActive() const;
    void pageActivationChanged();

    void updateSecureKeyboardEntryIfActive();

    // Returns true if a word is selected.
    bool selectWordAroundPosition(const VisiblePosition&);

#ifndef NDEBUG
    void formatForDebugger(char* buffer, unsigned length) const;
    void showTreeForThis() const;
#endif

    enum EndPointsAdjustmentMode { AdjustEndpointsAtBidiBoundary, DoNotAdjsutEndpoints };
    void setNonDirectionalSelectionIfNeeded(const VisibleSelection&, TextGranularity, EndPointsAdjustmentMode = DoNotAdjsutEndpoints);
    void setNonDirectionalSelectionIfNeeded(const VisibleSelectionInComposedTree&, TextGranularity, EndPointsAdjustmentMode = DoNotAdjsutEndpoints);
    void setFocusedNodeIfNeeded();
    void notifyLayoutObjectOfSelectionChange(EUserTriggered);

    EditingStyle* typingStyle() const;
    void setTypingStyle(PassRefPtrWillBeRawPtr<EditingStyle>);
    void clearTypingStyle();

    String selectedHTMLForClipboard() const;
    String selectedText(TextIteratorBehavior = TextIteratorDefaultBehavior) const;
    String selectedTextForClipboard() const;

    // The bounds are clipped to the viewport as this is what callers expect.
    LayoutRect bounds() const;
    LayoutRect unclippedBounds() const;

    HTMLFormElement* currentForm() const;

    void revealSelection(const ScrollAlignment& = ScrollAlignment::alignCenterIfNeeded, RevealExtentOption = DoNotRevealExtent);
    void setSelectionFromNone();

    bool shouldShowBlockCursor() const { return m_shouldShowBlockCursor; }
    void setShouldShowBlockCursor(bool);

    // TODO(yosin): We should check DOM tree version and style version in
    // |FrameSelection::selection()| to make sure we use updated selection,
    // rather than having |updateIfNeeded()|. Once, we update all layout tests
    // to use updated selection, we should make |updateIfNeeded()| private.
    void updateIfNeeded();

    DECLARE_VIRTUAL_TRACE();

private:
    friend class FrameSelectionTest;

    explicit FrameSelection(LocalFrame*);

    // Note: We have |selectionInComposedTree()| for unit tests, we should
    // use |visibleSelection<EditingInComposedTreeStrategy>()|.
    const VisibleSelectionInComposedTree& selectionInComposedTree() const;

    template <typename Strategy>
    VisiblePositionTemplate<Strategy> originalBase() const;
    void setOriginalBase(const VisiblePosition& newBase) { m_originalBase = newBase; }
    void setOriginalBase(const VisiblePositionInComposedTree& newBase) { m_originalBaseInComposedTree = newBase; }

    template <typename Strategy>
    bool containsAlgorithm(const LayoutPoint&);

    template <typename Strategy>
    void setNonDirectionalSelectionIfNeededAlgorithm(const VisibleSelectionTemplate<Strategy>&, TextGranularity, EndPointsAdjustmentMode);

    template <typename Strategy>
    void setSelectionAlgorithm(const VisibleSelectionTemplate<Strategy>&, SetSelectionOptions, CursorAlignOnScroll, TextGranularity);

    void respondToNodeModification(Node&, bool baseRemoved, bool extentRemoved, bool startRemoved, bool endRemoved);

    void notifyAccessibilityForSelectionChange();
    void notifyCompositorForSelectionChange();
    void notifyEventHandlerForSelectionChange();

    void focusedOrActiveStateChanged();

    void caretBlinkTimerFired(Timer<FrameSelection>*);
    void stopCaretBlinkTimer();

    void setUseSecureKeyboardEntry(bool);

    void setCaretVisibility(CaretVisibility);
    bool shouldBlinkCaret() const;

    void updateSelectionIfNeeded(const Position& base, const Position& extent, const Position& start, const Position& end);

    template <typename Strategy>
    VisibleSelectionTemplate<Strategy> validateSelection(const VisibleSelectionTemplate<Strategy>&);

    GranularityStrategy* granularityStrategy();

    RawPtrWillBeMember<LocalFrame> m_frame;
    const OwnPtrWillBeMember<PendingSelection> m_pendingSelection;
    const OwnPtrWillBeMember<SelectionEditor> m_selectionEditor;

    // Used to store base before the adjustment at bidi boundary
    VisiblePosition m_originalBase;
    VisiblePositionInComposedTree m_originalBaseInComposedTree;
    TextGranularity m_granularity;

    RefPtrWillBeMember<Node> m_previousCaretNode; // The last node which painted the caret. Retained for clearing the old caret when it moves.
    LayoutRect m_previousCaretRect;
    CaretVisibility m_previousCaretVisibility;

    RefPtrWillBeMember<EditingStyle> m_typingStyle;

    Timer<FrameSelection> m_caretBlinkTimer;

    bool m_caretRectDirty : 1;
    bool m_shouldPaintCaret : 1;
    bool m_isCaretBlinkingSuspended : 1;
    bool m_focused : 1;
    bool m_shouldShowBlockCursor : 1;

    // Controls text granularity used to adjust the selection's extent in moveRangeSelectionExtent.
    OwnPtr<GranularityStrategy> m_granularityStrategy;
};

inline EditingStyle* FrameSelection::typingStyle() const
{
    return m_typingStyle.get();
}

inline void FrameSelection::clearTypingStyle()
{
    m_typingStyle.clear();
}

inline void FrameSelection::setTypingStyle(PassRefPtrWillBeRawPtr<EditingStyle> style)
{
    m_typingStyle = style;
}
} // namespace blink

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
void showTree(const blink::FrameSelection&);
void showTree(const blink::FrameSelection*);
#endif

#endif // FrameSelection_h
