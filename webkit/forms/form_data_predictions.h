// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FORMS_FORM_DATA_PREDICTIONS_H__
#define WEBKIT_FORMS_FORM_DATA_PREDICTIONS_H__

#include <string>
#include <vector>

#include "webkit/forms/form_data.h"
#include "webkit/forms/form_field_predictions.h"
#include "webkit/forms/webkit_forms_export.h"

namespace webkit {
namespace forms {

// Holds information about a form to be filled and/or submitted.
struct WEBKIT_FORMS_EXPORT FormDataPredictions {
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

}  // namespace forms
}  // namespace webkit

#endif  // WEBKIT_FORMS_FORM_DATA_PREDICTIONS_H__
