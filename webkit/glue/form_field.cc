// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/form_field.h"

#include "base/string_util.h"

using WebKit::WebInputElement;

namespace webkit_glue {

FormField::FormField() {
}

FormField::FormField(const WebInputElement& input_element) {
  name_ = input_element.nameForAutofill();

  // TODO(jhawkins): Extract the field label.  For now we just use the field
  // name.
  label_ = name_;

  value_ = input_element.value();
  TrimWhitespace(value_, TRIM_LEADING, &value_);

  form_control_type_ = input_element.formControlType();
  input_type_ = input_element.inputType();
}

FormField::FormField(const string16& label,
                     const string16& name,
                     const string16& value,
                     const string16& form_control_type,
                     WebInputElement::InputType input_type)
  : label_(label),
    name_(name),
    value_(value),
    form_control_type_(form_control_type),
    input_type_(input_type) {
}

}  // namespace webkit_glue
