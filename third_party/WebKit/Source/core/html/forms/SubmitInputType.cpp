/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "core/html/forms/SubmitInputType.h"

#include "core/InputTypeNames.h"
#include "core/dom/events/Event.h"
#include "core/html/FormData.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLInputElement.h"
#include "platform/text/PlatformLocale.h"

namespace blink {

InputType* SubmitInputType::Create(HTMLInputElement& element) {
  UseCounter::Count(element.GetDocument(), WebFeature::kInputTypeSubmit);
  return new SubmitInputType(element);
}

const AtomicString& SubmitInputType::FormControlType() const {
  return InputTypeNames::submit;
}

void SubmitInputType::AppendToFormData(FormData& form_data) const {
  if (GetElement().IsActivatedSubmit())
    form_data.append(GetElement().GetName(),
                     GetElement().ValueOrDefaultLabel());
}

bool SubmitInputType::SupportsRequired() const {
  return false;
}

void SubmitInputType::HandleDOMActivateEvent(Event* event) {
  if (GetElement().IsDisabledFormControl() || !GetElement().Form())
    return;
  GetElement().Form()->PrepareForSubmission(
      event, &GetElement());  // Event handlers can run.
  event->SetDefaultHandled();
}

bool SubmitInputType::CanBeSuccessfulSubmitButton() {
  return true;
}

String SubmitInputType::DefaultLabel() const {
  return GetLocale().QueryString(WebLocalizedString::kSubmitButtonDefaultLabel);
}

bool SubmitInputType::IsTextButton() const {
  return true;
}

void SubmitInputType::ValueAttributeChanged() {
  UseCounter::Count(GetElement().GetDocument(),
                    WebFeature::kInputTypeSubmitWithValue);
  BaseButtonInputType::ValueAttributeChanged();
}

}  // namespace blink
