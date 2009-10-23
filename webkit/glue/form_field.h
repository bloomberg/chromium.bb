// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_FORM_FIELD_H_
#define WEBKIT_GLUE_FORM_FIELD_H_

#include "base/string16.h"

namespace WebCore {
class HTMLFormControlElement;
}

namespace webkit_glue {

// Stores information about a field in a form.
class FormField {
 public:
  FormField();
  FormField(WebCore::HTMLFormControlElement* element,
            const string16& name,
            const string16& value);

  WebCore::HTMLFormControlElement* element() const { return element_; }
  string16 name() const { return name_; }
  string16 value() const { return value_; }

  void set_element(WebCore::HTMLFormControlElement* element) {
      element_ = element;
  }
  void set_value(const string16& value) { value_ = value; }

 private:
  WebCore::HTMLFormControlElement* element_;
  string16 name_;
  string16 value_;
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_FORM_FIELD_H_
