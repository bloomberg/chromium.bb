/*
 * Copyright (C) 2006, 2007, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2015 Google Inc. All rights reserved.
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

#ifndef SelectionController_h
#define SelectionController_h

#include "core/CoreExport.h"
#include "core/dom/DocumentShutdownObserver.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/PositionWithAffinity.h"
#include "core/editing/TextGranularity.h"
#include "core/page/EventWithHitTestResults.h"
#include "platform/heap/Handle.h"

namespace blink {

class HitTestResult;
class LocalFrame;

class CORE_EXPORT SelectionController final
    : public GarbageCollectedFinalized<SelectionController>,
      public DocumentShutdownObserver {
  WTF_MAKE_NONCOPYABLE(SelectionController);
  USING_GARBAGE_COLLECTED_MIXIN(SelectionController);

 public:
  static SelectionController* Create(LocalFrame&);
  virtual ~SelectionController();
  void Trace(blink::Visitor*);

  bool HandleMousePressEvent(const MouseEventWithHitTestResults&);
  void HandleMouseDraggedEvent(const MouseEventWithHitTestResults&,
                               const IntPoint&,
                               const LayoutPoint&,
                               const IntPoint&);
  bool HandleMouseReleaseEvent(const MouseEventWithHitTestResults&,
                               const LayoutPoint&);
  bool HandlePasteGlobalSelection(const WebMouseEvent&);
  bool HandleGestureLongPress(const HitTestResult&);
  void HandleGestureTwoFingerTap(const GestureEventWithHitTestResults&);
  void HandleGestureLongTap(const GestureEventWithHitTestResults&);

  void UpdateSelectionForMouseDrag(const LayoutPoint&, const IntPoint&);
  void UpdateSelectionForMouseDrag(const HitTestResult&,
                                   const LayoutPoint&,
                                   const IntPoint&);
  void SendContextMenuEvent(const MouseEventWithHitTestResults&,
                            const LayoutPoint&);
  void PassMousePressEventToSubframe(const MouseEventWithHitTestResults&);

  void InitializeSelectionState();
  void SetMouseDownMayStartSelect(bool);
  bool MouseDownMayStartSelect() const;
  bool MouseDownWasSingleClickInSelection() const;
  void NotifySelectionChanged();
  bool HasExtendedSelection() const {
    return selection_state_ == SelectionState::kExtendedSelection;
  }

 private:
  friend class SelectionControllerTest;

  explicit SelectionController(LocalFrame&);

  enum class AppendTrailingWhitespace { kShouldAppend, kDontAppend };
  enum class SelectInputEventType { kTouch, kMouse };
  enum EndPointsAdjustmentMode {
    kAdjustEndpointsAtBidiBoundary,
    kDoNotAdjustEndpoints
  };

  Document& GetDocument() const;

  // Returns |true| if a word was selected.
  bool SelectClosestWordFromHitTestResult(const HitTestResult&,
                                          AppendTrailingWhitespace,
                                          SelectInputEventType);
  void SelectClosestMisspellingFromHitTestResult(const HitTestResult&,
                                                 AppendTrailingWhitespace);
  // Returns |true| if a word was selected.
  bool SelectClosestWordFromMouseEvent(const MouseEventWithHitTestResults&);
  void SelectClosestMisspellingFromMouseEvent(
      const MouseEventWithHitTestResults&);
  void SelectClosestWordOrLinkFromMouseEvent(
      const MouseEventWithHitTestResults&);
  void SetNonDirectionalSelectionIfNeeded(const SelectionInFlatTree&,
                                          TextGranularity,
                                          EndPointsAdjustmentMode,
                                          HandleVisibility);
  void SetCaretAtHitTestResult(const HitTestResult&);
  bool UpdateSelectionForMouseDownDispatchingSelectStart(
      Node*,
      const SelectionInFlatTree&,
      TextGranularity,
      HandleVisibility);

  FrameSelection& Selection() const;

  // Implements |DocumentShutdownObserver|.
  // TODO(yosin): We should relocate |m_originalBaseInFlatTree| when DOM tree
  // changed.
  void ContextDestroyed(Document*) final;

  bool HandleSingleClick(const MouseEventWithHitTestResults&);
  bool HandleDoubleClick(const MouseEventWithHitTestResults&);
  bool HandleTripleClick(const MouseEventWithHitTestResults&);

  bool HandleTapInsideSelection(const MouseEventWithHitTestResults&,
                                const SelectionInFlatTree&);

  Member<LocalFrame> const frame_;
  // Used to store base before the adjustment at bidi boundary
  PositionInFlatTreeWithAffinity original_base_in_flat_tree_;
  bool mouse_down_may_start_select_;
  bool mouse_down_was_single_click_in_selection_;
  bool mouse_down_allows_multi_click_;
  enum class SelectionState {
    kHaveNotStartedSelection,
    kPlacedCaret,
    kExtendedSelection
  };
  SelectionState selection_state_;
};

bool IsLinkSelection(const MouseEventWithHitTestResults&);
bool IsExtendingSelection(const MouseEventWithHitTestResults&);

}  // namespace blink

#endif  // SelectionController_h
