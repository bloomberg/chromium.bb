// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_FORM_FIELD_H_
#define WEBKIT_GLUE_FORM_FIELD_H_

#include "base/string16.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputElement.h"

namespace webkit_glue {

// Stores information about a field in a form.
class FormField {
 public:
  FormField();
  explicit FormField(const WebKit::WebInputElement& input_element);
  FormField(const string16& label,
            const string16& name,
            const string16& value,
            const string16& form_control_type,
            WebKit::WebInputElement::InputType input_type);

  string16 label() const { return label_; }
  string16 name() const { return name_; }
  string16 value() const { return value_; }
  string16 form_control_type() const { return form_control_type_; }
  WebKit::WebInputElement::InputType input_type() const { return input_type_; }

  void set_label(const string16& label) { label_ = label; }
  void set_name(const string16& name) { name_ = name; }
  void set_value(const string16& value) { value_ = value; }
  void set_form_control_type(const string16& form_control_type) {
    form_control_type_ = form_control_type;
  }
  void set_input_type(WebKit::WebInputElement::InputType input_type) {
    input_type_ = input_type;
  }

  bool operator==(const FormField& field) const;
  bool operator!=(const FormField& field) const;

 private:
  string16 label_;
  string16 name_;
  string16 value_;
  string16 form_control_type_;
  WebKit::WebInputElement::InputType input_type_;
};

// So we can compare FormFields with EXPECT_EQ().
std::ostream& operator<<(std::ostream& os, const FormField& profile);

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_FORM_FIELD_H_
