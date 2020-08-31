// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/origin_policy/origin_policy_parser.h"

#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/values.h"
#include "services/network/public/cpp/isolation_opt_in_hints.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

OriginPolicyContentsPtr OriginPolicyParser::Parse(base::StringPiece text) {
  OriginPolicyParser parser;
  parser.DoParse(text);
  return std::move(parser.policy_contents_);
}

OriginPolicyParser::OriginPolicyParser() = default;
OriginPolicyParser::~OriginPolicyParser() = default;

void OriginPolicyParser::DoParse(base::StringPiece policy_contents_text) {
  policy_contents_ = std::make_unique<OriginPolicyContents>();

  base::Optional<base::Value> json =
      base::JSONReader::Read(policy_contents_text);
  if (!json || !json->is_dict())
    return;

  if (!ParseIds(*json)) {
    return;
  }

  if (base::Value* content_security = json->FindDictKey("content_security")) {
    ParseContentSecurity(*content_security);
  }

  if (base::Value* features = json->FindDictKey("features")) {
    ParseFeatures(*features);
  }

  if (base::Value* isolation =
          json->FindKeyOfType("isolation", base::Value::Type::BOOLEAN)) {
    if (isolation->GetBool())
      policy_contents_->isolation_optin_hints = IsolationOptInHints::NO_HINTS;
  } else if (base::Value* isolation = json->FindDictKey("isolation")) {
    ParseIsolation(*isolation);
  }
}

bool OriginPolicyParser::ParseIds(const base::Value& json) {
  const base::Value* raw_ids = json.FindListKey("ids");
  if (!raw_ids) {
    return false;
  }
  for (const auto& id : raw_ids->GetList()) {
    if (id.is_string()) {
      const std::string& id_string = id.GetString();
      if (IsValidOriginPolicyId(id_string)) {
        policy_contents_->ids.push_back(id_string);
      }
    }
  }

  return !policy_contents_->ids.empty();
}

void OriginPolicyParser::ParseContentSecurity(
    const base::Value& content_security) {
  const base::Value* policies = content_security.FindListKey("policies");
  if (policies) {
    for (const auto& policy : policies->GetList()) {
      if (policy.is_string()) {
        policy_contents_->content_security_policies.push_back(
            policy.GetString());
      }
    }
  }

  const base::Value* policies_report_only =
      content_security.FindListKey("policies_report_only");
  if (policies_report_only) {
    for (const auto& policy : policies_report_only->GetList()) {
      if (policy.is_string()) {
        policy_contents_->content_security_policies_report_only.push_back(
            policy.GetString());
      }
    }
  }
}

void OriginPolicyParser::ParseFeatures(const base::Value& features) {
  const std::string* policy = features.FindStringKey("policy");

  if (policy) {
    policy_contents_->feature_policy = *policy;
  }
}

// The parsing is based on the example at
// https://github.com/domenic/origin-isolation#example.
void OriginPolicyParser::ParseIsolation(const base::Value& policy) {
  IsolationOptInHints hints = IsolationOptInHints::NO_HINTS;
  for (const auto& key_value : policy.DictItems()) {
    // If we hit a key with a non-boolean value, skip it.
    if (!key_value.second.is_bool())
      continue;
    if (key_value.second.GetBool()) {
      IsolationOptInHints dict_hint =
          GetIsolationOptInHintFromString(key_value.first);
      // If we hit a key we don't recognise, it will just return NO_HINTS and
      // have no effect.
      hints |= dict_hint;
    }
  }
  policy_contents_->isolation_optin_hints = hints;
}

// https://wicg.github.io/origin-policy/#valid-origin-policy-id
bool OriginPolicyParser::IsValidOriginPolicyId(const std::string& id) {
  return !id.empty() && std::none_of(id.begin(), id.end(), [](char ch) {
    return ch < 0x20 || ch > 0x7E;
  });
}

}  // namespace network
