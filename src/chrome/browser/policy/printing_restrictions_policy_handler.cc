// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/printing_restrictions_policy_handler.h"

#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

PrintingAllowedColorModesPolicyHandler::PrintingAllowedColorModesPolicyHandler()
    : TypeCheckingPolicyHandler(key::kPrintingAllowedColorModes,
                                base::Value::Type::STRING) {}

PrintingAllowedColorModesPolicyHandler::
    ~PrintingAllowedColorModesPolicyHandler() {}

bool PrintingAllowedColorModesPolicyHandler::GetValue(
    const PolicyMap& policies,
    PolicyErrorMap* errors,
    printing::ColorModeRestriction* result) {
  const base::Value* value;
  if (CheckAndGetValue(policies, errors, &value) && value) {
    base::Optional<printing::ColorModeRestriction> mode =
        printing::GetAllowedColorModesForName(value->GetString());
    if (mode.has_value()) {
      if (result)
        *result = mode.value();

      return true;
    } else if (errors) {
      errors->AddError(key::kPrintingAllowedColorModes,
                       IDS_POLICY_VALUE_FORMAT_ERROR);
    }
  }
  return false;
}

bool PrintingAllowedColorModesPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  return GetValue(policies, errors, nullptr);
}

void PrintingAllowedColorModesPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  printing::ColorModeRestriction value;
  if (GetValue(policies, nullptr, &value)) {
    prefs->SetInteger(prefs::kPrintingAllowedColorModes,
                      static_cast<int>(value));
  }
}

PrintingAllowedDuplexModesPolicyHandler::
    PrintingAllowedDuplexModesPolicyHandler()
    : TypeCheckingPolicyHandler(key::kPrintingAllowedDuplexModes,
                                base::Value::Type::STRING) {}

PrintingAllowedDuplexModesPolicyHandler::
    ~PrintingAllowedDuplexModesPolicyHandler() {}

bool PrintingAllowedDuplexModesPolicyHandler::GetValue(
    const PolicyMap& policies,
    PolicyErrorMap* errors,
    printing::DuplexModeRestriction* result) {
  const base::Value* value;
  if (CheckAndGetValue(policies, errors, &value) && value) {
    base::Optional<printing::DuplexModeRestriction> mode =
        printing::GetAllowedDuplexModesForName(value->GetString());
    if (mode.has_value()) {
      if (result)
        *result = mode.value();

      return true;
    } else if (errors) {
      errors->AddError(key::kPrintingAllowedDuplexModes,
                       IDS_POLICY_VALUE_FORMAT_ERROR);
    }
  }
  return false;
}

bool PrintingAllowedDuplexModesPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  return GetValue(policies, errors, nullptr);
}

void PrintingAllowedDuplexModesPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  printing::DuplexModeRestriction value;
  if (GetValue(policies, nullptr, &value)) {
    prefs->SetInteger(prefs::kPrintingAllowedDuplexModes,
                      static_cast<int>(value));
  }
}

PrintingAllowedPinModesPolicyHandler::PrintingAllowedPinModesPolicyHandler()
    : TypeCheckingPolicyHandler(key::kPrintingAllowedPinModes,
                                base::Value::Type::STRING) {}

PrintingAllowedPinModesPolicyHandler::~PrintingAllowedPinModesPolicyHandler() {}

bool PrintingAllowedPinModesPolicyHandler::GetValue(
    const PolicyMap& policies,
    PolicyErrorMap* errors,
    printing::PinModeRestriction* result) {
  const base::Value* value;
  if (CheckAndGetValue(policies, errors, &value) && value) {
    base::Optional<printing::PinModeRestriction> mode =
        printing::GetAllowedPinModesForName(value->GetString());
    if (mode.has_value()) {
      if (result)
        *result = mode.value();

      return true;
    } else if (errors) {
      errors->AddError(key::kPrintingAllowedPinModes,
                       IDS_POLICY_VALUE_FORMAT_ERROR);
    }
  }
  return false;
}

bool PrintingAllowedPinModesPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  return GetValue(policies, errors, nullptr);
}

void PrintingAllowedPinModesPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  printing::PinModeRestriction value;
  if (GetValue(policies, nullptr, &value))
    prefs->SetInteger(prefs::kPrintingAllowedPinModes, static_cast<int>(value));
}

PrintingAllowedPageSizesPolicyHandler::PrintingAllowedPageSizesPolicyHandler()
    : ListPolicyHandler(key::kPrintingAllowedPageSizes,
                        base::Value::Type::DICTIONARY) {}

PrintingAllowedPageSizesPolicyHandler::
    ~PrintingAllowedPageSizesPolicyHandler() {}

bool PrintingAllowedPageSizesPolicyHandler::CheckListEntry(
    const base::Value& value) {
  if (!value.is_dict())
    return false;
  const base::Value* width = value.FindKey(printing::kPageWidthUm);
  const base::Value* height = value.FindKey(printing::kPageHeightUm);
  return width && height && width->is_int() && height->is_int();
}

void PrintingAllowedPageSizesPolicyHandler::ApplyList(
    std::unique_ptr<base::ListValue> filtered_list,
    PrefValueMap* prefs) {
  DCHECK(filtered_list);
  prefs->SetValue(prefs::kPrintingAllowedPageSizes,
                  base::Value::FromUniquePtrValue(std::move(filtered_list)));
}

PrintingColorDefaultPolicyHandler::PrintingColorDefaultPolicyHandler()
    : TypeCheckingPolicyHandler(key::kPrintingColorDefault,
                                base::Value::Type::STRING) {}

