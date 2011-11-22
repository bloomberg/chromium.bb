// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
  virtual ~FormField();

  // Equality tests for identity which does not include |value| or
  // |is_autofilled|.
  // TODO(dhollowa): These operators need to be revised when we implement field
  // ids.
  bool operator==(const FormField& field) const;
  bool operator!=(const FormField& field) const;

  string16 label;
  string16 name;
  string16 value;
  string16 form_control_type;
  string16 autocomplete_type;
  size_t max_length;
  bool is_autofilled;
  bool is_focusable;
  bool should_autocomplete;

  // For the HTML snippet |<option value="US">United States</option>|, the
  // value is "US" and the contents are "United States".
  std::vector<string16> option_values;
  std::vector<string16> option_contents;
};

// So we can compare FormFields with EXPECT_EQ().
std::ostream& operator<<(std::ostream& os, const FormField& field);

}  // namespace webkit_glue

// Prefer to use this macro in place of |EXPECT_EQ()| for comparing |FormField|s
// in test code.
#define EXPECT_FORM_FIELD_EQUALS(expected, actual) \
  do { \
    EXPECT_EQ(expected.label, actual.label); \
    EXPECT_EQ(expected.name, actual.name); \
    EXPECT_EQ(expected.value, actual.value); \
    EXPECT_EQ(expected.form_control_type, actual.form_control_type); \
    EXPECT_EQ(expected.autocomplete_type, actual.autocomplete_type); \
    EXPECT_EQ(expected.max_length, actual.max_length); \
    EXPECT_EQ(expected.is_autofilled, actual.is_autofilled); \
  } while (0)

#endif  // WEBKIT_GLUE_FORM_FIELD_H_
