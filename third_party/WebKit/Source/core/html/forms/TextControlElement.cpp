/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "core/html/forms/TextControlElement.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/AXObjectCache.h"
#include "core/dom/Document.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/ShadowRoot.h"
#include "core/dom/Text.h"
#include "core/dom/events/Event.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/Position.h"
#include "core/editing/SelectionTemplate.h"
#include "core/editing/SetSelectionOptions.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/iterators/CharacterIterator.h"
#include "core/editing/iterators/TextIterator.h"
#include "core/editing/serializers/Serialization.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLBRElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/html_names.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutTheme.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

using namespace HTMLNames;

TextControlElement::TextControlElement(const QualifiedName& tag_name,
                                       Document& doc)
    : HTMLFormControlElementWithState(tag_name, doc),
      last_change_was_user_edit_(false),
      cached_selection_start_(0),
      cached_selection_end_(0) {
  cached_selection_direction_ =
      doc.GetFrame() && doc.GetFrame()
                            ->GetEditor()
                            .Behavior()
                            .ShouldConsiderSelectionAsDirectional()
          ? kSelectionHasForwardDirection
          : kSelectionHasNoDirection;
}

TextControlElement::~TextControlElement() {}

void TextControlElement::DispatchFocusEvent(
    Element* old_focused_element,
    WebFocusType type,
    InputDeviceCapabilities* source_capabilities) {
  if (SupportsPlaceholder())
    UpdatePlaceholderVisibility();
  HandleFocusEvent(old_focused_element, type);
  HTMLFormControlElementWithState::DispatchFocusEvent(old_focused_element, type,
                                                      source_capabilities);
}

void TextControlElement::DispatchBlurEvent(
    Element* new_focused_element,
    WebFocusType type,
    InputDeviceCapabilities* source_capabilities) {
  if (SupportsPlaceholder())
    UpdatePlaceholderVisibility();
  HandleBlurEvent();
  HTMLFormControlElementWithState::DispatchBlurEvent(new_focused_element, type,
                                                     source_capabilities);
}

void TextControlElement::DefaultEventHandler(Event* event) {
  if (event->type() == EventTypeNames::webkitEditableContentChanged &&
      GetLayoutObject() && GetLayoutObject()->IsTextControl()) {
    last_change_was_user_edit_ = !GetDocument().IsRunningExecCommand();

    if (IsFocused()) {
      // Updating the cache in SelectionChanged() isn't enough because
      // SelectionChanged() is not called if:
      // - Text nodes in the inner-editor is split to multiple, and
      // - The caret is on the beginning of a Text node, and its previous node
      //   is updated, or
      // - The caret is on the end of a text node, and its next node is updated.
      CacheSelection(ComputeSelectionStart(), ComputeSelectionEnd(),
                     ComputeSelectionDirection());
    }

    SubtreeHasChanged();
    return;
  }

  HTMLFormControlElementWithState::DefaultEventHandler(event);
}

void TextControlElement::ForwardEvent(Event* event) {
  if (event->type() == EventTypeNames::blur ||
      event->type() == EventTypeNames::focus)
    return;
  InnerEditorElement()->DefaultEventHandler(event);
}

String TextControlElement::StrippedPlaceholder() const {
  // According to the HTML5 specification, we need to remove CR and LF from
  // the attribute value.
  const AtomicString& attribute_value = FastGetAttribute(placeholderAttr);
  if (!attribute_value.Contains(kNewlineCharacter) &&
      !attribute_value.Contains(kCarriageReturnCharacter))
    return attribute_value;

  StringBuilder stripped;
  unsigned length = attribute_value.length();
  stripped.ReserveCapacity(length);
  for (unsigned i = 0; i < length; ++i) {
    UChar character = attribute_value[i];
    if (character == kNewlineCharacter || character == kCarriageReturnCharacter)
      continue;
    stripped.Append(character);
  }
  return stripped.ToString();
}

static bool IsNotLineBreak(UChar ch) {
  return ch != kNewlineCharacter && ch != kCarriageReturnCharacter;
}

bool TextControlElement::IsPlaceholderEmpty() const {
  const AtomicString& attribute_value = FastGetAttribute(placeholderAttr);
  return attribute_value.GetString().Find(IsNotLineBreak) == kNotFound;
}

