/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights
 * reserved.
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

#include <memory>

#include "base/macros.h"
#include "core/CoreExport.h"
#include "core/dom/SynchronousMutationObserver.h"
#include "core/editing/Forward.h"
#include "core/editing/SetSelectionOptions.h"
#include "core/layout/ScrollAlignment.h"
#include "platform/geometry/IntRect.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Optional.h"

namespace blink {

class DisplayItemClient;
class Element;
class LayoutBlock;
class LocalFrame;
class FrameCaret;
class GranularityStrategy;
class GraphicsContext;
class Range;
class SelectionEditor;
class LayoutSelection;
enum class SelectionModifyAlteration;
enum class SelectionModifyDirection;
class TextIteratorBehavior;
struct PaintInvalidatorContext;

enum RevealExtentOption { kRevealExtent, kDoNotRevealExtent };

enum class CaretVisibility;

enum class HandleVisibility { kNotVisible, kVisible };

class CORE_EXPORT FrameSelection final
    : public GarbageCollectedFinalized<FrameSelection>,
      public SynchronousMutationObserver {
  USING_GARBAGE_COLLECTED_MIXIN(FrameSelection);

 public:
  static FrameSelection* Create(LocalFrame& frame) {
    return new FrameSelection(frame);
  }
  ~FrameSelection();

  bool IsAvailable() const { return LifecycleContext(); }
  // You should not call |document()| when |!isAvailable()|.
  Document& GetDocument() const;
  LocalFrame* GetFrame() const { return frame_; }
  Element* RootEditableElementOrDocumentElement() const;

  // An implementation of |WebFrame::moveCaretSelection()|
  void MoveCaretSelection(const IntPoint&);

  VisibleSelection ComputeVisibleSelectionInDOMTree() const;
  VisibleSelectionInFlatTree ComputeVisibleSelectionInFlatTree() const;

  // TODO(editing-dev): We should replace
  // |computeVisibleSelectionInDOMTreeDeprecated()| with update layout and
  // |computeVisibleSelectionInDOMTree()| to increase places hoisting update
  // layout.
  VisibleSelection ComputeVisibleSelectionInDOMTreeDeprecated() const;

  void SetSelection(const SelectionInDOMTree&, const SetSelectionOptions&);

  // TODO(editing-dev): We should rename this function to
  // SetSelectionAndEndTyping()
  // Set selection with end of typing processing == close typing and clear
  // typing style.
  void SetSelection(const SelectionInDOMTree&);
  void SelectAll(SetSelectionBy);
  void SelectAll();
  void Clear();
  bool IsHidden() const;

  // TODO(tkent): These two functions were added to fix crbug.com/695211 without
  // changing focus behavior. Once we fix crbug.com/690272, we can remove these
  // functions.
  // setSelectionDeprecated() returns true if didSetSelectionDeprecated() should
  // be called.
  bool SetSelectionDeprecated(const SelectionInDOMTree&,
                              const SetSelectionOptions&);
  void DidSetSelectionDeprecated(const SetSelectionOptions&);

  // Call this after doing user-triggered selections to make it easy to delete
  // the frame you entirely selected.
  void SelectFrameElementInParentIfFullySelected();

  bool Contains(const LayoutPoint&);

  bool Modify(SelectionModifyAlteration,
              SelectionModifyDirection,
              TextGranularity,
              SetSelectionBy);

  // Moves the selection extent based on the selection granularity strategy.
  // This function does not allow the selection to collapse. If the new
  // extent is resolved to the same position as the current base, this
  // function will do nothing.
  void MoveRangeSelectionExtent(const IntPoint&);
  void MoveRangeSelection(const VisiblePosition& base,
                          const VisiblePosition& extent,
                          TextGranularity);

  TextGranularity Granularity() const { return granularity_; }

  // Returns true if specified layout block should paint caret. This function is
  // called during painting only.
  bool ShouldPaintCaret(const LayoutBlock&) const;

  // Bounds of (possibly transformed) caret in absolute coords
  IntRect AbsoluteCaretBounds() const;

  // Returns anchor and focus bounds in absolute coords.
  // If the selection range is empty, returns the caret bounds.
  // Note: this updates styles and layout, use cautiously.
  bool ComputeAbsoluteBounds(IntRect& anchor, IntRect& focus) const;

  void DidChangeFocus();

  SelectionInDOMTree GetSelectionInDOMTree() const;
  bool IsDirectional() const;

  void DocumentAttached(Document*);

  void DidLayout();
  bool NeedsLayoutSelectionUpdate() const;
  void CommitAppearanceIfNeeded();
  void SetCaretVisible(bool caret_is_visible);
  void ScheduleVisualUpdate() const;
  void ScheduleVisualUpdateForPaintInvalidationIfNeeded() const;

  // Paint invalidation methods delegating to FrameCaret.
  void ClearPreviousCaretVisualRect(const LayoutBlock&);
  void LayoutBlockWillBeDestroyed(const LayoutBlock&);
  void UpdateStyleAndLayoutIfNeeded();
  void InvalidatePaint(const LayoutBlock&, const PaintInvalidatorContext&);

  void PaintCaret(GraphicsContext&, const LayoutPoint&);

  // Used to suspend caret blinking while the mouse is down.
  void SetCaretBlinkingSuspended(bool);
  bool IsCaretBlinkingSuspended() const;

  // Focus
  bool SelectionHasFocus() const;
  void SetFrameIsFocused(bool);
  bool FrameIsFocused() const { return focused_; }
  bool FrameIsFocusedAndActive() const;
  void PageActivationChanged();

  void SetUseSecureKeyboardEntryWhenActive(bool);

  bool IsHandleVisible() const { return is_handle_visible_; }
  bool ShouldShrinkNextTap() const { return should_shrink_next_tap_; }

  void UpdateSecureKeyboardEntryIfActive();

  // Returns true if a word is selected.
  bool SelectWordAroundCaret();

#ifndef NDEBUG
  void ShowTreeForThis() const;
#endif

  void SetFocusedNodeIfNeeded();
  void NotifyTextControlOfSelectionChange(SetSelectionBy);

  String SelectedHTMLForClipboard() const;
  String SelectedText(const TextIteratorBehavior&) const;
  String SelectedText() const;
  String SelectedTextForClipboard() const;

  // This returns last layouted selection bounds of LayoutSelection rather than
  // SelectionEditor keeps.
  LayoutRect UnclippedBounds() const;

  // TODO(tkent): This function has a bug that scrolling doesn't work well in
  // a case of RangeSelection. crbug.com/443061
  void RevealSelection(
      const ScrollAlignment& = ScrollAlignment::kAlignCenterIfNeeded,
      RevealExtentOption = kDoNotRevealExtent);
  void SetSelectionFromNone();

  void UpdateAppearance();
  bool ShouldShowBlockCursor() const;
  void SetShouldShowBlockCursor(bool);

  void CacheRangeOfDocument(Range*);
  Range* DocumentCachedRange() const;
  void ClearDocumentCachedRange();

  FrameCaret& FrameCaretForTesting() const { return *frame_caret_; }

  WTF::Optional<unsigned> LayoutSelectionStart() const;
  WTF::Optional<unsigned> LayoutSelectionEnd() const;
  void ClearLayoutSelection();

  void Trace(blink::Visitor*);

 private:
  friend class CaretDisplayItemClientTest;
  friend class FrameSelectionTest;
  friend class PaintControllerPaintTestBase;
  friend class SelectionControllerTest;

  explicit FrameSelection(LocalFrame&);

  const DisplayItemClient& CaretDisplayItemClientForTesting() const;

  // Note: We have |selectionInFlatTree()| for unit tests, we should
  // use |visibleSelection<EditingInFlatTreeStrategy>()|.
  VisibleSelectionInFlatTree GetSelectionInFlatTree() const;

  void NotifyAccessibilityForSelectionChange();
  void NotifyCompositorForSelectionChange();
  void NotifyEventHandlerForSelectionChange();

  void FocusedOrActiveStateChanged();

  void SetUseSecureKeyboardEntry(bool);

  void UpdateSelectionIfNeeded(const Position& base,
                               const Position& extent,
                               const Position& start,
                               const Position& end);

  GranularityStrategy* GetGranularityStrategy();

  IntRect ComputeRectToScroll(RevealExtentOption);

  // Implementation of |SynchronousMutationObserver| member functions.
  void ContextDestroyed(Document*) final;
  void NodeChildrenWillBeRemoved(ContainerNode&) final;
  void NodeWillBeRemoved(Node&) final;

  Member<LocalFrame> frame_;
  const Member<LayoutSelection> layout_selection_;
  const Member<SelectionEditor> selection_editor_;

  TextGranularity granularity_;
  LayoutUnit x_pos_for_vertical_arrow_navigation_;

  bool focused_ : 1;
  bool is_handle_visible_ = false;
  bool should_shrink_next_tap_ = false;

  // Controls text granularity used to adjust the selection's extent in
  // moveRangeSelectionExtent.
  std::unique_ptr<GranularityStrategy> granularity_strategy_;

  const Member<FrameCaret> frame_caret_;
  bool use_secure_keyboard_entry_when_active_ = false;

  DISALLOW_COPY_AND_ASSIGN(FrameSelection);
};

}  // namespace blink

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
void showTree(const blink::FrameSelection&);
void showTree(const blink::FrameSelection*);
#endif

#endif  // FrameSelection_h
