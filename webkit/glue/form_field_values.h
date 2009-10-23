// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_FORM_FIELD_VALUES_H_
#define WEBKIT_GLUE_FORM_FIELD_VALUES_H_

#include <vector>

#include "webkit/glue/form_field.h"

namespace WebKit {
class WebForm;
}

namespace webkit_glue {

// The FormFieldValues struct represents a single HTML form together with the
// values entered in the fields.
class FormFieldValues {
 public:
  static FormFieldValues* Create(const WebKit::WebForm& form);

  // A vector of all the input fields in the form.
  std::vector<FormField> elements;
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_FORM_FIELD_VALUES_H_
