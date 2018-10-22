// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/form_data_predictions.h"

namespace autofill {

FormDataPredictions::FormDataPredictions() {
}

FormDataPredictions::FormDataPredictions(const FormDataPredictions& other)
    : data(other.data),
      signature(other.signature),
      fields(other.fields) {
}

FormDataPredictions::~FormDataPredictions() {
}

bool FormDataPredictions::operator==(
    const FormDataPredictions& predictions) const {
  return (data.SameFormAs(predictions.data) &&
          signature == predictions.signature &&
          fields == predictions.fields);
}

bool FormDataPredictions::operator!=(
    const FormDataPredictions& predictions) const {
  return !operator==(predictions);
}

}  // namespace autofill