bool TextControlElement::PlaceholderShouldBeVisible() const {
  return SupportsPlaceholder() && IsEmptyValue() && IsEmptySuggestedValue() &&
         !IsPlaceholderEmpty();
}

HTMLElement* TextControlElement::PlaceholderElement() const {
  return ToHTMLElementOrDie(
      UserAgentShadowRoot()->getElementById(ShadowElementNames::Placeholder()));
}

void TextControlElement::UpdatePlaceholderVisibility() {
  HTMLElement* placeholder = PlaceholderElement();
  if (!placeholder) {
    UpdatePlaceholderText();
    return;
  }

  bool place_holder_was_visible = IsPlaceholderVisible();
  SetPlaceholderVisibility(PlaceholderShouldBeVisible());
  if (place_holder_was_visible == IsPlaceholderVisible())
    return;

  PseudoStateChanged(CSSSelector::kPseudoPlaceholderShown);
  placeholder->SetInlineStyleProperty(
      CSSPropertyDisplay, IsPlaceholderVisible() ? CSSValueBlock : CSSValueNone,
      true);
}

void TextControlElement::setSelectionStart(unsigned start) {
  setSelectionRangeForBinding(start, std::max(start, selectionEnd()),
                              selectionDirection());
}

void TextControlElement::setSelectionEnd(unsigned end) {
  setSelectionRangeForBinding(std::min(end, selectionStart()), end,
                              selectionDirection());
}

void TextControlElement::setSelectionDirection(const String& direction) {
  setSelectionRangeForBinding(selectionStart(), selectionEnd(), direction);
}

void TextControlElement::select() {
  setSelectionRangeForBinding(0, std::numeric_limits<unsigned>::max());
  // Avoid SelectionBehaviorOnFocus::Restore, which scrolls containers to show
  // the selection.
  focus(
      FocusParams(SelectionBehaviorOnFocus::kNone, kWebFocusTypeNone, nullptr));
  RestoreCachedSelection();
}

void TextControlElement::SetValueBeforeFirstUserEditIfNotSet() {
  if (!value_before_first_user_edit_.IsNull())
    return;
  String value = this->value();
  value_before_first_user_edit_ = value.IsNull() ? g_empty_string : value;
}

void TextControlElement::CheckIfValueWasReverted(const String& value) {
  DCHECK(!value_before_first_user_edit_.IsNull())
      << "setValueBeforeFirstUserEditIfNotSet should be called beforehand.";
  String non_null_value = value.IsNull() ? g_empty_string : value;
  if (value_before_first_user_edit_ == non_null_value)
    ClearValueBeforeFirstUserEdit();
}

void TextControlElement::ClearValueBeforeFirstUserEdit() {
  value_before_first_user_edit_ = String();
}

void TextControlElement::SetFocused(bool flag, WebFocusType focus_type) {
  HTMLFormControlElementWithState::SetFocused(flag, focus_type);

  if (!flag)
    DispatchFormControlChangeEvent();
}

void TextControlElement::DispatchFormControlChangeEvent() {
  if (!value_before_first_user_edit_.IsNull() &&
      !EqualIgnoringNullity(value_before_first_user_edit_, value())) {
    ClearValueBeforeFirstUserEdit();
    DispatchChangeEvent();
  } else {
    ClearValueBeforeFirstUserEdit();
  }
}

void TextControlElement::EnqueueChangeEvent() {
  if (!value_before_first_user_edit_.IsNull() &&
      !EqualIgnoringNullity(value_before_first_user_edit_, value())) {
    Event* event = Event::CreateBubble(EventTypeNames::change);
    event->SetTarget(this);
    GetDocument().EnqueueAnimationFrameEvent(event);
  }
  ClearValueBeforeFirstUserEdit();
}

void TextControlElement::setRangeText(const String& replacement,
                                      ExceptionState& exception_state) {
  setRangeText(replacement, selectionStart(), selectionEnd(), "preserve",
               exception_state);
}

