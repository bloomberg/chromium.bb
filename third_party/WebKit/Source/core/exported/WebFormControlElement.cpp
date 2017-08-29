/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "public/web/WebFormControlElement.h"

#include "core/dom/NodeComputedStyle.h"
#include "core/dom/events/Event.h"
#include "core/html/HTMLFormControlElement.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLSelectElement.h"
#include "core/html/HTMLTextAreaElement.h"

#include "platform/wtf/RefPtr.h"

namespace blink {

bool WebFormControlElement::IsEnabled() const {
  return !ConstUnwrap<HTMLFormControlElement>()->IsDisabledFormControl();
}

bool WebFormControlElement::IsReadOnly() const {
  return ConstUnwrap<HTMLFormControlElement>()->IsReadOnly();
}

WebString WebFormControlElement::FormControlName() const {
  return ConstUnwrap<HTMLFormControlElement>()->GetName();
}

WebString WebFormControlElement::FormControlType() const {
  return ConstUnwrap<HTMLFormControlElement>()->type();
}

bool WebFormControlElement::IsAutofilled() const {
  return ConstUnwrap<HTMLFormControlElement>()->IsAutofilled();
}

void WebFormControlElement::SetAutofilled(bool autofilled) {
  Unwrap<HTMLFormControlElement>()->SetAutofilled(autofilled);
}

WebString WebFormControlElement::NameForAutofill() const {
  return ConstUnwrap<HTMLFormControlElement>()->NameForAutofill();
}

bool WebFormControlElement::AutoComplete() const {
  if (isHTMLInputElement(*private_))
    return ConstUnwrap<HTMLInputElement>()->ShouldAutocomplete();
  if (isHTMLTextAreaElement(*private_))
    return ConstUnwrap<HTMLTextAreaElement>()->ShouldAutocomplete();
  if (isHTMLSelectElement(*private_))
    return ConstUnwrap<HTMLSelectElement>()->ShouldAutocomplete();
  return false;
}

void WebFormControlElement::SetValue(const WebString& value, bool send_events) {
  if (isHTMLInputElement(*private_)) {
    Unwrap<HTMLInputElement>()->setValue(
        value, send_events ? kDispatchInputAndChangeEvent : kDispatchNoEvent);
  } else if (isHTMLTextAreaElement(*private_)) {
    Unwrap<HTMLTextAreaElement>()->setValue(
        value, send_events ? kDispatchInputAndChangeEvent : kDispatchNoEvent);
  } else if (isHTMLSelectElement(*private_)) {
    Unwrap<HTMLSelectElement>()->setValue(value, send_events);
  }
}

void WebFormControlElement::SetAutofillValue(const WebString& value) {
  // The input and change events will be sent in setValue.
  if (isHTMLInputElement(*private_) || isHTMLTextAreaElement(*private_)) {
    if (!Focused()) {
      Unwrap<Element>()->DispatchFocusEvent(nullptr, kWebFocusTypeForward,
                                            nullptr);
    }
    Unwrap<Element>()->DispatchScopedEvent(
        Event::CreateBubble(EventTypeNames::keydown));
    Unwrap<TextControlElement>()->setValue(value, kDispatchInputAndChangeEvent);
    Unwrap<Element>()->DispatchScopedEvent(
        Event::CreateBubble(EventTypeNames::keyup));
    if (!Focused()) {
      Unwrap<Element>()->DispatchBlurEvent(nullptr, kWebFocusTypeForward,
                                           nullptr);
    }
  } else if (isHTMLSelectElement(*private_)) {
    if (!Focused()) {
      Unwrap<Element>()->DispatchFocusEvent(nullptr, kWebFocusTypeForward,
                                            nullptr);
    }
    Unwrap<HTMLSelectElement>()->setValue(value, true);
    if (!Focused()) {
      Unwrap<Element>()->DispatchBlurEvent(nullptr, kWebFocusTypeForward,
                                           nullptr);
    }
  }
}

WebString WebFormControlElement::Value() const {
  if (isHTMLInputElement(*private_))
    return ConstUnwrap<HTMLInputElement>()->value();
  if (isHTMLTextAreaElement(*private_))
    return ConstUnwrap<HTMLTextAreaElement>()->value();
  if (isHTMLSelectElement(*private_))
    return ConstUnwrap<HTMLSelectElement>()->value();
  return WebString();
}

void WebFormControlElement::SetSuggestedValue(const WebString& value) {
  if (isHTMLInputElement(*private_))
    Unwrap<HTMLInputElement>()->SetSuggestedValue(value);
  else if (isHTMLTextAreaElement(*private_))
    Unwrap<HTMLTextAreaElement>()->SetSuggestedValue(value);
  else if (isHTMLSelectElement(*private_))
    Unwrap<HTMLSelectElement>()->SetSuggestedValue(value);
}

WebString WebFormControlElement::SuggestedValue() const {
  if (isHTMLInputElement(*private_))
    return ConstUnwrap<HTMLInputElement>()->SuggestedValue();
  if (isHTMLTextAreaElement(*private_))
    return ConstUnwrap<HTMLTextAreaElement>()->SuggestedValue();
  if (isHTMLSelectElement(*private_))
    return ConstUnwrap<HTMLSelectElement>()->SuggestedValue();
  return WebString();
}

WebString WebFormControlElement::EditingValue() const {
  if (isHTMLInputElement(*private_))
    return ConstUnwrap<HTMLInputElement>()->InnerEditorValue();
  if (isHTMLTextAreaElement(*private_))
    return ConstUnwrap<HTMLTextAreaElement>()->InnerEditorValue();
  return WebString();
}

void WebFormControlElement::SetSelectionRange(int start, int end) {
  if (isHTMLInputElement(*private_))
    Unwrap<HTMLInputElement>()->SetSelectionRange(start, end);
  else if (isHTMLTextAreaElement(*private_))
    Unwrap<HTMLTextAreaElement>()->SetSelectionRange(start, end);
}

int WebFormControlElement::SelectionStart() const {
  if (isHTMLInputElement(*private_))
    return ConstUnwrap<HTMLInputElement>()->selectionStart();
  if (isHTMLTextAreaElement(*private_))
    return ConstUnwrap<HTMLTextAreaElement>()->selectionStart();
  return 0;
}

int WebFormControlElement::SelectionEnd() const {
  if (isHTMLInputElement(*private_))
    return ConstUnwrap<HTMLInputElement>()->selectionEnd();
  if (isHTMLTextAreaElement(*private_))
    return ConstUnwrap<HTMLTextAreaElement>()->selectionEnd();
  return 0;
}

WebString WebFormControlElement::AlignmentForFormData() const {
  if (const ComputedStyle* style =
          ConstUnwrap<HTMLFormControlElement>()->GetComputedStyle()) {
    if (style->GetTextAlign() == ETextAlign::kRight)
      return WebString::FromUTF8("right");
    if (style->GetTextAlign() == ETextAlign::kLeft)
      return WebString::FromUTF8("left");
  }
  return WebString();
}

WebString WebFormControlElement::DirectionForFormData() const {
  if (const ComputedStyle* style =
          ConstUnwrap<HTMLFormControlElement>()->GetComputedStyle()) {
    return style->IsLeftToRightDirection() ? WebString::FromUTF8("ltr")
                                           : WebString::FromUTF8("rtl");
  }
  return WebString::FromUTF8("ltr");
}

WebFormElement WebFormControlElement::Form() const {
  return WebFormElement(ConstUnwrap<HTMLFormControlElement>()->Form());
}

WebFormControlElement::WebFormControlElement(HTMLFormControlElement* elem)
    : WebElement(elem) {}

DEFINE_WEB_NODE_TYPE_CASTS(WebFormControlElement,
                           IsElementNode() &&
                               ConstUnwrap<Element>()->IsFormControlElement());

WebFormControlElement& WebFormControlElement::operator=(
    HTMLFormControlElement* elem) {
  private_ = elem;
  return *this;
}

WebFormControlElement::operator HTMLFormControlElement*() const {
  return ToHTMLFormControlElement(private_.Get());
}

}  // namespace blink
