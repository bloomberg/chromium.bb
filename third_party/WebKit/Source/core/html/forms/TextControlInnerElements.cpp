/*
 * Copyright (C) 2006, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "core/html/forms/TextControlInnerElements.h"

#include "core/css/StyleChangeReason.h"
#include "core/css/resolver/StyleAdjuster.h"
#include "core/dom/Document.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/events/MouseEvent.h"
#include "core/events/TextEvent.h"
#include "core/events/TextEventInputType.h"
#include "core/frame/LocalFrame.h"
#include "core/html/forms/HTMLInputElement.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/html_names.h"
#include "core/input/EventHandler.h"
#include "core/layout/LayoutTextControlSingleLine.h"

namespace blink {

using namespace HTMLNames;

TextControlInnerContainer::TextControlInnerContainer(Document& document)
    : HTMLDivElement(document) {}

TextControlInnerContainer* TextControlInnerContainer::Create(
    Document& document) {
  TextControlInnerContainer* element = new TextControlInnerContainer(document);
  element->setAttribute(idAttr, ShadowElementNames::TextFieldContainer());
  return element;
}

LayoutObject* TextControlInnerContainer::CreateLayoutObject(
    const ComputedStyle&) {
  return new LayoutTextControlInnerContainer(this);
}

// ---------------------------

EditingViewPortElement::EditingViewPortElement(Document& document)
    : HTMLDivElement(document) {
  SetHasCustomStyleCallbacks();
}

EditingViewPortElement* EditingViewPortElement::Create(Document& document) {
  EditingViewPortElement* element = new EditingViewPortElement(document);
  element->setAttribute(idAttr, ShadowElementNames::EditingViewPort());
  return element;
}

scoped_refptr<ComputedStyle>
EditingViewPortElement::CustomStyleForLayoutObject() {
  // FXIME: Move these styles to html.css.

  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  style->InheritFrom(OwnerShadowHost()->ComputedStyleRef());

  style->SetFlexGrow(1);
  style->SetMinWidth(Length(0, kFixed));
  style->SetDisplay(EDisplay::kBlock);
  style->SetDirection(TextDirection::kLtr);

  // We don't want the shadow dom to be editable, so we set this block to
  // read-only in case the input itself is editable.
  style->SetUserModify(EUserModify::kReadOnly);
  style->SetUnique();

  return style;
}

// ---------------------------

inline TextControlInnerEditorElement::TextControlInnerEditorElement(
    Document& document)
    : HTMLDivElement(document) {
  SetHasCustomStyleCallbacks();
}

TextControlInnerEditorElement* TextControlInnerEditorElement::Create(
    Document& document) {
  return new TextControlInnerEditorElement(document);
}

void TextControlInnerEditorElement::DefaultEventHandler(Event* event) {
  // FIXME: In the future, we should add a way to have default event listeners.
  // Then we would add one to the text field's inner div, and we wouldn't need
  // this subclass.
  // Or possibly we could just use a normal event listener.
  if (event->IsBeforeTextInsertedEvent() ||
      event->type() == EventTypeNames::webkitEditableContentChanged) {
    Element* shadow_ancestor = OwnerShadowHost();
    // A TextControlInnerTextElement can have no host if its been detached,
    // but kept alive by an EditCommand. In this case, an undo/redo can
    // cause events to be sent to the TextControlInnerTextElement. To
    // prevent an infinite loop, we must check for this case before sending
    // the event up the chain.
    if (shadow_ancestor)
      shadow_ancestor->DefaultEventHandler(event);
  }
  if (!event->DefaultHandled())
    HTMLDivElement::DefaultEventHandler(event);
}

void TextControlInnerEditorElement::SetVisibility(bool is_visible) {
  if (is_visible_ != is_visible) {
    is_visible_ = is_visible;
    SetNeedsStyleRecalc(
        kLocalStyleChange,
        StyleChangeReasonForTracing::Create(StyleChangeReason::kControlValue));
  }
}

LayoutObject* TextControlInnerEditorElement::CreateLayoutObject(
    const ComputedStyle&) {
  return new LayoutTextControlInnerEditor(this);
}

scoped_refptr<ComputedStyle>
TextControlInnerEditorElement::CustomStyleForLayoutObject() {
  LayoutObject* parent_layout_object = OwnerShadowHost()->GetLayoutObject();
  if (!parent_layout_object || !parent_layout_object->IsTextControl())
    return OriginalStyleForLayoutObject();
  LayoutTextControl* text_control = ToLayoutTextControl(parent_layout_object);
  scoped_refptr<ComputedStyle> inner_editor_style =
      text_control->CreateInnerEditorStyle(text_control->StyleRef());
  // Using StyleAdjuster::adjustComputedStyle updates unwanted style. We'd like
  // to apply only editing-related and alignment-related.
  StyleAdjuster::AdjustStyleForEditing(*inner_editor_style);
  if (!is_visible_)
    inner_editor_style->SetOpacity(0);
  return inner_editor_style;
}

// ----------------------------

inline SearchFieldCancelButtonElement::SearchFieldCancelButtonElement(
    Document& document)
    : HTMLDivElement(document), capturing_(false) {}

SearchFieldCancelButtonElement* SearchFieldCancelButtonElement::Create(
    Document& document) {
  SearchFieldCancelButtonElement* element =
      new SearchFieldCancelButtonElement(document);
  element->SetShadowPseudoId(AtomicString("-webkit-search-cancel-button"));
  element->setAttribute(idAttr, ShadowElementNames::SearchClearButton());
  return element;
}

void SearchFieldCancelButtonElement::DetachLayoutTree(
    const AttachContext& context) {
  if (capturing_) {
    if (LocalFrame* frame = GetDocument().GetFrame())
      frame->GetEventHandler().SetCapturingMouseEventsNode(nullptr);
  }
  HTMLDivElement::DetachLayoutTree(context);
}

void SearchFieldCancelButtonElement::DefaultEventHandler(Event* event) {
  // If the element is visible, on mouseup, clear the value, and set selection
  HTMLInputElement* input(ToHTMLInputElement(OwnerShadowHost()));
  if (!input || input->IsDisabledOrReadOnly()) {
    if (!event->DefaultHandled())
      HTMLDivElement::DefaultEventHandler(event);
    return;
  }

  if (event->type() == EventTypeNames::click && event->IsMouseEvent() &&
      ToMouseEvent(event)->button() ==
          static_cast<short>(WebPointerProperties::Button::kLeft)) {
    input->SetValueForUser("");
    input->SetAutofilled(false);
    input->OnSearch();
    event->SetDefaultHandled();
  }

  if (!event->DefaultHandled())
    HTMLDivElement::DefaultEventHandler(event);
}

bool SearchFieldCancelButtonElement::WillRespondToMouseClickEvents() {
  const HTMLInputElement* input = ToHTMLInputElement(OwnerShadowHost());
  if (input && !input->IsDisabledOrReadOnly())
    return true;

  return HTMLDivElement::WillRespondToMouseClickEvents();
}

}  // namespace blink