void TextControlElement::setRangeText(const String& replacement,
                                      unsigned start,
                                      unsigned end,
                                      const String& selection_mode,
                                      ExceptionState& exception_state) {
  if (start > end) {
    exception_state.ThrowDOMException(
        kIndexSizeError, "The provided start value (" + String::Number(start) +
                             ") is larger than the provided end value (" +
                             String::Number(end) + ").");
    return;
  }
  if (OpenShadowRoot())
    return;

  String text = InnerEditorValue();
  unsigned text_length = text.length();
  unsigned replacement_length = replacement.length();
  unsigned new_selection_start = selectionStart();
  unsigned new_selection_end = selectionEnd();

  start = std::min(start, text_length);
  end = std::min(end, text_length);

  if (start < end)
    text.replace(start, end - start, replacement);
  else
    text.insert(replacement, start);

  setValue(text, TextFieldEventBehavior::kDispatchNoEvent,
           TextControlSetValueSelection::kDoNotSet);

  if (selection_mode == "select") {
    new_selection_start = start;
    new_selection_end = start + replacement_length;
  } else if (selection_mode == "start") {
    new_selection_start = new_selection_end = start;
  } else if (selection_mode == "end") {
    new_selection_start = new_selection_end = start + replacement_length;
  } else {
    DCHECK_EQ(selection_mode, "preserve");
    long delta = replacement_length - (end - start);

    if (new_selection_start > end)
      new_selection_start += delta;
    else if (new_selection_start > start)
      new_selection_start = start;

    if (new_selection_end > end)
      new_selection_end += delta;
    else if (new_selection_end > start)
      new_selection_end = start + replacement_length;
  }

  setSelectionRangeForBinding(new_selection_start, new_selection_end);
}

void TextControlElement::setSelectionRangeForBinding(
    unsigned start,
    unsigned end,
    const String& direction_string) {
  TextFieldSelectionDirection direction = kSelectionHasNoDirection;
  if (direction_string == "forward")
    direction = kSelectionHasForwardDirection;
  else if (direction_string == "backward")
    direction = kSelectionHasBackwardDirection;
  if (SetSelectionRange(start, end, direction))
    ScheduleSelectEvent();
}

static Position PositionForIndex(HTMLElement* inner_editor, unsigned index) {
  if (index == 0) {
    Node* node = NodeTraversal::Next(*inner_editor, inner_editor);
    if (node && node->IsTextNode())
      return Position(node, 0);
    return Position(inner_editor, 0);
  }
  unsigned remaining_characters_to_move_forward = index;
  Node* last_br_or_text = inner_editor;
  for (Node& node : NodeTraversal::DescendantsOf(*inner_editor)) {
    if (node.HasTagName(brTag)) {
      if (remaining_characters_to_move_forward == 0)
        return Position::BeforeNode(node);
      --remaining_characters_to_move_forward;
      last_br_or_text = &node;
      continue;
    }

    if (node.IsTextNode()) {
      Text& text = ToText(node);
      if (remaining_characters_to_move_forward < text.length())
        return Position(&text, remaining_characters_to_move_forward);
      remaining_characters_to_move_forward -= text.length();
      last_br_or_text = &node;
      continue;
    }

    NOTREACHED();
  }
  DCHECK(last_br_or_text);
  return LastPositionInOrAfterNode(*last_br_or_text);
}

unsigned TextControlElement::IndexForPosition(HTMLElement* inner_editor,
                                              const Position& passed_position) {
  if (!inner_editor || !inner_editor->contains(passed_position.AnchorNode()) ||
      passed_position.IsNull())
    return 0;

  if (Position::BeforeNode(*inner_editor) == passed_position)
    return 0;

  unsigned index = 0;
  Node* start_node = passed_position.ComputeNodeBeforePosition();
  if (!start_node)
    start_node = passed_position.ComputeContainerNode();
  if (start_node == inner_editor && passed_position.IsAfterAnchor())
    start_node = inner_editor->lastChild();
  DCHECK(start_node);
  DCHECK(inner_editor->contains(start_node));

  for (Node* node = start_node; node;
       node = NodeTraversal::Previous(*node, inner_editor)) {
    if (node->IsTextNode()) {
      int length = ToText(*node).length();
      if (node == passed_position.ComputeContainerNode())
        index += std::min(length, passed_position.OffsetInContainerNode());
      else
        index += length;
    } else if (node->HasTagName(brTag)) {
      ++index;
    }
  }

  return index;
}

