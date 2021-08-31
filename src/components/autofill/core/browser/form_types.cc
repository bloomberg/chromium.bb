// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

FormType FieldTypeGroupToFormType(FieldTypeGroup field_type_group) {
  switch (field_type_group) {
    case FieldTypeGroup::kName:
    case FieldTypeGroup::kNameBilling:
    case FieldTypeGroup::kEmail:
    case FieldTypeGroup::kCompany:
    case FieldTypeGroup::kAddressHome:
    case FieldTypeGroup::kAddressBilling:
    case FieldTypeGroup::kPhoneHome:
    case FieldTypeGroup::kPhoneBilling:
      return FormType::kAddressForm;
    case FieldTypeGroup::kCreditCard:
      return FormType::kCreditCardForm;
    case FieldTypeGroup::kUsernameField:
    case FieldTypeGroup::kPasswordField:
      return FormType::kPasswordForm;
    case FieldTypeGroup::kNoGroup:
    case FieldTypeGroup::kTransaction:
    case FieldTypeGroup::kUnfillable:
      return FormType::kUnknownFormType;
  }
}
}  // namespace autofill
