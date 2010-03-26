// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/form_field.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"

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

bool FormField::operator==(const FormField& field) const {
  // A FormField stores a value, but the value is not part of the identity of
  // the field, so we don't want to compare the values.
  return (label_ == field.label_ &&
          name_ == field.name_ &&
          form_control_type_ == field.form_control_type_ &&
          input_type_ == field.input_type_);
}

bool FormField::operator!=(const FormField& field) const {
  return !operator==(field);
}

std::ostream& operator<<(std::ostream& os, const FormField& field) {
  return os
      << UTF16ToUTF8(field.label())
      << " "
      << UTF16ToUTF8(field.name())
      << " "
      << UTF16ToUTF8(field.value())
      << " "
      << UTF16ToUTF8(field.form_control_type())
      << " "
      << field.input_type();
}

}  // namespace webkit_glue
