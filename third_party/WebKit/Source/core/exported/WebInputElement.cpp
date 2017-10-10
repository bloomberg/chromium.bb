/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "public/web/WebInputElement.h"

#include "core/dom/ElementShadow.h"
#include "core/dom/ShadowRoot.h"
#include "core/html/forms/HTMLDataListElement.h"
#include "core/html/forms/HTMLDataListOptionsCollection.h"
#include "core/html/forms/HTMLInputElement.h"
#include "core/html/forms/TextControlInnerElements.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/html_names.h"
#include "core/input_type_names.h"
#include "public/platform/WebString.h"
#include "public/web/WebElementCollection.h"
#include "public/web/WebOptionElement.h"

namespace blink {

bool WebInputElement::IsTextField() const {
  return ConstUnwrap<HTMLInputElement>()->IsTextField();
}

bool WebInputElement::IsText() const {
  return ConstUnwrap<HTMLInputElement>()->IsTextField() &&
         ConstUnwrap<HTMLInputElement>()->type() != InputTypeNames::number;
}

bool WebInputElement::IsEmailField() const {
  return ConstUnwrap<HTMLInputElement>()->type() == InputTypeNames::email;
}

bool WebInputElement::IsPasswordField() const {
  return ConstUnwrap<HTMLInputElement>()->type() == InputTypeNames::password;
}

bool WebInputElement::IsImageButton() const {
  return ConstUnwrap<HTMLInputElement>()->type() == InputTypeNames::image;
}

bool WebInputElement::IsRadioButton() const {
  return ConstUnwrap<HTMLInputElement>()->type() == InputTypeNames::radio;
}

bool WebInputElement::IsCheckbox() const {
  return ConstUnwrap<HTMLInputElement>()->type() == InputTypeNames::checkbox;
}

int WebInputElement::MaxLength() const {
  int max_len = ConstUnwrap<HTMLInputElement>()->maxLength();
  return max_len == -1 ? DefaultMaxLength() : max_len;
}

void WebInputElement::SetActivatedSubmit(bool activated) {
  Unwrap<HTMLInputElement>()->SetActivatedSubmit(activated);
}

int WebInputElement::size() const {
  return ConstUnwrap<HTMLInputElement>()->size();
}

void WebInputElement::SetEditingValue(const WebString& value) {
  Unwrap<HTMLInputElement>()->SetEditingValue(value);
}

bool WebInputElement::IsValidValue(const WebString& value) const {
  return ConstUnwrap<HTMLInputElement>()->IsValidValue(value);
}

void WebInputElement::SetChecked(bool now_checked, bool send_events) {
  Unwrap<HTMLInputElement>()->setChecked(
      now_checked,
      send_events ? kDispatchInputAndChangeEvent : kDispatchNoEvent);
}

bool WebInputElement::IsChecked() const {
  return ConstUnwrap<HTMLInputElement>()->checked();
}

bool WebInputElement::IsMultiple() const {
  return ConstUnwrap<HTMLInputElement>()->Multiple();
}

WebVector<WebOptionElement> WebInputElement::FilteredDataListOptions() const {
  return WebVector<WebOptionElement>(
      ConstUnwrap<HTMLInputElement>()->FilteredDataListOptions());
}

WebString WebInputElement::LocalizeValue(
    const WebString& proposed_value) const {
  return ConstUnwrap<HTMLInputElement>()->LocalizeValue(proposed_value);
}

int WebInputElement::DefaultMaxLength() {
  return std::numeric_limits<int>::max();
}

void WebInputElement::SetShouldRevealPassword(bool value) {
  Unwrap<HTMLInputElement>()->SetShouldRevealPassword(value);
}

bool WebInputElement::ShouldRevealPassword() const {
  return ConstUnwrap<HTMLInputElement>()->ShouldRevealPassword();
}

WebInputElement::WebInputElement(HTMLInputElement* elem)
    : WebFormControlElement(elem) {}

DEFINE_WEB_NODE_TYPE_CASTS(WebInputElement,
                           IsHTMLInputElement(ConstUnwrap<Node>()));

WebInputElement& WebInputElement::operator=(HTMLInputElement* elem) {
  private_ = elem;
  return *this;
}

WebInputElement::operator HTMLInputElement*() const {
  return ToHTMLInputElement(private_.Get());
}

WebInputElement* ToWebInputElement(WebElement* web_element) {
  if (!IsHTMLInputElement(*web_element->Unwrap<Element>()))
    return 0;

  return static_cast<WebInputElement*>(web_element);
}
}  // namespace blink
