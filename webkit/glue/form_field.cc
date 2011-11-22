// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/form_field.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebOptionElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSelectElement.h"

using WebKit::WebFormControlElement;
using WebKit::WebElement;
using WebKit::WebInputElement;
using WebKit::WebOptionElement;
using WebKit::WebSelectElement;
using WebKit::WebVector;

namespace webkit_glue {

FormField::FormField()
    : max_length(0),
      is_autofilled(false),
      is_focusable(false),
      should_autocomplete(false) {
}

FormField::~FormField() {
}

bool FormField::operator==(const FormField& field) const {
  // A FormField stores a value, but the value is not part of the identity of
  // the field, so we don't want to compare the values.
  return (label == field.label &&
          name == field.name &&
          form_control_type == field.form_control_type &&
          autocomplete_type == field.autocomplete_type &&
          max_length == field.max_length);
}

bool FormField::operator!=(const FormField& field) const {
  return !operator==(field);
}

std::ostream& operator<<(std::ostream& os, const FormField& field) {
  return os
      << UTF16ToUTF8(field.label)
      << " "
      << UTF16ToUTF8(field.name)
      << " "
      << UTF16ToUTF8(field.value)
      << " "
      << UTF16ToUTF8(field.form_control_type)
      << " "
      << UTF16ToUTF8(field.autocomplete_type)
      << " "
      << field.max_length
      << " "
      << (field.is_autofilled ? "true" : "false")
      << " "
      << (field.is_focusable ? "true" : "false")
      << " "
      << (field.should_autocomplete ? "true" : "false");
}

}  // namespace webkit_glue
