// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/label_formatter.h"

namespace autofill {

LabelFormatter::LabelFormatter(const std::string& app_locale,
                               ServerFieldType focused_field_type,
                               const std::vector<ServerFieldType>& field_types)
    : app_locale_(app_locale),
      focused_field_type_(focused_field_type),
      field_types_(field_types) {}
LabelFormatter::~LabelFormatter() = default;

}  // namespace autofill
