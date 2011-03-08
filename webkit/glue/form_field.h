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
struct FormField {
  FormField();
  explicit FormField(WebKit::WebFormControlElement element);
  FormField(const string16& label,
            const string16& name,
            const string16& value,
            const string16& form_control_type,
            int max_length,
            bool is_autofilled);
  virtual ~FormField();

  // Equality tests for identity which does not include |value_| or |size_|.
  // Use |StrictlyEqualsHack| method to test all members.
  // TODO(dhollowa): These operators need to be revised when we implement field
  // ids.
  bool operator==(const FormField& field) const;
  bool operator!=(const FormField& field) const;

  // Test equality of all data members.
  // TODO(dhollowa): This will be removed when we implement field ids.
  bool StrictlyEqualsHack(const FormField& field) const;

  string16 label;
  string16 name;
  string16 value;
  string16 form_control_type;
  int max_length;
  bool is_autofilled;
  std::vector<string16> option_strings;
};

// So we can compare FormFields with EXPECT_EQ().
std::ostream& operator<<(std::ostream& os, const FormField& field);

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_FORM_FIELD_H_
