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

#include "config.h"
#include "WebFormControlElement.h"

#include "core/html/HTMLFormControlElement.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLSelectElement.h"
#include "core/html/HTMLTextAreaElement.h"

#include "wtf/PassRefPtr.h"

using namespace WebCore;

namespace blink {

bool WebFormControlElement::isEnabled() const
{
    return !constUnwrap<HTMLFormControlElement>()->isDisabledFormControl();
}

bool WebFormControlElement::isReadOnly() const
{
    return constUnwrap<HTMLFormControlElement>()->isReadOnly();
}

WebString WebFormControlElement::formControlName() const
{
    return constUnwrap<HTMLFormControlElement>()->name();
}

WebString WebFormControlElement::formControlType() const
{
    return constUnwrap<HTMLFormControlElement>()->type();
}

void WebFormControlElement::dispatchFormControlChangeEvent()
{
    unwrap<HTMLFormControlElement>()->dispatchFormControlChangeEvent();
}

bool WebFormControlElement::isAutofilled() const
{
    return constUnwrap<HTMLFormControlElement>()->isAutofilled();
}

void WebFormControlElement::setAutofilled(bool autofilled)
{
    unwrap<HTMLFormControlElement>()->setAutofilled(autofilled);
}

WebString WebFormControlElement::nameForAutofill() const
{
    return constUnwrap<HTMLFormControlElement>()->nameForAutofill();
}

bool WebFormControlElement::autoComplete() const
{
    if (m_private->hasTagName(HTMLNames::inputTag))
        return constUnwrap<HTMLInputElement>()->shouldAutocomplete();
    if (m_private->hasTagName(HTMLNames::textareaTag))
        return constUnwrap<HTMLTextAreaElement>()->shouldAutocomplete();
    return false;
}

void WebFormControlElement::setValue(const WebString& value, bool sendChangeEvent)
{
    if (m_private->hasTagName(HTMLNames::inputTag))
        unwrap<HTMLInputElement>()->setValue(value, sendChangeEvent ? DispatchChangeEvent : DispatchNoEvent);
    if (m_private->hasTagName(HTMLNames::textareaTag))
        unwrap<HTMLTextAreaElement>()->setValue(value);
    if (m_private->hasTagName(HTMLNames::selectTag))
        unwrap<HTMLSelectElement>()->setValue(value);
}

WebString WebFormControlElement::value() const
{
    if (m_private->hasTagName(HTMLNames::inputTag))
        return constUnwrap<HTMLInputElement>()->value();
    if (m_private->hasTagName(HTMLNames::textareaTag))
        return constUnwrap<HTMLTextAreaElement>()->value();
    if (m_private->hasTagName(HTMLNames::selectTag))
        return constUnwrap<HTMLSelectElement>()->value();
    return WebString();
}

void WebFormControlElement::setSuggestedValue(const WebString& value)
{
    if (m_private->hasTagName(HTMLNames::inputTag))
        unwrap<HTMLInputElement>()->setSuggestedValue(value);
    if (m_private->hasTagName(HTMLNames::textareaTag))
        unwrap<HTMLTextAreaElement>()->setSuggestedValue(value);
}

WebString WebFormControlElement::suggestedValue() const
{
    if (m_private->hasTagName(HTMLNames::inputTag))
        return constUnwrap<HTMLInputElement>()->suggestedValue();
    if (m_private->hasTagName(HTMLNames::textareaTag))
        return constUnwrap<HTMLTextAreaElement>()->suggestedValue();
    return WebString();
}

WebString WebFormControlElement::editingValue() const
{
    if (m_private->hasTagName(HTMLNames::inputTag))
        return constUnwrap<HTMLInputElement>()->innerTextValue();
    if (m_private->hasTagName(HTMLNames::textareaTag))
        return constUnwrap<HTMLTextAreaElement>()->innerTextValue();
    return WebString();
}

void WebFormControlElement::setSelectionRange(int start, int end)
{
    if (m_private->hasTagName(HTMLNames::inputTag))
        unwrap<HTMLInputElement>()->setSelectionRange(start, end);
    if (m_private->hasTagName(HTMLNames::textareaTag))
        unwrap<HTMLTextAreaElement>()->setSelectionRange(start, end);
}

int WebFormControlElement::selectionStart() const
{
    if (m_private->hasTagName(HTMLNames::inputTag))
        return constUnwrap<HTMLInputElement>()->selectionStart();
    if (m_private->hasTagName(HTMLNames::textareaTag))
        return constUnwrap<HTMLTextAreaElement>()->selectionStart();
    return 0;
}

int WebFormControlElement::selectionEnd() const
{
    if (m_private->hasTagName(HTMLNames::inputTag))
        return constUnwrap<HTMLInputElement>()->selectionEnd();
    if (m_private->hasTagName(HTMLNames::textareaTag))
        return constUnwrap<HTMLTextAreaElement>()->selectionEnd();
    return 0;
}

WebString WebFormControlElement::directionForFormData() const
{
    if (m_private->hasTagName(HTMLNames::inputTag))
        return constUnwrap<HTMLInputElement>()->directionForFormData();
    if (m_private->hasTagName(HTMLNames::textareaTag))
        return constUnwrap<HTMLTextAreaElement>()->directionForFormData();
    return WebString();
}

WebFormElement WebFormControlElement::form() const
{
    return WebFormElement(constUnwrap<HTMLFormControlElement>()->form());
}

WebFormControlElement::WebFormControlElement(const PassRefPtr<HTMLFormControlElement>& elem)
    : WebElement(elem)
{
}

WebFormControlElement& WebFormControlElement::operator=(const PassRefPtr<HTMLFormControlElement>& elem)
{
    m_private = elem;
    return *this;
}

WebFormControlElement::operator PassRefPtr<HTMLFormControlElement>() const
{
    return toHTMLFormControlElement(m_private.get());
}

} // namespace blink
