// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FORMS_FORM_FIELD_PREDICTIONS_H_
#define WEBKIT_FORMS_FORM_FIELD_PREDICTIONS_H_

#include <string>
#include <vector>

#include "webkit/forms/form_field.h"
#include "webkit/forms/webkit_forms_export.h"

namespace webkit {
namespace forms {

// Stores information about a field in a form.
struct WEBKIT_FORMS_EXPORT FormFieldPredictions {
  FormFieldPredictions();
  FormFieldPredictions(const FormFieldPredictions& other);
  ~FormFieldPredictions();

  FormField field;
  std::string signature;
  std::string heuristic_type;
  std::string server_type;
  std::string overall_type;
};

}  // namespace forms
}  // namespace webkit

#endif  // WEBKIT_FORMS_FORM_FIELD_PREDICTIONS_H_
