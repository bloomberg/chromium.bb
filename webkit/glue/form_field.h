// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_FORM_FIELD_H_
#define WEBKIT_GLUE_FORM_FIELD_H_

#include "base/string16.h"

namespace webkit_glue {

// Stores information about a field in a form.
class FormField {
 public:
  FormField();
  FormField(const string16& name,
            const string16& html_input_type,
            const string16& value);

  string16 name() const { return name_; }
  string16 html_input_type() const { return html_input_type_; }
  string16 value() const { return value_; }

  void set_value(const string16& value) { value_ = value; }

 private:
  string16 name_;
  string16 html_input_type_;
  string16 value_;
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_FORM_FIELD_H_
