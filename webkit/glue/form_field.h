// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_FORM_FIELD_H_
#define WEBKIT_GLUE_FORM_FIELD_H_

#include <vector>

#include "base/string16.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFormControlElement.h"

namespace webkit_glue {

// Stores information about a field in a form.
class FormField {
 public:
  FormField();
  explicit FormField(WebKit::WebFormControlElement element);
  FormField(const string16& label,
            const string16& name,
            const string16& value,
            const string16& form_control_type,
            int max_length,
            bool is_autofilled);
  virtual ~FormField();

  const string16& label() const { return label_; }
  const string16& name() const { return name_; }
  const string16& value() const { return value_; }
  const string16& form_control_type() const { return form_control_type_; }
  int max_length() const { return max_length_; }
  bool is_autofilled() const { return is_autofilled_; }

  // Returns option string for elements for which they make sense (select-one,
  // for example) for the rest of elements return an empty array.
  const std::vector<string16>& option_strings() const {
    return option_strings_;
  }

  void set_label(const string16& label) { label_ = label; }
  void set_name(const string16& name) { name_ = name; }
  void set_value(const string16& value) { value_ = value; }
  void set_form_control_type(const string16& form_control_type) {
    form_control_type_ = form_control_type;
  }
  void set_max_length(int max_length) { max_length_ = max_length; }
  void set_autofilled(bool is_autofilled) { is_autofilled_ = is_autofilled; }
  void set_option_strings(const std::vector<string16>& strings) {
    option_strings_ = strings;
  }

  // Equality tests for identity which does not include |value_| or |size_|.
  // Use |StrictlyEqualsHack| method to test all members.
  // TODO(dhollowa): These operators need to be revised when we implement field
  // ids.
  bool operator==(const FormField& field) const;
  bool operator!=(const FormField& field) const;

  // Test equality of all data members.
  // TODO(dhollowa): This will be removed when we implement field ids.
  bool StrictlyEqualsHack(const FormField& field) const;

 private:
  string16 label_;
  string16 name_;
  string16 value_;
  string16 form_control_type_;
  int max_length_;
  bool is_autofilled_;
  std::vector<string16> option_strings_;
};

// So we can compare FormFields with EXPECT_EQ().
std::ostream& operator<<(std::ostream& os, const FormField& field);

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_FORM_FIELD_H_
