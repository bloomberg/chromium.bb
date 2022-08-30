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

#include "third_party/blink/renderer/core/html/forms/text_control_inner_elements.h"

#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "third_party/blink/renderer/core/css/resolver/style_adjuster.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_object_factory.h"

namespace blink {

EditingViewPortElement::EditingViewPortElement(Document& document)
    : HTMLDivElement(document) {
  SetHasCustomStyleCallbacks();
  setAttribute(html_names::kIdAttr, shadow_element_names::kIdEditingViewPort);
}

scoped_refptr<ComputedStyle> EditingViewPortElement::CustomStyleForLayoutObject(
    const StyleRecalcContext&) {
  // FXIME: Move these styles to html.css.

  scoped_refptr<ComputedStyle> style =
      GetDocument().GetStyleResolver().CreateComputedStyle();
  style->InheritFrom(OwnerShadowHost()->ComputedStyleRef());

  style->SetFlexGrow(1);
  style->SetMinWidth(Length::Fixed(0));
  style->SetDisplay(EDisplay::kBlock);
  style->SetDirection(TextDirection::kLtr);

  // We don't want the shadow dom to be editable, so we set this block to
  // read-only in case the input itself is editable.
  style->SetUserModify(EUserModify::kReadOnly);

  return style;
}

// ---------------------------

TextControlInnerEditorElement::TextControlInnerEditorElement(Document& document)
    : HTMLDivElement(document) {
  SetHasCustomStyleCallbacks();
}

void TextControlInnerEditorElement::DefaultEventHandler(Event& event) {
  // FIXME: In the future, we should add a way to have default event listeners.
  // Then we would add one to the text field's inner div, and we wouldn't need
  // this subclass.
  // Or possibly we could just use a normal event listener.
  if (event.IsBeforeTextInsertedEvent() ||
      event.type() == event_type_names::kWebkitEditableContentChanged) {
    Element* shadow_ancestor = OwnerShadowHost();
    // A TextControlInnerTextElement can have no host if its been detached,
    // but kept alive by an EditCommand. In this case, an undo/redo can
    // cause events to be sent to the TextControlInnerTextElement. To
    // prevent an infinite loop, we must check for this case before sending
    // the event up the chain.
    if (shadow_ancestor)
      shadow_ancestor->DefaultEventHandler(event);
  }

  if (event.type() == event_type_names::kScroll) {
    // The scroller for a text control is inside of a shadow tree but the
    // scroll event won't bubble past the shadow root and authors cannot add
    // an event listener to it. Fire the scroll event at the shadow host so
    // that the page can hear about the scroll.
    Element* shadow_ancestor = OwnerShadowHost();
    if (shadow_ancestor)
      shadow_ancestor->DispatchEvent(event);
  }

  if (!event.DefaultHandled())
    HTMLDivElement::DefaultEventHandler(event);
}

void TextControlInnerEditorElement::SetVisibility(bool is_visible) {
  if (is_visible_ != is_visible) {
    is_visible_ = is_visible;
    SetNeedsStyleRecalc(kLocalStyleChange,
                        StyleChangeReasonForTracing::Create(
                            style_change_reason::kControlValue));
  }
}

void TextControlInnerEditorElement::FocusChanged() {
  // When the focus changes for the host element, we may need to recalc style
  // for text-overflow. See TextControlElement::ValueForTextOverflow().
  SetNeedsStyleRecalc(kLocalStyleChange, StyleChangeReasonForTracing::Create(
                                             style_change_reason::kControl));
}

LayoutObject* TextControlInnerEditorElement::CreateLayoutObject(
    const ComputedStyle& style,
    LegacyLayout legacy) {
  return LayoutObjectFactory::CreateTextControlInnerEditor(*this, style,
                                                           legacy);
}

scoped_refptr<ComputedStyle>
TextControlInnerEditorElement::CustomStyleForLayoutObject(
    const StyleRecalcContext&) {
  scoped_refptr<ComputedStyle> inner_editor_style = CreateInnerEditorStyle();
  // Using StyleAdjuster::adjustComputedStyle updates unwanted style. We'd like
  // to apply only editing-related and alignment-related.
  StyleAdjuster::AdjustStyleForEditing(*inner_editor_style);
  if (!is_visible_)
    inner_editor_style->SetOpacity(0);
  return inner_editor_style;
}

scoped_refptr<ComputedStyle>
TextControlInnerEditorElement::CreateInnerEditorStyle() const {
  Element* host = OwnerShadowHost();
  DCHECK(host);
  const ComputedStyle& start_style = host->ComputedStyleRef();
  scoped_refptr<ComputedStyle> text_block_style =
      GetDocument().GetStyleResolver().CreateComputedStyle();
  text_block_style->InheritFrom(start_style);
  // The inner block, if present, always has its direction set to LTR,
  // so we need to inherit the direction and unicode-bidi style from the
  // element.
  // TODO(https://crbug.com/1101564): The custom inheritance done here means we
  // need to mark for style recalc inside style recalc. See the workaround in
  // LayoutTextControl::StyleDidChange.
  text_block_style->SetDirection(start_style.Direction());
  text_block_style->SetUnicodeBidi(start_style.GetUnicodeBidi());
  text_block_style->SetUserSelect(EUserSelect::kText);
  text_block_style->SetUserModify(
      To<HTMLFormControlElement>(host)->IsDisabledOrReadOnly()
          ? EUserModify::kReadOnly
          : EUserModify::kReadWritePlaintextOnly);
  text_block_style->SetDisplay(EDisplay::kBlock);
  text_block_style->SetHasLineIfEmpty(true);
  text_block_style->SetShouldIgnoreOverflowPropertyForInlineBlockBaseline();

  if (!IsA<HTMLTextAreaElement>(host)) {
    text_block_style->SetWhiteSpace(EWhiteSpace::kPre);
    text_block_style->SetOverflowWrap(EOverflowWrap::kNormal);
    text_block_style->SetTextOverflow(
        ToTextControl(host)->ValueForTextOverflow());
    int computed_line_height = start_style.ComputedLineHeight();
    // Do not allow line-height to be smaller than our default.
    if (text_block_style->FontSize() >= computed_line_height) {
      text_block_style->SetLineHeight(
          ComputedStyleInitialValues::InitialLineHeight());
    }

    // We'd like to remove line-height if it's unnecessary because
    // overflow:scroll clips editing text by line-height.
    const Length& logical_height = start_style.LogicalHeight();
    // Here, we remove line-height if the INPUT fixed height is taller than the
    // line-height.  It's not the precise condition because logicalHeight
    // includes border and padding if box-sizing:border-box, and there are cases
    // in which we don't want to remove line-height with percent or calculated
    // length.
    // TODO(tkent): This should be done during layout.
    if (logical_height.IsPercentOrCalc() ||
        (logical_height.IsFixed() &&
         logical_height.GetFloatValue() > computed_line_height)) {
      text_block_style->SetLineHeight(
          ComputedStyleInitialValues::InitialLineHeight());
    }

    if (To<HTMLInputElement>(host)->ShouldRevealPassword())
      text_block_style->SetTextSecurity(ETextSecurity::kNone);

    text_block_style->SetOverflowX(EOverflow::kScroll);
    // overflow-y:visible doesn't work because overflow-x:scroll makes a layer.
    text_block_style->SetOverflowY(EOverflow::kScroll);
    scoped_refptr<ComputedStyle> no_scrollbar_style =
        GetDocument().GetStyleResolver().CreateComputedStyle();
    no_scrollbar_style->SetStyleType(kPseudoIdScrollbar);
    no_scrollbar_style->SetDisplay(EDisplay::kNone);
    text_block_style->AddCachedPseudoElementStyle(
        no_scrollbar_style, kPseudoIdScrollbar, g_null_atom);
    text_block_style->SetHasPseudoElementStyle(kPseudoIdScrollbar);

    text_block_style->SetDisplay(EDisplay::kFlowRoot);
    if (parentNode()->IsShadowRoot())
      text_block_style->SetAlignSelfBlockCenter(true);
  }

  return text_block_style;
}

// ----------------------------

SearchFieldCancelButtonElement::SearchFieldCancelButtonElement(
    Document& document)
    : HTMLDivElement(document) {
  SetShadowPseudoId(AtomicString("-webkit-search-cancel-button"));
  setAttribute(html_names::kIdAttr, shadow_element_names::kIdSearchClearButton);
}

void SearchFieldCancelButtonElement::DefaultEventHandler(Event& event) {
  // If the element is visible, on mouseup, clear the value, and set selection
  auto* mouse_event = DynamicTo<MouseEvent>(event);
  auto* input = To<HTMLInputElement>(OwnerShadowHost());
  if (!input || input->IsDisabledOrReadOnly()) {
    if (!event.DefaultHandled())
      HTMLDivElement::DefaultEventHandler(event);
    return;
  }

  if (event.type() == event_type_names::kClick && mouse_event &&
      mouse_event->button() ==
          static_cast<int16_t>(WebPointerProperties::Button::kLeft)) {
    input->SetValueForUser("");
    input->SetAutofillState(WebAutofillState::kNotFilled);
    input->OnSearch();
    event.SetDefaultHandled();
  }

  if (!event.DefaultHandled())
    HTMLDivElement::DefaultEventHandler(event);
}

bool SearchFieldCancelButtonElement::WillRespondToMouseClickEvents() {
  auto* input = To<HTMLInputElement>(OwnerShadowHost());
  if (input && !input->IsDisabledOrReadOnly())
    return true;

  return HTMLDivElement::WillRespondToMouseClickEvents();
}

// ----------------------------

PasswordRevealButtonElement::PasswordRevealButtonElement(Document& document)
    : HTMLDivElement(document) {
  SetShadowPseudoId(AtomicString("-internal-reveal"));
  setAttribute(html_names::kIdAttr,
               shadow_element_names::kIdPasswordRevealButton);
}

void PasswordRevealButtonElement::DefaultEventHandler(Event& event) {
  auto* input = To<HTMLInputElement>(OwnerShadowHost());
  if (!input || input->IsDisabledOrReadOnly()) {
    if (!event.DefaultHandled())
      HTMLDivElement::DefaultEventHandler(event);
    return;
  }

  // Toggle the should-reveal-password state when clicked.
  if (event.type() == event_type_names::kClick && IsA<MouseEvent>(event)) {
    bool shouldRevealPassword = !input->ShouldRevealPassword();

    input->SetShouldRevealPassword(shouldRevealPassword);
    input->UpdateView();

    event.SetDefaultHandled();
  }

  if (!event.DefaultHandled())
    HTMLDivElement::DefaultEventHandler(event);
}

bool PasswordRevealButtonElement::WillRespondToMouseClickEvents() {
  auto* input = To<HTMLInputElement>(OwnerShadowHost());
  if (input && !input->IsDisabledOrReadOnly())
    return true;

  return HTMLDivElement::WillRespondToMouseClickEvents();
}
}  // namespace blink
