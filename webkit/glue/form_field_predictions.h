// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_FORM_FIELD_PREDICTIONS_H_
#define WEBKIT_GLUE_FORM_FIELD_PREDICTIONS_H_

#include <string>
#include <vector>

#include "webkit/glue/form_field.h"
#include "webkit/glue/webkit_glue_export.h"

namespace webkit_glue {

// Stores information about a field in a form.
struct WEBKIT_GLUE_EXPORT FormFieldPredictions {
  FormFieldPredictions();
  FormFieldPredictions(const FormFieldPredictions& other);
  ~FormFieldPredictions();

  FormField field;
  std::string signature;
  std::string heuristic_type;
  std::string server_type;
  std::string overall_type;
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_FORM_FIELD_PREDICTIONS_H_