bool TextControlElement::SetSelectionRange(
    unsigned start,
    unsigned end,
    TextFieldSelectionDirection direction) {
  if (OpenShadowRoot() || !IsTextControl())
    return false;
  const unsigned editor_value_length = InnerEditorValue().length();
  end = std::min(end, editor_value_length);
  start = std::min(start, end);
  LocalFrame* frame = GetDocument().GetFrame();
  if (direction == kSelectionHasNoDirection && frame &&
      frame->GetEditor().Behavior().ShouldConsiderSelectionAsDirectional())
    direction = kSelectionHasForwardDirection;
  bool did_change = CacheSelection(start, end, direction);

  if (GetDocument().FocusedElement() != this)
    return did_change;

  HTMLElement* inner_editor = InnerEditorElement();
  if (!frame || !inner_editor)
    return did_change;

  Position start_position = PositionForIndex(inner_editor, start);
  Position end_position =
      start == end ? start_position : PositionForIndex(inner_editor, end);

  DCHECK_EQ(start, IndexForPosition(inner_editor, start_position));
  DCHECK_EQ(end, IndexForPosition(inner_editor, end_position));

#if DCHECK_IS_ON()
  // startPosition and endPosition can be null position for example when
  // "-webkit-user-select: none" style attribute is specified.
  if (start_position.IsNotNull() && end_position.IsNotNull()) {
    DCHECK_EQ(start_position.AnchorNode()->OwnerShadowHost(), this);
    DCHECK_EQ(end_position.AnchorNode()->OwnerShadowHost(), this);
  }
#endif  // DCHECK_IS_ON()
  frame->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .Collapse(direction == kSelectionHasBackwardDirection
                        ? end_position
                        : start_position)
          .Extend(direction == kSelectionHasBackwardDirection ? start_position
                                                              : end_position)
          .SetIsDirectional(direction != kSelectionHasNoDirection)
          .Build(),
      SetSelectionOptions::Builder()
          .SetShouldCloseTyping(true)
          .SetShouldClearTypingStyle(true)
          .SetDoNotSetFocus(true)
          .Build());
  return did_change;
}

bool TextControlElement::CacheSelection(unsigned start,
                                        unsigned end,
                                        TextFieldSelectionDirection direction) {
  DCHECK_LE(start, end);
  bool did_change = cached_selection_start_ != start ||
                    cached_selection_end_ != end ||
                    cached_selection_direction_ != direction;
  cached_selection_start_ = start;
  cached_selection_end_ = end;
  cached_selection_direction_ = direction;
  return did_change;
}

VisiblePosition TextControlElement::VisiblePositionForIndex(int index) const {
  if (index <= 0)
    return VisiblePosition::FirstPositionInNode(*InnerEditorElement());
  Position start, end;
  bool selected = Range::selectNodeContents(InnerEditorElement(), start, end);
  if (!selected)
    return VisiblePosition();
  CharacterIterator it(start, end);
  it.Advance(index - 1);
  return CreateVisiblePosition(it.EndPosition(), TextAffinity::kUpstream);
}

// TODO(yosin): We should move |TextControlElement::indexForVisiblePosition()|
// to "AXLayoutObject.cpp" since this funciton is used only there.
int TextControlElement::IndexForVisiblePosition(
    const VisiblePosition& pos) const {
  Position index_position = pos.DeepEquivalent().ParentAnchoredEquivalent();
  if (EnclosingTextControl(index_position) != this)
    return 0;
  DCHECK(index_position.IsConnected()) << index_position;
  return TextIterator::RangeLength(Position(InnerEditorElement(), 0),
                                   index_position);
}

unsigned TextControlElement::selectionStart() const {
  if (!IsTextControl())
    return 0;
  if (GetDocument().FocusedElement() != this)
    return cached_selection_start_;

  return ComputeSelectionStart();
}

unsigned TextControlElement::ComputeSelectionStart() const {
  DCHECK(IsTextControl());
  LocalFrame* frame = GetDocument().GetFrame();
  if (!frame)
    return 0;
  {
    // To avoid regression on speedometer benchmark[1] test, we should not
    // update layout tree in this code block.
    // [1] http://browserbench.org/Speedometer/
    DocumentLifecycle::DisallowTransitionScope disallow_transition(
        GetDocument().Lifecycle());
    const SelectionInDOMTree& selection =
        frame->Selection().GetSelectionInDOMTree();
    if (frame->Selection().Granularity() == TextGranularity::kCharacter) {
      return IndexForPosition(InnerEditorElement(),
                              selection.ComputeStartPosition());
    }
  }
  const VisibleSelection& visible_selection =
      frame->Selection().ComputeVisibleSelectionInDOMTreeDeprecated();
  return IndexForPosition(InnerEditorElement(), visible_selection.Start());
}

