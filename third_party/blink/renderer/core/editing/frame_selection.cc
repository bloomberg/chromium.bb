/*
 * Copyright (C) 2004, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/editing/frame_selection.h"

#include <stdio.h>
#include "third_party/blink/public/platform/web_scroll_into_view_params.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/dom/character_data.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/node_with_index.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/caret_display_item_client.h"
#include "third_party/blink/renderer/core/editing/commands/typing_command.h"
#include "third_party/blink/renderer/core/editing/editing_behavior.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_caret.h"
#include "third_party/blink/renderer/core/editing/granularity_strategy.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/layout_selection.h"
#include "third_party/blink/renderer/core/editing/local_caret_rect.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/selection_controller.h"
#include "third_party/blink/renderer/core/editing/selection_editor.h"
#include "third_party/blink/renderer/core/editing/selection_modifier.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/context_menu_allowed_scope.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/hit_test_request.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/spatial_navigation.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/geometry/float_quad.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/text/unicode_utilities.h"
#include "third_party/blink/renderer/platform/wtf/text/cstring.h"

#define EDIT_DEBUG 0

namespace blink {

using namespace HTMLNames;

static inline bool ShouldAlwaysUseDirectionalSelection(LocalFrame* frame) {
  return frame->GetEditor().Behavior().ShouldConsiderSelectionAsDirectional();
}

FrameSelection::FrameSelection(LocalFrame& frame)
    : frame_(frame),
      layout_selection_(LayoutSelection::Create(*this)),
      selection_editor_(SelectionEditor::Create(frame)),
      granularity_(TextGranularity::kCharacter),
      x_pos_for_vertical_arrow_navigation_(NoXPosForVerticalArrowNavigation()),
      focused_(frame.GetPage() &&
               frame.GetPage()->GetFocusController().FocusedFrame() == frame),
      is_directional_(ShouldAlwaysUseDirectionalSelection(frame_)),
      frame_caret_(new FrameCaret(frame, *selection_editor_)) {}

FrameSelection::~FrameSelection() = default;

const DisplayItemClient& FrameSelection::CaretDisplayItemClientForTesting()
    const {
  return frame_caret_->GetDisplayItemClient();
}

Document& FrameSelection::GetDocument() const {
  DCHECK(LifecycleContext());
  return *LifecycleContext();
}

VisibleSelection FrameSelection::ComputeVisibleSelectionInDOMTree() const {
  return selection_editor_->ComputeVisibleSelectionInDOMTree();
}

VisibleSelectionInFlatTree FrameSelection::ComputeVisibleSelectionInFlatTree()
    const {
  return selection_editor_->ComputeVisibleSelectionInFlatTree();
}

SelectionInDOMTree FrameSelection::GetSelectionInDOMTree() const {
  return selection_editor_->GetSelectionInDOMTree();
}

Element* FrameSelection::RootEditableElementOrDocumentElement() const {
  Element* selection_root =
      ComputeVisibleSelectionInDOMTreeDeprecated().RootEditableElement();
  return selection_root ? selection_root : GetDocument().documentElement();
}

size_t FrameSelection::CharacterIndexForPoint(const IntPoint& point) const {
  const EphemeralRange range = GetFrame()->GetEditor().RangeForPoint(point);
  if (range.IsNull())
    return kNotFound;
  Element* const editable = RootEditableElementOrDocumentElement();
  DCHECK(editable);
  return PlainTextRange::Create(*editable, range).Start();
}

VisibleSelection FrameSelection::ComputeVisibleSelectionInDOMTreeDeprecated()
    const {
  // TODO(editing-dev): Hoist updateStyleAndLayoutIgnorePendingStylesheets
  // to caller. See http://crbug.com/590369 for more details.
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();
  return ComputeVisibleSelectionInDOMTree();
}

VisibleSelectionInFlatTree FrameSelection::GetSelectionInFlatTree() const {
  return ComputeVisibleSelectionInFlatTree();
}

void FrameSelection::MoveCaretSelection(const IntPoint& point) {
  DCHECK(!GetDocument().NeedsLayoutTreeUpdate());

  Element* const editable =
      ComputeVisibleSelectionInDOMTree().RootEditableElement();
  if (!editable)
    return;

  const VisiblePosition position =
      VisiblePositionForContentsPoint(point, GetFrame());
  SelectionInDOMTree::Builder builder;
  if (position.IsNotNull())
    builder.Collapse(position.ToPositionWithAffinity());
  SetSelection(builder.Build(), SetSelectionOptions::Builder()
                                    .SetShouldCloseTyping(true)
                                    .SetShouldClearTypingStyle(true)
                                    .SetSetSelectionBy(SetSelectionBy::kUser)
                                    .SetShouldShowHandle(true)
                                    .SetIsDirectional(IsDirectional())
                                    .Build());
}

void FrameSelection::SetSelection(const SelectionInDOMTree& selection,
                                  const SetSelectionOptions& data) {
  if (SetSelectionDeprecated(selection, data))
    DidSetSelectionDeprecated(data);
}

void FrameSelection::SetSelectionAndEndTyping(
    const SelectionInDOMTree& selection) {
  SetSelection(selection, SetSelectionOptions::Builder()
                              .SetShouldCloseTyping(true)
                              .SetShouldClearTypingStyle(true)
                              .Build());
}

bool FrameSelection::SetSelectionDeprecated(
    const SelectionInDOMTree& new_selection,
    const SetSelectionOptions& passed_options) {
  SetSelectionOptions::Builder options_builder(passed_options);
  if (ShouldAlwaysUseDirectionalSelection(frame_)) {
    options_builder.SetIsDirectional(true);
  }
  SetSelectionOptions options = options_builder.Build();

  if (granularity_strategy_ && !options.DoNotClearStrategy())
    granularity_strategy_->Clear();
  granularity_ = options.Granularity();

  // TODO(yosin): We should move to call |TypingCommand::closeTyping()| to
  // |Editor| class.
  if (options.ShouldCloseTyping())
    TypingCommand::CloseTyping(frame_);

  if (options.ShouldClearTypingStyle())
    frame_->GetEditor().ClearTypingStyle();

  const SelectionInDOMTree old_selection_in_dom_tree =
      selection_editor_->GetSelectionInDOMTree();
  const bool is_changed = old_selection_in_dom_tree != new_selection;
  const bool should_show_handle = options.ShouldShowHandle();
  if (!is_changed && is_handle_visible_ == should_show_handle &&
      is_directional_ == options.IsDirectional())
    return false;
  if (is_changed)
    selection_editor_->SetSelectionAndEndTyping(new_selection);
  is_directional_ = options.IsDirectional();
  should_shrink_next_tap_ = options.ShouldShrinkNextTap();
  is_handle_visible_ = should_show_handle;
  ScheduleVisualUpdateForPaintInvalidationIfNeeded();

  const Document& current_document = GetDocument();
  frame_->GetEditor().RespondToChangedSelection();
  DCHECK_EQ(current_document, GetDocument());
  return true;
}

void FrameSelection::DidSetSelectionDeprecated(
    const SetSelectionOptions& options) {
  const Document& current_document = GetDocument();
  if (!GetSelectionInDOMTree().IsNone() && !options.DoNotSetFocus()) {
    SetFocusedNodeIfNeeded();
    // |setFocusedNodeIfNeeded()| dispatches sync events "FocusOut" and
    // "FocusIn", |frame_| may associate to another document.
    if (!IsAvailable() || GetDocument() != current_document) {
      // Once we get test case to reach here, we should change this
      // if-statement to |DCHECK()|.
      NOTREACHED();
      return;
    }
  }

  frame_caret_->StopCaretBlinkTimer();
  UpdateAppearance();

  // Always clear the x position used for vertical arrow navigation.
  // It will be restored by the vertical arrow navigation code if necessary.
  x_pos_for_vertical_arrow_navigation_ = NoXPosForVerticalArrowNavigation();

  // TODO(yosin): Can we move this to at end of this function?
  // This may dispatch a synchronous focus-related events.
  if (!options.DoNotSetFocus()) {
    SelectFrameElementInParentIfFullySelected();
    if (!IsAvailable() || GetDocument() != current_document) {
      // editing/selection/selectallchildren-crash.html and
      // editing/selection/longpress-selection-in-iframe-removed-crash.html
      // reach here.
      return;
    }
  }
  const SetSelectionBy set_selection_by = options.GetSetSelectionBy();
  NotifyTextControlOfSelectionChange(set_selection_by);
  if (set_selection_by == SetSelectionBy::kUser) {
    const CursorAlignOnScroll align = options.GetCursorAlignOnScroll();
    ScrollAlignment alignment;

    if (frame_->GetEditor()
            .Behavior()
            .ShouldCenterAlignWhenSelectionIsRevealed())
      alignment = (align == CursorAlignOnScroll::kAlways)
                      ? ScrollAlignment::kAlignCenterAlways
                      : ScrollAlignment::kAlignCenterIfNeeded;
    else
      alignment = (align == CursorAlignOnScroll::kAlways)
                      ? ScrollAlignment::kAlignTopAlways
                      : ScrollAlignment::kAlignToEdgeIfNeeded;

    RevealSelection(alignment, kRevealExtent);
  }

  NotifyAccessibilityForSelectionChange();
  NotifyCompositorForSelectionChange();
  NotifyEventHandlerForSelectionChange();
  // The task source should be kDOMManipulation, but the spec doesn't say
  // anything about this.
  frame_->DomWindow()->EnqueueDocumentEvent(
      *Event::Create(EventTypeNames::selectionchange),
      TaskType::kMiscPlatformAPI);
}

void FrameSelection::NodeChildrenWillBeRemoved(ContainerNode& container) {
  if (!container.InActiveDocument())
    return;
  // TODO(yosin): We should move to call |TypingCommand::closeTyping()| to
  // |Editor| class.
  if (!GetDocument().IsRunningExecCommand())
    TypingCommand::CloseTyping(frame_);
}

void FrameSelection::NodeWillBeRemoved(Node& node) {
  // There can't be a selection inside a fragment, so if a fragment's node is
  // being removed, the selection in the document that created the fragment
  // needs no adjustment.
  if (!node.InActiveDocument())
    return;
  // TODO(yosin): We should move to call |TypingCommand::closeTyping()| to
  // |Editor| class.
  if (!GetDocument().IsRunningExecCommand())
    TypingCommand::CloseTyping(frame_);
}

void FrameSelection::DidChangeFocus() {
  // Hits in
  // virtual/gpu/compositedscrolling/scrollbars/scrollbar-miss-mousemove-disabled.html
  DisableCompositingQueryAsserts disabler;
  UpdateAppearance();
}

static DispatchEventResult DispatchSelectStart(
    const VisibleSelection& selection) {
  Node* select_start_target = selection.Extent().ComputeContainerNode();
  if (!select_start_target)
    return DispatchEventResult::kNotCanceled;

  return select_start_target->DispatchEvent(
      *Event::CreateCancelableBubble(EventTypeNames::selectstart));
}

// The return value of |FrameSelection::modify()| is different based on
// value of |userTriggered| parameter.
// When |userTriggered| is |userTriggered|, |modify()| returns false if
// "selectstart" event is dispatched and canceled, otherwise returns true.
// When |userTriggered| is |NotUserTrigged|, return value specifies whether
// selection is modified or not.
bool FrameSelection::Modify(SelectionModifyAlteration alter,
                            SelectionModifyDirection direction,
                            TextGranularity granularity,
                            SetSelectionBy set_selection_by) {
  SelectionModifier selection_modifier(*GetFrame(), GetSelectionInDOMTree(),
                                       x_pos_for_vertical_arrow_navigation_);
  selection_modifier.SetSelectionIsDirectional(IsDirectional());
  const bool modified =
      selection_modifier.Modify(alter, direction, granularity);
  if (set_selection_by == SetSelectionBy::kUser &&
      selection_modifier.Selection().IsRange() &&
      ComputeVisibleSelectionInDOMTree().IsCaret() &&
      DispatchSelectStart(ComputeVisibleSelectionInDOMTree()) !=
          DispatchEventResult::kNotCanceled) {
    return false;
  }
  if (!modified) {
    if (set_selection_by == SetSelectionBy::kSystem)
      return false;
    // If spatial navigation enabled, focus navigator will move focus to
    // another element. See snav-input.html and snav-textarea.html
    if (IsSpatialNavigationEnabled(frame_))
      return false;
    // Even if selection isn't changed, we prevent to default action, e.g.
    // scroll window when caret is at end of content editable.
    return true;
  }

  // For MacOS only selection is directionless at the beginning.
  // Selection gets direction on extent.
  const bool selection_is_directional =
      alter == SelectionModifyAlteration::kExtend ||
      ShouldAlwaysUseDirectionalSelection(frame_);

  SetSelection(selection_modifier.Selection().AsSelection(),
               SetSelectionOptions::Builder()
                   .SetShouldCloseTyping(true)
                   .SetShouldClearTypingStyle(true)
                   .SetSetSelectionBy(set_selection_by)
                   .SetIsDirectional(selection_is_directional)
                   .Build());

  if (granularity == TextGranularity::kLine ||
      granularity == TextGranularity::kParagraph)
    x_pos_for_vertical_arrow_navigation_ =
        selection_modifier.XPosForVerticalArrowNavigation();

  if (set_selection_by == SetSelectionBy::kUser)
    granularity_ = TextGranularity::kCharacter;

  ScheduleVisualUpdateForPaintInvalidationIfNeeded();

  return true;
}

void FrameSelection::Clear() {
  granularity_ = TextGranularity::kCharacter;
  if (granularity_strategy_)
    granularity_strategy_->Clear();
  SetSelectionAndEndTyping(SelectionInDOMTree());
  is_handle_visible_ = false;
  is_directional_ = ShouldAlwaysUseDirectionalSelection(frame_);
}

bool FrameSelection::SelectionHasFocus() const {
  // TODO(editing-dev): Hoist UpdateStyleAndLayoutIgnorePendingStylesheets
  // to caller. See http://crbug.com/590369 for more details.
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();
  if (ComputeVisibleSelectionInFlatTree().IsNone())
    return false;
  const Node* current =
      ComputeVisibleSelectionInFlatTree().Start().ComputeContainerNode();
  if (!current)
    return false;

  // No focused element means document root has focus.
  Element* const focused_element = GetDocument().FocusedElement()
                                       ? GetDocument().FocusedElement()
                                       : GetDocument().documentElement();
  if (!focused_element)
    return false;

  if (focused_element->IsTextControl())
    return focused_element->ContainsIncludingHostElements(*current);

  // Selection has focus if it contains the focused element.
  const PositionInFlatTree& focused_position =
      PositionInFlatTree::FirstPositionInNode(*focused_element);
  if (ComputeVisibleSelectionInFlatTree().Start() <= focused_position &&
      ComputeVisibleSelectionInFlatTree().End() >= focused_position)
    return true;

  bool has_editable_style = HasEditableStyle(*current);
  do {
    // If the selection is within an editable sub tree and that sub tree
    // doesn't have focus, the selection doesn't have focus either.
    if (has_editable_style && !HasEditableStyle(*current))
      return false;

    // Selection has focus if its sub tree has focus.
    if (current == focused_element)
      return true;
    current = current->ParentOrShadowHostNode();
  } while (current);

  return false;
}

bool FrameSelection::IsHidden() const {
  if (SelectionHasFocus())
    return false;

  const Node* start =
      ComputeVisibleSelectionInDOMTree().Start().ComputeContainerNode();
  if (!start)
    return true;

  // The selection doesn't have focus, so hide everything but range selections.
  if (!GetSelectionInDOMTree().IsRange())
    return true;

  // Here we know we have an unfocused range selection. Let's say that
  // selection resides inside a text control. Since the selection doesn't have
  // focus neither does the text control. Meaning, if the selection indeed
  // resides inside a text control, it should be hidden.
  return EnclosingTextControl(start);
}

void FrameSelection::DocumentAttached(Document* document) {
  DCHECK(document);
  selection_editor_->DocumentAttached(document);
  SetContext(document);
}

void FrameSelection::ContextDestroyed(Document* document) {
  granularity_ = TextGranularity::kCharacter;

  layout_selection_->OnDocumentShutdown();

  frame_->GetEditor().ClearTypingStyle();
}

void FrameSelection::ClearPreviousCaretVisualRect(const LayoutBlock& block) {
  frame_caret_->ClearPreviousVisualRect(block);
}

void FrameSelection::LayoutBlockWillBeDestroyed(const LayoutBlock& block) {
  frame_caret_->LayoutBlockWillBeDestroyed(block);
}

void FrameSelection::UpdateStyleAndLayoutIfNeeded() {
  frame_caret_->UpdateStyleAndLayoutIfNeeded();
}

void FrameSelection::InvalidatePaint(const LayoutBlock& block,
                                     const PaintInvalidatorContext& context) {
  frame_caret_->InvalidatePaint(block, context);
}

bool FrameSelection::ShouldPaintCaret(const LayoutBlock& block) const {
  DCHECK_GE(GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kLayoutClean);
  bool result = frame_caret_->ShouldPaintCaret(block);
  DCHECK(!result ||
         (ComputeVisibleSelectionInDOMTree().IsCaret() &&
          IsEditablePosition(ComputeVisibleSelectionInDOMTree().Start())));
  return result;
}

IntRect FrameSelection::AbsoluteCaretBounds() const {
  DCHECK(ComputeVisibleSelectionInDOMTree().IsValidFor(*frame_->GetDocument()));
  return frame_caret_->AbsoluteCaretBounds();
}

bool FrameSelection::ComputeAbsoluteBounds(IntRect& anchor,
                                           IntRect& focus) const {
  if (!IsAvailable() || GetSelectionInDOMTree().IsNone())
    return false;

  // TODO(editing-dev): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  frame_->GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();
  if (ComputeVisibleSelectionInDOMTree().IsNone()) {
    // plugins/mouse-capture-inside-shadow.html reaches here.
    return false;
  }

  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      frame_->GetDocument()->Lifecycle());

  if (ComputeVisibleSelectionInDOMTree().IsCaret()) {
    anchor = focus = AbsoluteCaretBounds();
  } else {
    const EphemeralRange selected_range =
        ComputeVisibleSelectionInDOMTree().ToNormalizedEphemeralRange();
    if (selected_range.IsNull())
      return false;
    anchor = FirstRectForRange(EphemeralRange(selected_range.StartPosition()));
    focus = FirstRectForRange(EphemeralRange(selected_range.EndPosition()));
  }

  if (!ComputeVisibleSelectionInDOMTree().IsBaseFirst())
    std::swap(anchor, focus);
  return true;
}

void FrameSelection::PaintCaret(GraphicsContext& context,
                                const LayoutPoint& paint_offset) {
  frame_caret_->PaintCaret(context, paint_offset);
}

bool FrameSelection::Contains(const LayoutPoint& point) {
  if (!GetDocument().GetLayoutView())
    return false;

  // This is a workaround of the issue that we sometimes get null from
  // ComputeVisibleSelectionInDOMTree(), but non-null from flat tree.
  // By running this, in case we get null, we also set the cached result in flat
  // tree into null, so that this function can return false correctly.
  // See crbug.com/846527 for details.
  // TODO(editing-dev): Fix the inconsistency and then remove this call.
  ComputeVisibleSelectionInDOMTree();

  // Treat a collapsed selection like no selection.
  const VisibleSelectionInFlatTree& visible_selection =
      ComputeVisibleSelectionInFlatTree();
  if (!visible_selection.IsRange())
    return false;

  HitTestRequest request(HitTestRequest::kReadOnly | HitTestRequest::kActive);
  HitTestLocation location(point);
  HitTestResult result(request, location);
  GetDocument().GetLayoutView()->HitTest(location, result);
  Node* inner_node = result.InnerNode();
  if (!inner_node || !inner_node->GetLayoutObject())
    return false;

  const VisiblePositionInFlatTree& visible_pos =
      CreateVisiblePosition(FromPositionInDOMTree<EditingInFlatTreeStrategy>(
          inner_node->GetLayoutObject()->PositionForPoint(
              result.LocalPoint())));
  if (visible_pos.IsNull())
    return false;

  const VisiblePositionInFlatTree& visible_start =
      visible_selection.VisibleStart();
  const VisiblePositionInFlatTree& visible_end = visible_selection.VisibleEnd();
  if (visible_start.IsNull() || visible_end.IsNull())
    return false;

  const PositionInFlatTree& start = visible_start.DeepEquivalent();
  const PositionInFlatTree& end = visible_end.DeepEquivalent();
  const PositionInFlatTree& pos = visible_pos.DeepEquivalent();
  return start.CompareTo(pos) <= 0 && pos.CompareTo(end) <= 0;
}

// Workaround for the fact that it's hard to delete a frame.
// Call this after doing user-triggered selections to make it easy to delete the
// frame you entirely selected. Can't do this implicitly as part of every
// setSelection call because in some contexts it might not be good for the focus
// to move to another frame. So instead we call it from places where we are
// selecting with the mouse or the keyboard after setting the selection.
void FrameSelection::SelectFrameElementInParentIfFullySelected() {
  // Find the parent frame; if there is none, then we have nothing to do.
  Frame* parent = frame_->Tree().Parent();
  if (!parent)
    return;
  Page* page = frame_->GetPage();
  if (!page)
    return;

  // Check if the selection contains the entire frame contents; if not, then
  // there is nothing to do.
  if (GetSelectionInDOMTree().Type() != kRangeSelection) {
    return;
  }

  // TODO(editing-dev): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

  if (!IsStartOfDocument(ComputeVisibleSelectionInDOMTree().VisibleStart()))
    return;
  if (!IsEndOfDocument(ComputeVisibleSelectionInDOMTree().VisibleEnd()))
    return;

  // FIXME: This is not yet implemented for cross-process frame relationships.
  if (!parent->IsLocalFrame())
    return;

  // Get to the <iframe> or <frame> (or even <object>) element in the parent
  // frame.
  // FIXME: Doesn't work for OOPI.
  HTMLFrameOwnerElement* owner_element = frame_->DeprecatedLocalOwner();
  if (!owner_element)
    return;
  ContainerNode* owner_element_parent = owner_element->parentNode();
  if (!owner_element_parent)
    return;

  // TODO(editing-dev): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  owner_element_parent->GetDocument()
      .UpdateStyleAndLayoutIgnorePendingStylesheets();

  // This method's purpose is it to make it easier to select iframes (in order
  // to delete them).  Don't do anything if the iframe isn't deletable.
  if (!blink::HasEditableStyle(*owner_element_parent))
    return;

  // Create compute positions before and after the element.
  unsigned owner_element_node_index = owner_element->NodeIndex();
  VisiblePosition before_owner_element = CreateVisiblePosition(
      Position(owner_element_parent, owner_element_node_index));
  VisiblePosition after_owner_element = CreateVisiblePosition(
      Position(owner_element_parent, owner_element_node_index + 1),
      TextAffinity::kUpstreamIfPossible);

  SelectionInDOMTree::Builder builder;
  builder
      .SetBaseAndExtentDeprecated(before_owner_element.DeepEquivalent(),
                                  after_owner_element.DeepEquivalent())
      .SetAffinity(before_owner_element.Affinity());

  // Focus on the parent frame, and then select from before this element to
  // after.
  VisibleSelection new_selection = CreateVisibleSelection(builder.Build());
  // TODO(yosin): We should call |FocusController::setFocusedFrame()| before
  // |createVisibleSelection()|.
  page->GetFocusController().SetFocusedFrame(parent);
  // setFocusedFrame can dispatch synchronous focus/blur events.  The document
  // tree might be modified.
  if (!new_selection.IsNone() &&
      new_selection.IsValidFor(*(ToLocalFrame(parent)->GetDocument()))) {
    ToLocalFrame(parent)->Selection().SetSelectionAndEndTyping(
        new_selection.AsSelection());
  }
}

// Returns a shadow tree node for legacy shadow trees, a child of the
// ShadowRoot node for new shadow trees, or 0 for non-shadow trees.
static Node* NonBoundaryShadowTreeRootNode(const Position& position) {
  return position.AnchorNode() && !position.AnchorNode()->IsShadowRoot()
             ? position.AnchorNode()->NonBoundaryShadowTreeRootNode()
             : nullptr;
}

void FrameSelection::SelectAll(SetSelectionBy set_selection_by) {
  if (auto* select_element =
          ToHTMLSelectElementOrNull(GetDocument().FocusedElement())) {
    if (select_element->CanSelectAll()) {
      select_element->SelectAll();
      return;
    }
  }

  Node* root = nullptr;
  Node* select_start_target = nullptr;
  if (set_selection_by == SetSelectionBy::kUser && IsHidden()) {
    // Hidden selection appears as no selection to user, in which case user-
    // triggered SelectAll should act as if there is no selection.
    root = GetDocument().documentElement();
    select_start_target = GetDocument().body();
  } else if (ComputeVisibleSelectionInDOMTree().IsContentEditable()) {
    root = HighestEditableRoot(ComputeVisibleSelectionInDOMTree().Start());
    if (Node* shadow_root = NonBoundaryShadowTreeRootNode(
            ComputeVisibleSelectionInDOMTree().Start()))
      select_start_target = shadow_root->OwnerShadowHost();
    else
      select_start_target = root;
  } else {
    root = NonBoundaryShadowTreeRootNode(
        ComputeVisibleSelectionInDOMTree().Start());
    if (root) {
      select_start_target = root->OwnerShadowHost();
    } else {
      root = GetDocument().documentElement();
      select_start_target = GetDocument().body();
    }
  }
  if (!root || EditingIgnoresContent(*root))
    return;

  if (select_start_target) {
    const Document& expected_document = GetDocument();
    if (select_start_target->DispatchEvent(*Event::CreateCancelableBubble(
            EventTypeNames::selectstart)) != DispatchEventResult::kNotCanceled)
      return;
    // The frame may be detached due to selectstart event.
    if (!IsAvailable()) {
      // Reached by editing/selection/selectstart_detach_frame.html
      return;
    }
    // |root| may be detached due to selectstart event.
    if (!root->isConnected() || expected_document != root->GetDocument())
      return;
  }

  // TODO(editing-dev): Should we pass in set_selection_by?
  SetSelection(SelectionInDOMTree::Builder().SelectAllChildren(*root).Build(),
               SetSelectionOptions::Builder()
                   .SetShouldCloseTyping(true)
                   .SetShouldClearTypingStyle(true)
                   .SetShouldShowHandle(IsHandleVisible())
                   .Build());
  SelectFrameElementInParentIfFullySelected();
  // TODO(editing-dev): Should we pass in set_selection_by?
  NotifyTextControlOfSelectionChange(SetSelectionBy::kUser);
  if (IsHandleVisible()) {
    ContextMenuAllowedScope scope;
    frame_->GetEventHandler().ShowNonLocatedContextMenu(nullptr,
                                                        kMenuSourceTouch);
  }
}

void FrameSelection::SelectAll() {
  SelectAll(SetSelectionBy::kSystem);
}

// Implementation of |SVGTextControlElement::selectSubString()|
void FrameSelection::SelectSubString(const Element& element,
                                     int offset,
                                     int length) {
  // Find selection start
  VisiblePosition start = VisiblePosition::FirstPositionInNode(element);
  for (int i = 0; i < offset; ++i)
    start = NextPositionOf(start);
  if (start.IsNull())
    return;

  // Find selection end
  VisiblePosition end(start);
  for (int i = 0; i < length; ++i)
    end = NextPositionOf(end);
  if (end.IsNull())
    return;

  // TODO(editing-dev): We assume |start| and |end| are not null and we don't
  // known when |start| and |end| are null. Once we get a such case, we check
  // null for |start| and |end|.
  SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(start.DeepEquivalent(), end.DeepEquivalent())
          .SetAffinity(start.Affinity())
          .Build());
}

void FrameSelection::NotifyAccessibilityForSelectionChange() {
  if (GetSelectionInDOMTree().IsNone())
    return;
  AXObjectCache* cache = GetDocument().ExistingAXObjectCache();
  if (!cache)
    return;
  const Position& start = GetSelectionInDOMTree().ComputeStartPosition();
  cache->SelectionChanged(start.ComputeContainerNode());
}

void FrameSelection::NotifyCompositorForSelectionChange() {
  if (!RuntimeEnabledFeatures::CompositedSelectionUpdateEnabled())
    return;

  ScheduleVisualUpdate();
}

void FrameSelection::NotifyEventHandlerForSelectionChange() {
  frame_->GetEventHandler().GetSelectionController().NotifySelectionChanged();
}

void FrameSelection::FocusedOrActiveStateChanged() {
  bool active_and_focused = FrameIsFocusedAndActive();

  // Trigger style invalidation from the focused element. Even though
  // the focused element hasn't changed, the evaluation of focus pseudo
  // selectors are dependent on whether the frame is focused and active.
  if (Element* element = GetDocument().FocusedElement())
    element->FocusStateChanged();

  GetDocument().UpdateStyleAndLayoutTree();

  // Because LayoutObject::selectionBackgroundColor() and
  // LayoutObject::selectionForegroundColor() check if the frame is active,
  // we have to update places those colors were painted.
  auto* view = GetDocument().GetLayoutView();
  if (view)
    layout_selection_->InvalidatePaintForSelection();

  // Caret appears in the active frame.
  if (active_and_focused)
    SetSelectionFromNone();
  frame_caret_->SetCaretVisibility(active_and_focused
                                       ? CaretVisibility::kVisible
                                       : CaretVisibility::kHidden);

  // Update for caps lock state
  frame_->GetEventHandler().CapsLockStateMayHaveChanged();
}

void FrameSelection::PageActivationChanged() {
  FocusedOrActiveStateChanged();
}

void FrameSelection::SetFrameIsFocused(bool flag) {
  if (focused_ == flag)
    return;
  focused_ = flag;

  FocusedOrActiveStateChanged();
}

bool FrameSelection::FrameIsFocusedAndActive() const {
  return focused_ && frame_->GetPage() &&
         frame_->GetPage()->GetFocusController().IsActive();
}

void FrameSelection::CommitAppearanceIfNeeded() {
  return layout_selection_->Commit();
}

void FrameSelection::DidLayout() {
  UpdateAppearance();
}

void FrameSelection::UpdateAppearance() {
  DCHECK(frame_->ContentLayoutObject());
  frame_caret_->ScheduleVisualUpdateForPaintInvalidationIfNeeded();
  layout_selection_->SetHasPendingSelection();
}

void FrameSelection::NotifyTextControlOfSelectionChange(
    SetSelectionBy set_selection_by) {
  TextControlElement* text_control =
      EnclosingTextControl(GetSelectionInDOMTree().Base());
  if (!text_control)
    return;
  text_control->SelectionChanged(set_selection_by == SetSelectionBy::kUser);
}

// Helper function that tells whether a particular node is an element that has
// an entire LocalFrame and LocalFrameView, a <frame>, <iframe>, or <object>.
static bool IsFrameElement(const Node* n) {
  if (!n)
    return false;
  LayoutObject* layout_object = n->GetLayoutObject();
  if (!layout_object || !layout_object->IsLayoutEmbeddedContent())
    return false;
  return ToLayoutEmbeddedContent(layout_object)->ChildFrameView();
}

void FrameSelection::SetFocusedNodeIfNeeded() {
  if (ComputeVisibleSelectionInDOMTreeDeprecated().IsNone() ||
      !FrameIsFocused())
    return;

  if (Element* target =
          ComputeVisibleSelectionInDOMTreeDeprecated().RootEditableElement()) {
    // Walk up the DOM tree to search for a node to focus.
    GetDocument().UpdateStyleAndLayoutTreeIgnorePendingStylesheets();
    while (target) {
      // We don't want to set focus on a subframe when selecting in a parent
      // frame, so add the !isFrameElement check here. There's probably a better
      // way to make this work in the long term, but this is the safest fix at
      // this time.
      if (target->IsMouseFocusable() && !IsFrameElement(target)) {
        frame_->GetPage()->GetFocusController().SetFocusedElement(target,
                                                                  frame_);
        return;
      }
      target = target->ParentOrShadowHostElement();
    }
    GetDocument().ClearFocusedElement();
  }
}

static String ExtractSelectedText(const FrameSelection& selection,
                                  TextIteratorBehavior behavior) {
  const VisibleSelectionInFlatTree& visible_selection =
      selection.ComputeVisibleSelectionInFlatTree();
  const EphemeralRangeInFlatTree& range =
      visible_selection.ToNormalizedEphemeralRange();
  // We remove '\0' characters because they are not visibly rendered to the
  // user.
  return PlainText(range, behavior).Replace(0, "");
}

String FrameSelection::SelectedHTMLForClipboard() const {
  const VisibleSelectionInFlatTree& visible_selection =
      ComputeVisibleSelectionInFlatTree();
  const EphemeralRangeInFlatTree& range =
      visible_selection.ToNormalizedEphemeralRange();
  return CreateMarkup(
      range.StartPosition(), range.EndPosition(), kAnnotateForInterchange,
      ConvertBlocksToInlines::kNotConvert, kResolveNonLocalURLs);
}

String FrameSelection::SelectedText(
    const TextIteratorBehavior& behavior) const {
  return ExtractSelectedText(*this, behavior);
}

String FrameSelection::SelectedText() const {
  return SelectedText(TextIteratorBehavior());
}

String FrameSelection::SelectedTextForClipboard() const {
  return ExtractSelectedText(
      *this, TextIteratorBehavior::Builder()
                 .SetEmitsImageAltText(
                     frame_->GetSettings() &&
                     frame_->GetSettings()->GetSelectionIncludesAltImageText())
                 .SetSkipsUnselectableContent(true)
                 .Build());
}

LayoutRect FrameSelection::AbsoluteUnclippedBounds() const {
  LocalFrameView* view = frame_->View();
  LayoutView* layout_view = frame_->ContentLayoutObject();

  if (!view || !layout_view)
    return LayoutRect();

  view->UpdateLifecycleToLayoutClean();
  return LayoutRect(layout_selection_->AbsoluteSelectionBounds());
}

IntRect FrameSelection::ComputeRectToScroll(
    RevealExtentOption reveal_extent_option) {
  const VisibleSelection& selection = ComputeVisibleSelectionInDOMTree();
  if (selection.IsCaret())
    return AbsoluteCaretBounds();
  DCHECK(selection.IsRange());
  if (reveal_extent_option == kRevealExtent)
    return AbsoluteCaretBoundsOf(CreateVisiblePosition(selection.Extent()));
  layout_selection_->SetHasPendingSelection();
  return layout_selection_->AbsoluteSelectionBounds();
}

// TODO(editing-dev): This should be done in FlatTree world.
void FrameSelection::RevealSelection(const ScrollAlignment& alignment,
                                     RevealExtentOption reveal_extent_option) {
  DCHECK(IsAvailable());

  // TODO(editing-dev): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  // Calculation of absolute caret bounds requires clean layout.
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

  const VisibleSelection& selection = ComputeVisibleSelectionInDOMTree();
  if (selection.IsNone())
    return;

  // FIXME: This code only handles scrolling the startContainer's layer, but
  // the selection rect could intersect more than just that.
  if (DocumentLoader* document_loader = frame_->Loader().GetDocumentLoader())
    document_loader->GetInitialScrollState().was_scrolled_by_user = true;
  const Position& start = selection.Start();
  DCHECK(start.AnchorNode());
  DCHECK(start.AnchorNode()->GetLayoutObject());
  // This function is needed to make sure that ComputeRectToScroll below has the
  // sticky offset info available before the computation.
  GetDocument().EnsurePaintLocationDataValidForNode(start.AnchorNode());
  LayoutRect selection_rect =
      LayoutRect(ComputeRectToScroll(reveal_extent_option));
  if (selection_rect == LayoutRect() ||
      !start.AnchorNode()->GetLayoutObject()->EnclosingBox())
    return;

  start.AnchorNode()->GetLayoutObject()->ScrollRectToVisible(
      selection_rect, WebScrollIntoViewParams(alignment, alignment));
  UpdateAppearance();
}

void FrameSelection::SetSelectionFromNone() {
  // Put a caret inside the body if the entire frame is editable (either the
  // entire WebView is editable or designMode is on for this document).

  Document* document = frame_->GetDocument();
  if (!ComputeVisibleSelectionInDOMTreeDeprecated().IsNone() ||
      !(blink::HasEditableStyle(*document)))
    return;

  Element* document_element = document->documentElement();
  if (!document_element)
    return;
  if (HTMLBodyElement* body =
          Traversal<HTMLBodyElement>::FirstChild(*document_element)) {
    SetSelectionAndEndTyping(SelectionInDOMTree::Builder()
                                 .Collapse(FirstPositionInOrBeforeNode(*body))
                                 .Build());
  }
}

// TODO(yoichio): We should have LocalFrame having FrameCaret,
// Editor and PendingSelection using FrameCaret directly
// and get rid of this.
bool FrameSelection::ShouldShowBlockCursor() const {
  return frame_caret_->ShouldShowBlockCursor();
}

// TODO(yoichio): We should have LocalFrame having FrameCaret,
// Editor and PendingSelection using FrameCaret directly
// and get rid of this.
// TODO(yoichio): We should use "caret-shape" in "CSS Basic User Interface
// Module Level 4" https://drafts.csswg.org/css-ui-4/
// To use "caret-shape", we need to expose inserting mode information to CSS;
// https://github.com/w3c/csswg-drafts/issues/133
void FrameSelection::SetShouldShowBlockCursor(bool should_show_block_cursor) {
  frame_caret_->SetShouldShowBlockCursor(should_show_block_cursor);
}

#ifndef NDEBUG

void FrameSelection::ShowTreeForThis() const {
  ComputeVisibleSelectionInDOMTreeDeprecated().ShowTreeForThis();
}

#endif

void FrameSelection::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_);
  visitor->Trace(layout_selection_);
  visitor->Trace(selection_editor_);
  visitor->Trace(frame_caret_);
  SynchronousMutationObserver::Trace(visitor);
}

void FrameSelection::ScheduleVisualUpdate() const {
  if (Page* page = frame_->GetPage())
    page->Animator().ScheduleVisualUpdate(&frame_->LocalFrameRoot());
}

void FrameSelection::ScheduleVisualUpdateForPaintInvalidationIfNeeded() const {
  if (LocalFrameView* frame_view = frame_->View())
    frame_view->ScheduleVisualUpdateForPaintInvalidationIfNeeded();
}

bool FrameSelection::SelectWordAroundCaret() {
  const VisibleSelection& selection = ComputeVisibleSelectionInDOMTree();
  // TODO(editing-dev): The use of VisibleSelection needs to be audited. See
  // http://crbug.com/657237 for more details.
  if (!selection.IsCaret())
    return false;
  const VisiblePosition& position = selection.VisibleStart();
  static const EWordSide kWordSideList[2] = {kNextWordIfOnBoundary,
                                             kPreviousWordIfOnBoundary};
  for (EWordSide word_side : kWordSideList) {
    // TODO(yoichio): We should have Position version of |start/endOfWord|
    // for avoiding unnecessary canonicalization.
    VisiblePosition start = StartOfWord(position, word_side);
    VisiblePosition end = EndOfWord(position, word_side);

    // TODO(editing-dev): |StartOfWord()| and |EndOfWord()| should not make null
    // for non-null parameter.
    // See http://crbug.com/872443
    if (start.DeepEquivalent().IsNull() || end.DeepEquivalent().IsNull())
      continue;

    String text =
        PlainText(EphemeralRange(start.DeepEquivalent(), end.DeepEquivalent()));
    if (!text.IsEmpty() && !IsSeparator(text.CharacterStartingAt(0))) {
      SetSelection(SelectionInDOMTree::Builder()
                       .Collapse(start.ToPositionWithAffinity())
                       .Extend(end.DeepEquivalent())
                       .Build(),
                   SetSelectionOptions::Builder()
                       .SetShouldCloseTyping(true)
                       .SetShouldClearTypingStyle(true)
                       .SetGranularity(TextGranularity::kWord)
                       .Build());
      return true;
    }
  }

  return false;
}

GranularityStrategy* FrameSelection::GetGranularityStrategy() {
  // We do lazy initialization for granularity_strategy_, because if we
  // initialize it right in the constructor - the correct settings may not be
  // set yet.
  SelectionStrategy strategy_type = SelectionStrategy::kCharacter;
  Settings* settings = frame_ ? frame_->GetSettings() : nullptr;
  if (settings &&
      settings->GetSelectionStrategy() == SelectionStrategy::kDirection)
    strategy_type = SelectionStrategy::kDirection;

  if (granularity_strategy_ &&
      granularity_strategy_->GetType() == strategy_type)
    return granularity_strategy_.get();

  if (strategy_type == SelectionStrategy::kDirection)
    granularity_strategy_ = std::make_unique<DirectionGranularityStrategy>();
  else
    granularity_strategy_ = std::make_unique<CharacterGranularityStrategy>();
  return granularity_strategy_.get();
}

void FrameSelection::MoveRangeSelectionExtent(const IntPoint& contents_point) {
  if (ComputeVisibleSelectionInDOMTree().IsNone())
    return;

  SetSelection(
      SelectionInDOMTree::Builder(
          GetGranularityStrategy()->UpdateExtent(contents_point, frame_))
          .Build(),
      SetSelectionOptions::Builder()
          .SetShouldCloseTyping(true)
          .SetShouldClearTypingStyle(true)
          .SetDoNotClearStrategy(true)
          .SetSetSelectionBy(SetSelectionBy::kUser)
          .SetShouldShowHandle(true)
          .Build());
}

void FrameSelection::MoveRangeSelection(const IntPoint& base_point,
                                        const IntPoint& extent_point,
                                        TextGranularity granularity) {
  const VisiblePosition& base_position =
      VisiblePositionForContentsPoint(base_point, GetFrame());
  const VisiblePosition& extent_position =
      VisiblePositionForContentsPoint(extent_point, GetFrame());
  MoveRangeSelectionInternal(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtentDeprecated(base_position.DeepEquivalent(),
                                      extent_position.DeepEquivalent())
          .SetAffinity(base_position.Affinity())
          .Build(),
      granularity);
}

void FrameSelection::MoveRangeSelectionInternal(
    const SelectionInDOMTree& new_selection,
    TextGranularity granularity) {
  if (new_selection.IsNone())
    return;

  const VisibleSelection& visible_selection =
      CreateVisibleSelectionWithGranularity(new_selection, granularity);
  if (visible_selection.IsNone())
    return;

  SelectionInDOMTree::Builder builder;
  if (visible_selection.IsBaseFirst()) {
    builder.SetBaseAndExtent(visible_selection.Start(),
                             visible_selection.End());
  } else {
    builder.SetBaseAndExtent(visible_selection.End(),
                             visible_selection.Start());
  }
  builder.SetAffinity(visible_selection.Affinity());
  SetSelection(builder.Build(), SetSelectionOptions::Builder()
                                    .SetShouldCloseTyping(true)
                                    .SetShouldClearTypingStyle(true)
                                    .SetGranularity(granularity)
                                    .SetShouldShowHandle(IsHandleVisible())
                                    .Build());
}

void FrameSelection::SetCaretVisible(bool caret_is_visible) {
  frame_caret_->SetCaretVisibility(caret_is_visible ? CaretVisibility::kVisible
                                                    : CaretVisibility::kHidden);
}

void FrameSelection::SetCaretBlinkingSuspended(bool suspended) {
  frame_caret_->SetCaretBlinkingSuspended(suspended);
}

bool FrameSelection::IsCaretBlinkingSuspended() const {
  return frame_caret_->IsCaretBlinkingSuspended();
}

void FrameSelection::CacheRangeOfDocument(Range* range) {
  selection_editor_->CacheRangeOfDocument(range);
}

Range* FrameSelection::DocumentCachedRange() const {
  return selection_editor_->DocumentCachedRange();
}

void FrameSelection::ClearDocumentCachedRange() {
  selection_editor_->ClearDocumentCachedRange();
}

LayoutSelectionStatus FrameSelection::ComputeLayoutSelectionStatus(
    const NGPaintFragment& text_fragment) const {
  return layout_selection_->ComputeSelectionStatus(text_fragment);
}

bool FrameSelection::IsDirectional() const {
  return is_directional_;
}

}  // namespace blink

#ifndef NDEBUG

void showTree(const blink::FrameSelection& sel) {
  sel.ShowTreeForThis();
}

void showTree(const blink::FrameSelection* sel) {
  if (sel)
    sel->ShowTreeForThis();
  else
    LOG(INFO) << "Cannot showTree for <null> FrameSelection.";
}

#endif
