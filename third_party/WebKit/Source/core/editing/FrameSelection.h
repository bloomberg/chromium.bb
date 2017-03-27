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
#include "core/CoreExport.h"
#include "core/dom/Range.h"
#include "core/dom/SynchronousMutationObserver.h"
#include "core/editing/EphemeralRange.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/VisibleSelection.h"
#include "core/editing/iterators/TextIteratorBehavior.h"
#include "core/layout/ScrollAlignment.h"
#include "platform/Timer.h"
#include "platform/geometry/IntRect.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/PaintInvalidationReason.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"

namespace blink {

class DisplayItemClient;
class LayoutBlock;
class LocalFrame;
class FrameCaret;
class GranularityStrategy;
class GraphicsContext;
class HTMLFormElement;
class SelectionEditor;
class PendingSelection;
class TextIteratorBehavior;
struct PaintInvalidatorContext;

enum class CursorAlignOnScroll { IfNeeded, Always };

enum EUserTriggered { NotUserTriggered = 0, UserTriggered = 1 };

enum RevealExtentOption { RevealExtent, DoNotRevealExtent };

enum class SelectionDirectionalMode { NonDirectional, Directional };

enum class CaretVisibility;

enum class HandleVisibility { NotVisible, Visible };

class CORE_EXPORT FrameSelection final
    : public GarbageCollectedFinalized<FrameSelection>,
      public SynchronousMutationObserver {
  WTF_MAKE_NONCOPYABLE(FrameSelection);
  USING_GARBAGE_COLLECTED_MIXIN(FrameSelection);

 public:
  static FrameSelection* create(LocalFrame& frame) {
    return new FrameSelection(frame);
  }
  ~FrameSelection();

  enum EAlteration { AlterationMove, AlterationExtend };
  enum SetSelectionOption {
    // 1 << 0 is reserved for EUserTriggered
    CloseTyping = 1 << 1,
    ClearTypingStyle = 1 << 2,
    DoNotSetFocus = 1 << 3,
    DoNotClearStrategy = 1 << 4,
  };
  // Union of values in SetSelectionOption and EUserTriggered
  typedef unsigned SetSelectionOptions;
  static inline EUserTriggered selectionOptionsToUserTriggered(
      SetSelectionOptions options) {
    return static_cast<EUserTriggered>(options & UserTriggered);
  }

  bool isAvailable() const { return lifecycleContext(); }
  // You should not call |document()| when |!isAvailable()|.
  Document& document() const;
  LocalFrame* frame() const { return m_frame; }
  Element* rootEditableElementOrDocumentElement() const;

  // An implementation of |WebFrame::moveCaretSelection()|
  void moveCaretSelection(const IntPoint&);

  const VisibleSelection& computeVisibleSelectionInDOMTree() const;
  const VisibleSelectionInFlatTree& computeVisibleSelectionInFlatTree() const;

  // TODO(editing-dev): We should replace
  // |computeVisibleSelectionInDOMTreeDeprecated()| with update layout and
  // |computeVisibleSelectionInDOMTree()| to increase places hoisting update
  // layout.
  const VisibleSelection& computeVisibleSelectionInDOMTreeDeprecated() const;

  void setSelection(const SelectionInDOMTree&,
                    SetSelectionOptions = CloseTyping | ClearTypingStyle,
                    CursorAlignOnScroll = CursorAlignOnScroll::IfNeeded,
                    TextGranularity = CharacterGranularity);

  void setSelection(const SelectionInFlatTree&,
                    SetSelectionOptions = CloseTyping | ClearTypingStyle,
                    CursorAlignOnScroll = CursorAlignOnScroll::IfNeeded,
                    TextGranularity = CharacterGranularity);
  bool setSelectedRange(
      const EphemeralRange&,
      TextAffinity,
      SelectionDirectionalMode = SelectionDirectionalMode::NonDirectional,
      FrameSelection::SetSelectionOptions = CloseTyping | ClearTypingStyle);
  void selectAll();
  void clear();

  // TODO(tkent): These two functions were added to fix crbug.com/695211 without
  // changing focus behavior. Once we fix crbug.com/690272, we can remove these
  // functions.
  // setSelectionDeprecated() returns true if didSetSelectionDeprecated() should
  // be called.
  bool setSelectionDeprecated(const SelectionInDOMTree&,
                              SetSelectionOptions = CloseTyping |
                                                    ClearTypingStyle,
                              TextGranularity = CharacterGranularity);
  void didSetSelectionDeprecated(
      SetSelectionOptions = CloseTyping | ClearTypingStyle,
      CursorAlignOnScroll = CursorAlignOnScroll::IfNeeded);

  // Call this after doing user-triggered selections to make it easy to delete
  // the frame you entirely selected.
  void selectFrameElementInParentIfFullySelected();

  bool contains(const LayoutPoint&);

  bool modify(EAlteration,
              SelectionDirection,
              TextGranularity,
              EUserTriggered = NotUserTriggered);
  enum VerticalDirection { DirectionUp, DirectionDown };
  bool modify(EAlteration, unsigned verticalDistance, VerticalDirection);

  // Moves the selection extent based on the selection granularity strategy.
  // This function does not allow the selection to collapse. If the new
  // extent is resolved to the same position as the current base, this
  // function will do nothing.
  void moveRangeSelectionExtent(const IntPoint&);
  void moveRangeSelection(const VisiblePosition& base,
                          const VisiblePosition& extent,
                          TextGranularity);

  TextGranularity granularity() const { return m_granularity; }

  // Returns true if specified layout block should paint caret. This function is
  // called during painting only.
  bool shouldPaintCaret(const LayoutBlock&) const;

  // Bounds of (possibly transformed) caret in absolute coords
  IntRect absoluteCaretBounds();

  void didChangeFocus();

  const SelectionInDOMTree& selectionInDOMTree() const;
  bool isDirectional() const { return selectionInDOMTree().isDirectional(); }

  void documentAttached(Document*);

  void didLayout();
  bool isAppearanceDirty() const;
  void commitAppearanceIfNeeded(LayoutView&);
  void setCaretVisible(bool caretIsVisible);
  void scheduleVisualUpdate() const;
  void scheduleVisualUpdateForPaintInvalidationIfNeeded() const;

  // Paint invalidation methods delegating to FrameCaret.
  void clearPreviousCaretVisualRect(const LayoutBlock&);
  void layoutBlockWillBeDestroyed(const LayoutBlock&);
  void updateStyleAndLayoutIfNeeded();
  void invalidatePaintIfNeeded(const LayoutBlock&,
                               const PaintInvalidatorContext&);

  void paintCaret(GraphicsContext&, const LayoutPoint&);

  // Used to suspend caret blinking while the mouse is down.
  void setCaretBlinkingSuspended(bool);
  bool isCaretBlinkingSuspended() const;

  // Focus
  void setFocused(bool);
  bool isFocused() const { return m_focused; }
  bool isFocusedAndActive() const;
  void pageActivationChanged();

  void setUseSecureKeyboardEntryWhenActive(bool);

  bool isHandleVisible() const;

  void updateSecureKeyboardEntryIfActive();

  // Returns true if a word is selected.
  bool selectWordAroundPosition(const VisiblePosition&);

#ifndef NDEBUG
  void showTreeForThis() const;
#endif

  void setFocusedNodeIfNeeded();
  void notifyLayoutObjectOfSelectionChange(EUserTriggered);

  String selectedHTMLForClipboard() const;
  String selectedText(const TextIteratorBehavior&) const;
  String selectedText() const;
  String selectedTextForClipboard() const;

  // The bounds are clipped to the viewport as this is what callers expect.
  LayoutRect bounds() const;
  LayoutRect unclippedBounds() const;

  HTMLFormElement* currentForm() const;

  // TODO(tkent): This function has a bug that scrolling doesn't work well in
  // a case of RangeSelection. crbug.com/443061
  void revealSelection(
      const ScrollAlignment& = ScrollAlignment::alignCenterIfNeeded,
      RevealExtentOption = DoNotRevealExtent);
  void setSelectionFromNone();

  void updateAppearance();
  bool shouldShowBlockCursor() const;
  void setShouldShowBlockCursor(bool);

  void cacheRangeOfDocument(Range*);
  Range* documentCachedRange() const;
  void clearDocumentCachedRange();

  FrameCaret& frameCaretForTesting() const { return *m_frameCaret; }

  DECLARE_TRACE();

 private:
  friend class CaretDisplayItemClientTest;
  friend class FrameSelectionTest;
  friend class PaintControllerPaintTestForSlimmingPaintV1AndV2;
  friend class SelectionControllerTest;
  FRIEND_TEST_ALL_PREFIXES(PaintControllerPaintTestForSlimmingPaintV1AndV2,
                           FullDocumentPaintingWithCaret);

  explicit FrameSelection(LocalFrame&);

  const DisplayItemClient& caretDisplayItemClientForTesting() const;

  // Note: We have |selectionInFlatTree()| for unit tests, we should
  // use |visibleSelection<EditingInFlatTreeStrategy>()|.
  const VisibleSelectionInFlatTree& selectionInFlatTree() const;

  void notifyAccessibilityForSelectionChange();
  void notifyCompositorForSelectionChange();
  void notifyEventHandlerForSelectionChange();

  void focusedOrActiveStateChanged();

  void setUseSecureKeyboardEntry(bool);

  void updateSelectionIfNeeded(const Position& base,
                               const Position& extent,
                               const Position& start,
                               const Position& end);

  GranularityStrategy* granularityStrategy();

  // Implementation of |SynchronousMutationObserver| member functions.
  void contextDestroyed(Document*) final;
  void nodeChildrenWillBeRemoved(ContainerNode&) final;
  void nodeWillBeRemoved(Node&) final;

  Member<LocalFrame> m_frame;
  const Member<PendingSelection> m_pendingSelection;
  const Member<SelectionEditor> m_selectionEditor;

  TextGranularity m_granularity;
  LayoutUnit m_xPosForVerticalArrowNavigation;

  bool m_focused : 1;

  // Controls text granularity used to adjust the selection's extent in
  // moveRangeSelectionExtent.
  std::unique_ptr<GranularityStrategy> m_granularityStrategy;

  const Member<FrameCaret> m_frameCaret;
  bool m_useSecureKeyboardEntryWhenActive = false;
};

}  // namespace blink

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
void showTree(const blink::FrameSelection&);
void showTree(const blink::FrameSelection*);
#endif

#endif  // FrameSelection_h
