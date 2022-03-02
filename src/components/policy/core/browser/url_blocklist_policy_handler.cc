// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/url_blocklist_policy_handler.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/browser/url_util.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

URLBlocklistPolicyHandler::URLBlocklistPolicyHandler(const char* policy_name)
    : TypeCheckingPolicyHandler(policy_name, base::Value::Type::LIST) {}

URLBlocklistPolicyHandler::~URLBlocklistPolicyHandler() = default;

bool URLBlocklistPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                    PolicyErrorMap* errors) {
  size_t disabled_schemes_entries = 0;
  // This policy is deprecated but still supported so check it first.
  const base::Value* disabled_schemes =
      policies.GetValue(key::kDisabledSchemes);
  if (disabled_schemes) {
    if (!disabled_schemes->is_list()) {
      errors->AddError(key::kDisabledSchemes, IDS_POLICY_TYPE_ERROR,
                       base::Value::GetTypeName(base::Value::Type::LIST));
    } else {
      disabled_schemes_entries = disabled_schemes->GetList().size();
    }
  }

  const base::Value* url_blocklist = policies.GetValue(policy_name());
  if (!url_blocklist)
    return true;

  if (!url_blocklist->is_list()) {
    errors->AddError(policy_name(), IDS_POLICY_TYPE_ERROR,
                     base::Value::GetTypeName(base::Value::Type::LIST));

    return true;
  }

  // Filters more than |url_util::kMaxFiltersPerPolicy| are ignored, add a
  // warning message.
  if (url_blocklist->GetList().size() + disabled_schemes_entries >
      url_util::GetMaxFiltersPerPolicy()) {
    errors->AddError(policy_name(),
                     IDS_POLICY_URL_ALLOW_BLOCK_LIST_MAX_FILTERS_LIMIT_WARNING,
                     base::NumberToString(url_util::GetMaxFiltersPerPolicy()));
  }

  bool type_error = false;
  std::string policy;
  std::vector<std::string> invalid_policies;
  for (const auto& policy_iter : url_blocklist->GetList()) {
    if (!policy_iter.is_string()) {
      type_error = true;
      continue;
    }

    policy = policy_iter.GetString();
    if (!ValidatePolicy(policy))
      invalid_policies.push_back(policy);
  }

  if (type_error) {
    errors->AddError(policy_name(), IDS_POLICY_TYPE_ERROR,
                     base::Value::GetTypeName(base::Value::Type::STRING));
  }

  if (invalid_policies.size()) {
    errors->AddError(policy_name(), IDS_POLICY_PROTO_PARSING_ERROR,
                     base::JoinString(invalid_policies, ","));
  }

  return true;
}

void URLBlocklistPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                    PrefValueMap* prefs) {
  const base::Value* url_blocklist_policy = policies.GetValue(policy_name());
  const base::ListValue* url_blocklist = nullptr;
  if (url_blocklist_policy) {
    url_blocklist_policy->GetAsList(&url_blocklist);
  }

  const base::Value* disabled_schemes_policy =
      policies.GetValue(key::kDisabledSchemes);
  const base::ListValue* disabled_schemes = nullptr;
  if (disabled_schemes_policy)
    disabled_schemes_policy->GetAsList(&disabled_schemes);

  std::vector<base::Value> merged_url_blocklist;

  // We start with the DisabledSchemes because we have size limit when
  // handling URLBlocklists.
  if (disabled_schemes) {
    for (const auto& entry : disabled_schemes->GetList()) {
      if (entry.is_string()) {
        merged_url_blocklist.emplace_back(
            base::StrCat({entry.GetString(), "://*"}));
      }
    }
  }

  if (url_blocklist) {
    for (const auto& entry : url_blocklist->GetList()) {
      if (entry.is_string())
        merged_url_blocklist.push_back(entry.Clone());
    }
  }

  if (disabled_schemes || url_blocklist) {
    prefs->SetValue(policy_prefs::kUrlBlocklist,
                    base::Value(std::move(merged_url_blocklist)));
  }
}

bool URLBlocklistPolicyHandler::ValidatePolicy(const std::string& policy) {
  url_util::FilterComponents components;
  return url_util::FilterToComponents(
      policy, &components.scheme, &components.host,
      &components.match_subdomains, &components.port, &components.path,
      &components.query);
}

}  // namespace policy