unsigned TextControlElement::selectionEnd() const {
  if (!IsTextControl())
    return 0;
  if (GetDocument().FocusedElement() != this)
    return cached_selection_end_;
  return ComputeSelectionEnd();
}

unsigned TextControlElement::ComputeSelectionEnd() const {
  DCHECK(IsTextControl());
  LocalFrame* frame = GetDocument().GetFrame();
  if (!frame)
    return 0;
  {
    // To avoid regression on speedometer benchmark[1] test, we should not
    // update layout tree in this code block.
    // [1] http://browserbench.org/Speedometer/
    DocumentLifecycle::DisallowTransitionScope disallow_transition(
        GetDocument().Lifecycle());
    const SelectionInDOMTree& selection =
        frame->Selection().GetSelectionInDOMTree();
    if (frame->Selection().Granularity() == TextGranularity::kCharacter) {
      return IndexForPosition(InnerEditorElement(),
                              selection.ComputeEndPosition());
    }
  }
  const VisibleSelection& visible_selection =
      frame->Selection().ComputeVisibleSelectionInDOMTreeDeprecated();
  return IndexForPosition(InnerEditorElement(), visible_selection.End());
}

static const AtomicString& DirectionString(
    TextFieldSelectionDirection direction) {
  DEFINE_STATIC_LOCAL(const AtomicString, none, ("none"));
  DEFINE_STATIC_LOCAL(const AtomicString, forward, ("forward"));
  DEFINE_STATIC_LOCAL(const AtomicString, backward, ("backward"));

  switch (direction) {
    case kSelectionHasNoDirection:
      return none;
    case kSelectionHasForwardDirection:
      return forward;
    case kSelectionHasBackwardDirection:
      return backward;
  }

  NOTREACHED();
  return none;
}

const AtomicString& TextControlElement::selectionDirection() const {
  // Ensured by HTMLInputElement::selectionDirectionForBinding().
  DCHECK(IsTextControl());
  if (GetDocument().FocusedElement() != this)
    return DirectionString(cached_selection_direction_);
  return DirectionString(ComputeSelectionDirection());
}

TextFieldSelectionDirection TextControlElement::ComputeSelectionDirection()
    const {
  DCHECK(IsTextControl());
  LocalFrame* frame = GetDocument().GetFrame();
  if (!frame)
    return kSelectionHasNoDirection;

  // To avoid regression on speedometer benchmark[1] test, we should not
  // update layout tree in this code block.
  // [1] http://browserbench.org/Speedometer/
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      GetDocument().Lifecycle());
  const SelectionInDOMTree& selection =
      frame->Selection().GetSelectionInDOMTree();
  const Position& start = selection.ComputeStartPosition();
  return frame->Selection().IsDirectional()
             ? (selection.Base() == start ? kSelectionHasForwardDirection
                                          : kSelectionHasBackwardDirection)
             : kSelectionHasNoDirection;
}

static inline void SetContainerAndOffsetForRange(Node* node,
                                                 int offset,
                                                 Node*& container_node,
                                                 int& offset_in_container) {
  if (node->IsTextNode()) {
    container_node = node;
    offset_in_container = offset;
  } else {
    container_node = node->parentNode();
    offset_in_container = node->NodeIndex() + offset;
  }
}