PrintingColorDefaultPolicyHandler::~PrintingColorDefaultPolicyHandler() {}

bool PrintingColorDefaultPolicyHandler::GetValue(
    const PolicyMap& policies,
    PolicyErrorMap* errors,
    printing::ColorModeRestriction* result) {
  const base::Value* value;
  if (CheckAndGetValue(policies, errors, &value) && value) {
    base::Optional<printing::ColorModeRestriction> mode =
        printing::GetColorModeForName(value->GetString());
    if (mode.has_value()) {
      if (result)
        *result = mode.value();

      return true;
    } else if (errors) {
      errors->AddError(key::kPrintingColorDefault,
                       IDS_POLICY_VALUE_FORMAT_ERROR);
    }
  }
  return false;
}

bool PrintingColorDefaultPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  return GetValue(policies, errors, nullptr);
}

void PrintingColorDefaultPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  printing::ColorModeRestriction value;
  if (GetValue(policies, nullptr, &value))
    prefs->SetInteger(prefs::kPrintingColorDefault, static_cast<int>(value));
}

PrintingDuplexDefaultPolicyHandler::PrintingDuplexDefaultPolicyHandler()
    : TypeCheckingPolicyHandler(key::kPrintingDuplexDefault,
                                base::Value::Type::STRING) {}

PrintingDuplexDefaultPolicyHandler::~PrintingDuplexDefaultPolicyHandler() {}

bool PrintingDuplexDefaultPolicyHandler::GetValue(
    const PolicyMap& policies,
    PolicyErrorMap* errors,
    printing::DuplexModeRestriction* result) {
  const base::Value* value;
  if (CheckAndGetValue(policies, errors, &value) && value) {
    base::Optional<printing::DuplexModeRestriction> mode =
        printing::GetDuplexModeForName(value->GetString());
    if (mode.has_value()) {
      if (result)
        *result = mode.value();

      return true;
    } else if (errors) {
      errors->AddError(key::kPrintingDuplexDefault,
                       IDS_POLICY_VALUE_FORMAT_ERROR);
    }
  }
  return false;
}

bool PrintingDuplexDefaultPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  return GetValue(policies, errors, nullptr);
}

void PrintingDuplexDefaultPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  printing::DuplexModeRestriction value;
  if (GetValue(policies, nullptr, &value))
    prefs->SetInteger(prefs::kPrintingDuplexDefault, static_cast<int>(value));
}

PrintingPinDefaultPolicyHandler::PrintingPinDefaultPolicyHandler()
    : TypeCheckingPolicyHandler(key::kPrintingPinDefault,
                                base::Value::Type::STRING) {}

PrintingPinDefaultPolicyHandler::~PrintingPinDefaultPolicyHandler() {}

bool PrintingPinDefaultPolicyHandler::GetValue(
    const PolicyMap& policies,
    PolicyErrorMap* errors,
    printing::PinModeRestriction* result) {
  const base::Value* value;
  if (CheckAndGetValue(policies, errors, &value) && value) {
    base::Optional<printing::PinModeRestriction> mode =
        printing::GetPinModeForName(value->GetString());
    if (mode.has_value()) {
      if (result)
        *result = mode.value();

      return true;
    } else if (errors) {
      errors->AddError(key::kPrintingPinDefault, IDS_POLICY_VALUE_FORMAT_ERROR);
    }
  }
  return false;
}

bool PrintingPinDefaultPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  return GetValue(policies, errors, nullptr);
}

void PrintingPinDefaultPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  printing::PinModeRestriction value;
  if (GetValue(policies, nullptr, &value))
    prefs->SetInteger(prefs::kPrintingPinDefault, static_cast<int>(value));
}

PrintingSizeDefaultPolicyHandler::PrintingSizeDefaultPolicyHandler()
    : TypeCheckingPolicyHandler(key::kPrintingSizeDefault,
                                base::Value::Type::DICTIONARY) {}

PrintingSizeDefaultPolicyHandler::~PrintingSizeDefaultPolicyHandler() {}

bool PrintingSizeDefaultPolicyHandler::CheckIntSubkey(const base::Value* dict,
                                                      const std::string& key,
                                                      PolicyErrorMap* errors) {
  const base::Value* value = dict->FindKey(key);
  if (!value) {
    if (errors) {
      errors->AddError(policy_name(), key, IDS_POLICY_NOT_SPECIFIED_ERROR);
    }
    return false;
  }
  if (!value->is_int()) {
    if (errors) {
      errors->AddError(policy_name(), key, IDS_POLICY_TYPE_ERROR,
                       base::Value::GetTypeName(base::Value::Type::INTEGER));
    }
    return false;
  }
  return true;
}

bool PrintingSizeDefaultPolicyHandler::GetValue(const PolicyMap& policies,
                                                PolicyErrorMap* errors,
                                                const base::Value** result) {
  const base::Value* value;
  if (CheckAndGetValue(policies, errors, &value) && value &&
      CheckIntSubkey(value, printing::kPageWidthUm, errors) &&
      CheckIntSubkey(value, printing::kPageHeightUm, errors)) {
    if (result)
      *result = value;

    return true;
  }
  return false;
}

bool PrintingSizeDefaultPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  return GetValue(policies, errors, nullptr);
}

void PrintingSizeDefaultPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value;
  if (GetValue(policies, nullptr, &value)) {
    prefs->SetValue(prefs::kPrintingSizeDefault, value->Clone());
  }
}

}  // namespace policy
