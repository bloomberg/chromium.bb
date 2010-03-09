// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/string16.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFormElement.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputElement.h"
#include "webkit/glue/form_field_values.h"

using WebKit::WebFormElement;
using WebKit::WebFrame;
using WebKit::WebInputElement;
using WebKit::WebVector;

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

void FormFieldValues::ExtractFormFieldValues(const WebFormElement& form) {
  WebVector<WebInputElement> input_elements;
  form.getInputElements(input_elements);

  for (size_t i = 0; i < input_elements.size(); i++) {
    const WebInputElement& input_element = input_elements[i];
    if (input_element.isEnabledFormControl()) {
      // TODO(jhawkins): Remove the check nameForAutofill().isEmpty() when we
      //                 have labels.
      if (input_element.autoComplete() &&
          input_element.inputType() != WebInputElement::Password &&
          !input_element.nameForAutofill().isEmpty()) {
        elements.push_back(FormField(input_element));
      }
    }
  }
}

}  // namespace webkit_glue
