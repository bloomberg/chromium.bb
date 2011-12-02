// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_FORM_DATA_PREDICTIONS_H__
#define WEBKIT_GLUE_FORM_DATA_PREDICTIONS_H__

#include <string>
#include <vector>

#include "webkit/glue/form_data.h"
#include "webkit/glue/form_field_predictions.h"
#include "webkit/glue/webkit_glue_export.h"

namespace webkit_glue {

// Holds information about a form to be filled and/or submitted.
struct WEBKIT_GLUE_EXPORT FormDataPredictions {
  // Data for this form.
  FormData data;
  // The form signature for communication with the crowdsourcing server.
  std::string signature;
  // The experiment id for the server predictions.
  std::string experiment_id;
  // The form fields and their predicted field types.
  std::vector<FormFieldPredictions> fields;

  FormDataPredictions();
  FormDataPredictions(const FormDataPredictions& other);
  ~FormDataPredictions();
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_FORM_DATA_PREDICTIONS_H__