SelectionInDOMTree TextControlElement::Selection() const {
  if (!GetLayoutObject() || !IsTextControl())
    return SelectionInDOMTree();

  int start = cached_selection_start_;
  int end = cached_selection_end_;

  DCHECK_LE(start, end);
  HTMLElement* inner_text = InnerEditorElement();
  if (!inner_text)
    return SelectionInDOMTree();

  if (!inner_text->HasChildren()) {
    return SelectionInDOMTree::Builder()
        .Collapse(Position(inner_text, 0))
        .SetIsDirectional(selectionDirection() != "none")
        .Build();
  }

  int offset = 0;
  Node* start_node = 0;
  Node* end_node = 0;
  for (Node& node : NodeTraversal::DescendantsOf(*inner_text)) {
    DCHECK(!node.hasChildren());
    DCHECK(node.IsTextNode() || IsHTMLBRElement(node));
    int length = node.IsTextNode() ? Position::LastOffsetInNode(node) : 1;

    if (offset <= start && start <= offset + length)
      SetContainerAndOffsetForRange(&node, start - offset, start_node, start);

    if (offset <= end && end <= offset + length) {
      SetContainerAndOffsetForRange(&node, end - offset, end_node, end);
      break;
    }

    offset += length;
  }

  if (!start_node || !end_node)
    return SelectionInDOMTree();

  return SelectionInDOMTree::Builder()
      .SetBaseAndExtent(Position(start_node, start), Position(end_node, end))
      .SetIsDirectional(selectionDirection() != "none")
      .Build();
}

const AtomicString& TextControlElement::autocapitalize() const {
  DEFINE_STATIC_LOCAL(const AtomicString, off, ("off"));
  DEFINE_STATIC_LOCAL(const AtomicString, none, ("none"));
  DEFINE_STATIC_LOCAL(const AtomicString, characters, ("characters"));
  DEFINE_STATIC_LOCAL(const AtomicString, words, ("words"));
  DEFINE_STATIC_LOCAL(const AtomicString, sentences, ("sentences"));

  const AtomicString& value = FastGetAttribute(autocapitalizeAttr);
  if (DeprecatedEqualIgnoringCase(value, none) ||
      DeprecatedEqualIgnoringCase(value, off))
    return none;
  if (DeprecatedEqualIgnoringCase(value, characters))
    return characters;
  if (DeprecatedEqualIgnoringCase(value, words))
    return words;
  if (DeprecatedEqualIgnoringCase(value, sentences))
    return sentences;

  // Invalid or missing value.
  return DefaultAutocapitalize();
}

void TextControlElement::setAutocapitalize(const AtomicString& autocapitalize) {
  setAttribute(autocapitalizeAttr, autocapitalize);
}

int TextControlElement::maxLength() const {
  int value;
  if (!ParseHTMLInteger(FastGetAttribute(maxlengthAttr), value))
    return -1;
  return value >= 0 ? value : -1;
}

int TextControlElement::minLength() const {
  int value;
  if (!ParseHTMLInteger(FastGetAttribute(minlengthAttr), value))
    return -1;
  return value >= 0 ? value : -1;
}

void TextControlElement::setMaxLength(int new_value,
                                      ExceptionState& exception_state) {
  int min = minLength();
  if (new_value < 0) {
    exception_state.ThrowDOMException(
        kIndexSizeError, "The value provided (" + String::Number(new_value) +
                             ") is not positive or 0.");
  } else if (min >= 0 && new_value < min) {
    exception_state.ThrowDOMException(
        kIndexSizeError, ExceptionMessages::IndexExceedsMinimumBound(
                             "maxLength", new_value, min));
  } else {
    SetIntegralAttribute(maxlengthAttr, new_value);
  }
}

void TextControlElement::setMinLength(int new_value,
                                      ExceptionState& exception_state) {
  int max = maxLength();
  if (new_value < 0) {
    exception_state.ThrowDOMException(
        kIndexSizeError, "The value provided (" + String::Number(new_value) +
                             ") is not positive or 0.");
  } else if (max >= 0 && new_value > max) {
    exception_state.ThrowDOMException(
        kIndexSizeError, ExceptionMessages::IndexExceedsMaximumBound(
                             "minLength", new_value, max));
  } else {
    SetIntegralAttribute(minlengthAttr, new_value);
  }
}

void TextControlElement::RestoreCachedSelection() {
  if (SetSelectionRange(cached_selection_start_, cached_selection_end_,
                        cached_selection_direction_))
    ScheduleSelectEvent();
}

void TextControlElement::SelectionChanged(bool user_triggered) {
  if (!GetLayoutObject() || !IsTextControl())
    return;

  // selectionStart() or selectionEnd() will return cached selection when this
  // node doesn't have focus.
  CacheSelection(ComputeSelectionStart(), ComputeSelectionEnd(),
                 ComputeSelectionDirection());

  LocalFrame* frame = GetDocument().GetFrame();
  if (!frame || !user_triggered)
    return;
  const SelectionInDOMTree& selection =
      frame->Selection().GetSelectionInDOMTree();
  if (selection.Type() != kRangeSelection)
    return;
  DispatchEvent(Event::CreateBubble(EventTypeNames::select));
}

