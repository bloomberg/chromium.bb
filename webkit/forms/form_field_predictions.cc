// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/forms/form_field_predictions.h"

namespace webkit {
namespace forms {

FormFieldPredictions::FormFieldPredictions() {
}

FormFieldPredictions::FormFieldPredictions(const FormFieldPredictions& other)
    : field(other.field),
      signature(other.signature),
      heuristic_type(other.heuristic_type),
      server_type(other.server_type),
      overall_type(other.overall_type) {
}

FormFieldPredictions::~FormFieldPredictions() {
}

}  // namespace forms
}  // namespace webkit
