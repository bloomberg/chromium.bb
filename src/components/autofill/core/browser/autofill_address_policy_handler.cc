// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_address_policy_handler.h"

#include "base/values.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace autofill {

AutofillAddressPolicyHandler::AutofillAddressPolicyHandler()
    : TypeCheckingPolicyHandler(policy::key::kAutofillAddressEnabled,
                                base::Value::Type::BOOLEAN) {}

AutofillAddressPolicyHandler::~AutofillAddressPolicyHandler() {}

void AutofillAddressPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value = policies.GetValue(policy_name());
  if (value && value->is_bool() && !value->GetBool())
    prefs->SetBoolean(autofill::prefs::kAutofillProfileEnabled, false);
}

}  // namespace autofill
