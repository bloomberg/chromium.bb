// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFormElement.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputElement.h"
#include "webkit/glue/form_field_values.h"

using namespace WebKit;
namespace webkit_glue {

FormFieldValues* FormFieldValues::Create(const WebFormElement& form) {
  DCHECK(!form.isNull());

  WebFrame* frame = form.frame();
  if (!frame)
    return NULL;

  // Construct a new FormFieldValues.
  FormFieldValues* result = new FormFieldValues();

  result->form_name = form.name();
  result->method = form.method();
  result->source_url = frame->url();
  result->target_url = frame->completeURL(form.action());
  result->ExtractFormFieldValues(form);

  return result;
}

void FormFieldValues::ExtractFormFieldValues(
    const WebKit::WebFormElement& form) {

  WebVector<WebInputElement> input_elements;
  form.getInputElements(input_elements);

  for (size_t i = 0; i < input_elements.size(); i++) {
    const WebInputElement& input_element = input_elements[i];
    if (!input_element.isEnabledFormControl())
      continue;

    // Ignore all input types except TEXT.
    if (input_element.inputType() != WebInputElement::Text)
      continue;

    // For each TEXT input field, store the name and value
    string16 value = input_element.value();
    TrimWhitespace(value, TRIM_LEADING, &value);
    if (value.empty())
      continue;

    string16 name = input_element.nameForAutofill();
    if (name.empty())
      continue;  // If we have no name, there is nothing to store.

    string16 type = input_element.formControlType();
    if (type.empty())
      continue;

    elements.push_back(FormField(name, type, value));
  }
}

}  // namespace webkit_glue
