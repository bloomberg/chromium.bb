// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "Document.h"
#include "Frame.h"
#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#undef LOG

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFormElement.h"
#include "webkit/glue/form_field_values.h"
// Can include from api/src because this file will eventually be there too.
#include "third_party/WebKit/WebKit/chromium/src/DOMUtilitiesPrivate.h"
#include "webkit/glue/glue_util.h"

using WebKit::WebFormElement;

namespace webkit_glue {

FormFieldValues* FormFieldValues::Create(const WebFormElement& webform) {
  RefPtr<WebCore::HTMLFormElement> form = WebFormElementToHTMLFormElement(webform);
  DCHECK(form);

  WebCore::Document* document = form->document();
  WebCore::Frame* frame = document->frame();
  if (!frame)
    return NULL;

  WebCore::FrameLoader* loader = frame->loader();
  if (!loader)
    return NULL;

  // Construct a new FormFieldValues.
  FormFieldValues* result = new FormFieldValues();

  result->form_name = StringToString16(form->name());
  result->method = StringToString16(form->method());
  result->source_url = KURLToGURL(document->url());
  result->target_url = KURLToGURL(document->completeURL(form->action()));
  result->ExtractFormFieldValues(webform);

  return result;
}

void FormFieldValues::ExtractFormFieldValues(
    const WebKit::WebFormElement& webform) {
  RefPtr<WebCore::HTMLFormElement> form = WebFormElementToHTMLFormElement(webform);

  const WTF::Vector<WebCore::HTMLFormControlElement*>& form_elements =
      form->formElements;

  size_t form_element_count = form_elements.size();
  for (size_t i = 0; i < form_element_count; i++) {
    WebCore::HTMLFormControlElement* form_element = form_elements[i];

    if (!form_element->hasLocalName(WebCore::HTMLNames::inputTag))
      continue;

    WebCore::HTMLInputElement* input_element =
        static_cast<WebCore::HTMLInputElement*>(form_element);
    if (!input_element->isEnabledFormControl())
      continue;

    // Ignore all input types except TEXT.
    if (input_element->inputType() != WebCore::HTMLInputElement::TEXT)
      continue;

    // For each TEXT input field, store the name and value
    string16 value = StringToString16(input_element->value());
    TrimWhitespace(value, TRIM_LEADING, &value);
    if (value.empty())
      continue;

    string16 name = StringToString16(WebKit::nameOfInputElement(input_element));
    if (name.empty())
      continue;  // If we have no name, there is nothing to store.

    string16 type = StringToString16(input_element->formControlType());
    if (type.empty())
      continue;

    elements.push_back(FormField(name, type, value));
  }
}

}  // namespace webkit_glue
