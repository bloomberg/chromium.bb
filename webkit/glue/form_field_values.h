// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_FORM_FIELD_VALUES_H_
#define WEBKIT_GLUE_FORM_FIELD_VALUES_H_

#include <vector>

#include "base/string16.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/form_field.h"

namespace WebKit {
class WebFormElement;
}

namespace webkit_glue {

// The FormFieldValues class represents a single HTML form together with the
// values entered in the fields.
class FormFieldValues {
 public:
  static FormFieldValues* Create(const WebKit::WebFormElement&);

  // The name of the form.
  string16 form_name;

  // GET or POST.
  string16 method;

  // The source URL.
  GURL source_url;

  // The target URL.
  GURL target_url;

  // A vector of all the input fields in the form.
  std::vector<FormField> elements;

 private:
  void ExtractFormFieldValues(const WebKit::WebFormElement&);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_FORM_FIELD_VALUES_H_
