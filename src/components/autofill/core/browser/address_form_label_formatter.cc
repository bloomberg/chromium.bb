// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_form_label_formatter.h"

namespace autofill {

AddressFormLabelFormatter::AddressFormLabelFormatter(
    const std::string& app_locale,
    ServerFieldType focused_field_type,
    const std::vector<ServerFieldType>& field_types)
    : LabelFormatter(app_locale, focused_field_type, field_types) {
  for (const ServerFieldType& type : field_types) {
    if (type != focused_field_type && type != ADDRESS_HOME_COUNTRY &&
        type != ADDRESS_BILLING_COUNTRY) {
      field_types_for_labels_.push_back(type);
    }
  }
}

AddressFormLabelFormatter::~AddressFormLabelFormatter() {}

std::vector<base::string16> AddressFormLabelFormatter::GetLabels(
    const std::vector<AutofillProfile*>& profiles) const {
  // TODO(crbug.com/936168): Implement GetLabels().
  std::vector<base::string16> labels;
  return labels;
}

}  // namespace autofill
