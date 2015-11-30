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

#ifndef SelectionEditor_h
#define SelectionEditor_h

#include "core/editing/FrameSelection.h"

namespace blink {

class SelectionEditor final : public NoBaseWillBeGarbageCollectedFinalized<SelectionEditor>, public VisibleSelectionChangeObserver {
    WTF_MAKE_NONCOPYABLE(SelectionEditor);
    USING_FAST_MALLOC_WILL_BE_REMOVED(SelectionEditor);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(SelectionEditor);
public:
    // TODO(yosin) We should move |EAlteration| and |VerticalDirection| out
    // from |FrameSelection| class like |EUserTriggered|.
    typedef FrameSelection::EAlteration EAlteration;
    typedef FrameSelection::VerticalDirection VerticalDirection;

    static PassOwnPtrWillBeRawPtr<SelectionEditor> create(FrameSelection& frameSelection)
    {
        return adoptPtrWillBeNoop(new SelectionEditor(frameSelection));
    }
    virtual ~SelectionEditor();

    bool hasEditableStyle() const { return m_selection.hasEditableStyle(); }
    bool isContentEditable() const { return m_selection.isContentEditable(); }
    bool isContentRichlyEditable() const { return m_selection.isContentRichlyEditable(); }

    bool setSelectedRange(const EphemeralRange&, TextAffinity, SelectionDirectionalMode, FrameSelection::SetSelectionOptions);

    bool modify(EAlteration, SelectionDirection, TextGranularity, EUserTriggered);
    bool modify(EAlteration, unsigned verticalDistance, VerticalDirection, EUserTriggered, CursorAlignOnScroll);

    template <typename Strategy>
    const VisibleSelectionTemplate<Strategy>& visibleSelection() const;
    void setVisibleSelection(const VisibleSelection&, FrameSelection::SetSelectionOptions);
    void setVisibleSelection(const VisibleSelectionInComposedTree&, FrameSelection::SetSelectionOptions);

    void setIsDirectional(bool);
    void setWithoutValidation(const Position& base, const Position& extent);

    void resetXPosForVerticalArrowNavigation();

    void willBeModified(EAlteration, SelectionDirection);

    // If this FrameSelection has a logical range which is still valid, this
    // function return its clone. Otherwise, the return value from underlying
    // |VisibleSelection|'s |firstRange()| is returned.
    PassRefPtrWillBeRawPtr<Range> firstRange() const;

    // VisibleSelectionChangeObserver interface.
    void didChangeVisibleSelection() override;

    DECLARE_VIRTUAL_TRACE();

private:
    explicit SelectionEditor(FrameSelection&);

    // TODO(yosin) We should use capitalized name for |EPositionType|.
    enum EPositionType { START, END, BASE, EXTENT }; // NOLINT

    LocalFrame* frame() const;

    void adjustVisibleSelectionInComposedTree();
    void adjustVisibleSelectionInDOMTree();

    TextDirection directionOfEnclosingBlock();
    TextDirection directionOfSelection();

    VisiblePosition positionForPlatform(bool isGetStart) const;
    VisiblePosition startForPlatform() const;
    VisiblePosition endForPlatform() const;
    VisiblePosition nextWordPositionForPlatform(const VisiblePosition&);

    VisiblePosition modifyExtendingRight(TextGranularity);
    VisiblePosition modifyExtendingForward(TextGranularity);
    VisiblePosition modifyMovingRight(TextGranularity);
    VisiblePosition modifyMovingForward(TextGranularity);
    VisiblePosition modifyExtendingLeft(TextGranularity);
    VisiblePosition modifyExtendingBackward(TextGranularity);
    VisiblePosition modifyMovingLeft(TextGranularity);
    VisiblePosition modifyMovingBackward(TextGranularity);

    void startObservingVisibleSelectionChange();
    void stopObservingVisibleSelectionChangeIfNecessary();

    LayoutUnit lineDirectionPointForBlockDirectionNavigation(EPositionType);
    bool dispatchSelectStart();

    RawPtrWillBeMember<FrameSelection> m_frameSelection;

    LayoutUnit m_xPosForVerticalArrowNavigation;
    VisibleSelection m_selection;
    VisibleSelectionInComposedTree m_selectionInComposedTree;
    bool m_observingVisibleSelection;

    // The range specified by the user, which may not be visually canonicalized
    // (hence "logical"). This will be invalidated if the underlying
    // |VisibleSelection| changes. If that happens, this variable will
    // become |nullptr|, in which case logical positions == visible positions.
    RefPtrWillBeMember<Range> m_logicalRange;
};

} // namespace blink

#endif // SelectionEditor_h
