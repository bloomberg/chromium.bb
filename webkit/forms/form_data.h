// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FORMS_FORM_DATA_H__
#define WEBKIT_FORMS_FORM_DATA_H__

#include <vector>

#include "base/string16.h"
#include "googleurl/src/gurl.h"
#include "webkit/forms/form_field.h"
#include "webkit/forms/webkit_forms_export.h"

namespace webkit {
namespace forms {

// Holds information about a form to be filled and/or submitted.
struct WEBKIT_FORMS_EXPORT FormData {
  // The name of the form.
  string16 name;
  // GET or POST.
  string16 method;
  // The URL (minus query parameters) containing the form.
  GURL origin;
  // The action target of the form.
  GURL action;
  // true if this form was submitted by a user gesture and not javascript.
  bool user_submitted;
  // A vector of all the input fields in the form.
  std::vector<FormField> fields;

  FormData();
  FormData(const FormData& data);
  ~FormData();

  // Used by FormStructureTest.
  bool operator==(const FormData& form) const;
};

}  // namespace forms
}  // namespace webkit

#endif  // WEBKIT_FORMS_FORM_DATA_H__