void TextControlElement::ScheduleSelectEvent() {
  Event* event = Event::CreateBubble(EventTypeNames::select);
  event->SetTarget(this);
  GetDocument().EnqueueAnimationFrameEvent(event);
}

void TextControlElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == autocapitalizeAttr)
    UseCounter::Count(GetDocument(), WebFeature::kAutocapitalizeAttribute);

  if (params.name == placeholderAttr) {
    UpdatePlaceholderText();
    UpdatePlaceholderVisibility();
    UseCounter::Count(GetDocument(), WebFeature::kPlaceholderAttribute);
  } else {
    HTMLFormControlElementWithState::ParseAttribute(params);
  }
}

bool TextControlElement::LastChangeWasUserEdit() const {
  if (!IsTextControl())
    return false;
  return last_change_was_user_edit_;
}

Node* TextControlElement::CreatePlaceholderBreakElement() const {
  return HTMLBRElement::Create(GetDocument());
}

void TextControlElement::AddPlaceholderBreakElementIfNecessary() {
  HTMLElement* inner_editor = InnerEditorElement();
  if (inner_editor->GetLayoutObject() &&
      !inner_editor->GetLayoutObject()->Style()->PreserveNewline())
    return;
  Node* last_child = inner_editor->lastChild();
  if (!last_child || !last_child->IsTextNode())
    return;
  if (ToText(last_child)->data().EndsWith('\n') ||
      ToText(last_child)->data().EndsWith('\r'))
    inner_editor->AppendChild(CreatePlaceholderBreakElement());
}

void TextControlElement::SetInnerEditorValue(const String& value) {
  DCHECK(!OpenShadowRoot());
  if (!IsTextControl() || OpenShadowRoot())
    return;

  bool text_is_changed = value != InnerEditorValue();
  HTMLElement* inner_editor = InnerEditorElement();
  if (!text_is_changed && inner_editor->HasChildren())
    return;

  // If the last child is a trailing <br> that's appended below, remove it
  // first so as to enable setInnerText() fast path of updating a text node.
  if (IsHTMLBRElement(inner_editor->lastChild()))
    inner_editor->RemoveChild(inner_editor->lastChild(), ASSERT_NO_EXCEPTION);

  // We don't use setTextContent.  It triggers unnecessary paint.
  if (value.IsEmpty())
    inner_editor->RemoveChildren();
  else
    ReplaceChildrenWithText(inner_editor, value, ASSERT_NO_EXCEPTION);

  // Add <br> so that we can put the caret at the next line of the last
  // newline.
  AddPlaceholderBreakElementIfNecessary();

  if (text_is_changed && GetLayoutObject()) {
    if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache())
      cache->HandleTextFormControlChanged(this);
  }
}

String TextControlElement::InnerEditorValue() const {
  DCHECK(!OpenShadowRoot());
  HTMLElement* inner_editor = InnerEditorElement();
  if (!inner_editor || !IsTextControl())
    return g_empty_string;

  // Typically, innerEditor has 0 or one Text node followed by 0 or one <br>.
  if (!inner_editor->HasChildren())
    return g_empty_string;
  Node& first_child = *inner_editor->firstChild();
  if (first_child.IsTextNode()) {
    Node* second_child = first_child.nextSibling();
    if (!second_child)
      return ToText(first_child).data();
    if (!second_child->nextSibling() && IsHTMLBRElement(*second_child))
      return ToText(first_child).data();
  } else if (!first_child.nextSibling() && IsHTMLBRElement(first_child)) {
    return g_empty_string;
  }

  StringBuilder result;
  for (Node& node : NodeTraversal::InclusiveDescendantsOf(*inner_editor)) {
    if (IsHTMLBRElement(node)) {
      DCHECK_EQ(&node, inner_editor->lastChild());
      if (&node != inner_editor->lastChild())
        result.Append(kNewlineCharacter);
    } else if (node.IsTextNode()) {
      result.Append(ToText(node).data());
    }
  }
  return result.ToString();
}

