// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_limits_whitelist_policy_test_utils.h"

#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_limits_whitelist_policy_wrapper.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_policy_helpers.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_types.h"

namespace chromeos {
namespace app_time {

AppTimeLimitsWhitelistPolicyBuilder::AppTimeLimitsWhitelistPolicyBuilder() =
    default;

AppTimeLimitsWhitelistPolicyBuilder::~AppTimeLimitsWhitelistPolicyBuilder() =
    default;

void AppTimeLimitsWhitelistPolicyBuilder::SetUp() {
  value_ = base::Value(base::Value::Type::DICTIONARY);
  value_.SetKey(policy::kUrlList, base::Value(base::Value::Type::LIST));
  value_.SetKey(policy::kAppList, base::Value(base::Value::Type::LIST));
}

void AppTimeLimitsWhitelistPolicyBuilder::Clear() {
  base::DictionaryValue* dict_value;
  value_.GetAsDictionary(&dict_value);
  dict_value->Clear();
}

void AppTimeLimitsWhitelistPolicyBuilder::AppendToWhitelistUrlList(
    const std::string& scheme) {
  AppendToList(policy::kUrlList, base::Value(scheme));
}

void AppTimeLimitsWhitelistPolicyBuilder::AppendToWhitelistAppList(
    const AppId& app_id) {
  base::Value value_to_append(base::Value::Type::DICTIONARY);
  value_to_append.SetKey(policy::kAppId, base::Value(app_id.app_id()));
  value_to_append.SetKey(
      policy::kAppType,
      base::Value(policy::AppTypeToPolicyString(app_id.app_type())));
  AppendToList(policy::kAppList, std::move(value_to_append));
}

void AppTimeLimitsWhitelistPolicyBuilder::AppendToList(const std::string& key,
                                                       base::Value value) {
  base::Value* list = value_.FindListKey(key);
  DCHECK(list);
  list->Append(std::move(value));
}

}  // namespace app_time
}  // namespace chromeos