static void GetNextSoftBreak(RootInlineBox*& line,
                             Node*& break_node,
                             unsigned& break_offset) {
  RootInlineBox* next;
  for (; line; line = next) {
    next = line->NextRootBox();
    if (next && !line->EndsWithBreak()) {
      DCHECK(line->LineBreakObj());
      break_node = line->LineBreakObj().GetNode();
      break_offset = line->LineBreakPos();
      line = next;
      return;
    }
  }
  break_node = 0;
  break_offset = 0;
}

String TextControlElement::ValueWithHardLineBreaks() const {
  // FIXME: It's not acceptable to ignore the HardWrap setting when there is no
  // layoutObject.  While we have no evidence this has ever been a practical
  // problem, it would be best to fix it some day.
  HTMLElement* inner_text = InnerEditorElement();
  if (!inner_text || !IsTextControl())
    return value();

  LayoutBlockFlow* layout_object =
      ToLayoutBlockFlow(inner_text->GetLayoutObject());
  if (!layout_object)
    return value();

  Node* break_node;
  unsigned break_offset;
  RootInlineBox* line = layout_object->FirstRootBox();
  if (!line)
    return value();

  GetNextSoftBreak(line, break_node, break_offset);

  StringBuilder result;
  for (Node& node : NodeTraversal::DescendantsOf(*inner_text)) {
    if (IsHTMLBRElement(node)) {
      DCHECK_EQ(&node, inner_text->lastChild());
      if (&node != inner_text->lastChild())
        result.Append(kNewlineCharacter);
    } else if (node.IsTextNode()) {
      String data = ToText(node).data();
      unsigned length = data.length();
      unsigned position = 0;
      while (break_node == node && break_offset <= length) {
        if (break_offset > position) {
          result.Append(data, position, break_offset - position);
          position = break_offset;
          result.Append(kNewlineCharacter);
        }
        GetNextSoftBreak(line, break_node, break_offset);
      }
      result.Append(data, position, length - position);
    }
    while (break_node == node)
      GetNextSoftBreak(line, break_node, break_offset);
  }
  return result.ToString();
}

TextControlElement* EnclosingTextControl(const Position& position) {
  DCHECK(position.IsNull() || position.IsOffsetInAnchor() ||
         position.ComputeContainerNode() ||
         !position.AnchorNode()->OwnerShadowHost() ||
         (position.AnchorNode()->parentNode() &&
          position.AnchorNode()->parentNode()->IsShadowRoot()));
  return EnclosingTextControl(position.ComputeContainerNode());
}

TextControlElement* EnclosingTextControl(const Node* container) {
  if (!container)
    return nullptr;
  Element* ancestor = container->OwnerShadowHost();
  return ancestor && IsTextControlElement(*ancestor) &&
                 container->ContainingShadowRoot()->GetType() ==
                     ShadowRootType::kUserAgent
             ? ToTextControlElement(ancestor)
             : 0;
}

String TextControlElement::DirectionForFormData() const {
  for (const HTMLElement* element = this; element;
       element = Traversal<HTMLElement>::FirstAncestor(*element)) {
    const AtomicString& dir_attribute_value =
        element->FastGetAttribute(dirAttr);
    if (dir_attribute_value.IsNull())
      continue;

    if (DeprecatedEqualIgnoringCase(dir_attribute_value, "rtl") ||
        DeprecatedEqualIgnoringCase(dir_attribute_value, "ltr"))
      return dir_attribute_value;

    if (DeprecatedEqualIgnoringCase(dir_attribute_value, "auto")) {
      bool is_auto;
      TextDirection text_direction =
          element->DirectionalityIfhasDirAutoAttribute(is_auto);
      return text_direction == TextDirection::kRtl ? "rtl" : "ltr";
    }
  }

  return "ltr";
}

HTMLElement* TextControlElement::InnerEditorElement() const {
  return ToHTMLElementOrDie(
      UserAgentShadowRoot()->getElementById(ShadowElementNames::InnerEditor()));
}

void TextControlElement::CopyNonAttributePropertiesFromElement(
    const Element& source) {
  const TextControlElement& source_element =
      static_cast<const TextControlElement&>(source);
  last_change_was_user_edit_ = source_element.last_change_was_user_edit_;
  HTMLFormControlElement::CopyNonAttributePropertiesFromElement(source);
}

}  // namespace blink
