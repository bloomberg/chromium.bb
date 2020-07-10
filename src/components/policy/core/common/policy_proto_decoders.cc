// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_proto_decoders.h"

#include <limits>
#include <memory>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/values.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/cloud_policy.pb.h"

namespace policy {

namespace em = enterprise_management;

namespace {

// Returns true and sets |level| to a PolicyLevel if the policy has been set
// at that level. Returns false if the policy is not set, or has been set at
// the level of PolicyOptions::UNSET.
template <class AnyPolicyProto>
bool GetPolicyLevel(const AnyPolicyProto& policy_proto, PolicyLevel* level) {
  if (!policy_proto.has_value()) {
    return false;
  }
  if (!policy_proto.has_policy_options()) {
    *level = POLICY_LEVEL_MANDATORY;  // Default level.
    return true;
  }
  switch (policy_proto.policy_options().mode()) {
    case em::PolicyOptions::MANDATORY:
      *level = POLICY_LEVEL_MANDATORY;
      return true;
    case em::PolicyOptions::RECOMMENDED:
      *level = POLICY_LEVEL_RECOMMENDED;
      return true;
    case em::PolicyOptions::UNSET:
      return false;
  }
}

// Convert a BooleanPolicyProto to a bool base::Value.
std::unique_ptr<base::Value> DecodeBooleanProto(
    const em::BooleanPolicyProto& proto) {
  return std::make_unique<base::Value>(proto.value());
}

// Convert an IntegerPolicyProto to an int base::Value.
std::unique_ptr<base::Value> DecodeIntegerProto(
    const em::IntegerPolicyProto& proto,
    std::string* error) {
  google::protobuf::int64 value = proto.value();

  if (value < std::numeric_limits<int>::min() ||
      value > std::numeric_limits<int>::max()) {
    LOG(WARNING) << "Integer value " << value << " out of numeric limits";
    *error = "Number out of range - invalid int32";
    return std::make_unique<base::Value>(std::to_string(value));
  }

  return std::make_unique<base::Value>(static_cast<int>(value));
}

// Convert a StringPolicyProto to a string base::Value.
std::unique_ptr<base::Value> DecodeStringProto(
    const em::StringPolicyProto& proto) {
  return std::make_unique<base::Value>(proto.value());
}

// Convert a StringListPolicyProto to a List base::Value, where each list value
// is of Type::STRING.
std::unique_ptr<base::Value> DecodeStringListProto(
    const em::StringListPolicyProto& proto) {
  auto list_value = std::make_unique<base::ListValue>();
  for (const auto& entry : proto.value().entries())
    list_value->AppendString(entry);
  return std::move(list_value);
}

// Convert a StringPolicyProto to a base::Value of any type (for example,
// Type::DICTIONARY or Type::LIST) by parsing it as JSON.
std::unique_ptr<base::Value> DecodeJsonProto(const em::StringPolicyProto& proto,
                                             std::string* error) {
  const std::string& json = proto.value();
  std::unique_ptr<base::Value> parsed_value =
      base::JSONReader::ReadAndReturnErrorDeprecated(
          json, base::JSON_ALLOW_TRAILING_COMMAS, nullptr, error);

  if (!parsed_value) {
    // Can't parse as JSON so return it as a string, and leave it to the handler
    // to validate.
    LOG(WARNING) << "Invalid JSON: " << json;
    return std::make_unique<base::Value>(json);
  }

  // Accept any Value type that parsed as JSON, and leave it to the handler to
  // convert and check the concrete type.
  error->clear();
  return parsed_value;
}

}  // namespace

void DecodeProtoFields(
    const em::CloudPolicySettings& policy,
    base::WeakPtr<CloudExternalDataManager> external_data_manager,
    PolicySource source,
    PolicyScope scope,
    PolicyMap* map) {
  PolicyLevel level;

  // Access arrays are terminated by a struct that contains only nullptrs.
  for (const BooleanPolicyAccess* access = &kBooleanPolicyAccess[0];
       access->policy_key; access++) {
    if (!(policy.*access->has_proto)())
      continue;

    const em::BooleanPolicyProto& proto = (policy.*access->get_proto)();
    if (!GetPolicyLevel(proto, &level))
      continue;

    map->Set(access->policy_key, level, scope, source,
             DecodeBooleanProto(proto), nullptr);
  }

  for (const IntegerPolicyAccess* access = &kIntegerPolicyAccess[0];
       access->policy_key; access++) {
    if (!(policy.*access->has_proto)())
      continue;

    const em::IntegerPolicyProto& proto = (policy.*access->get_proto)();
    if (!GetPolicyLevel(proto, &level))
      continue;

    std::string error;
    map->Set(access->policy_key, level, scope, source,
             DecodeIntegerProto(proto, &error), nullptr);
    if (!error.empty())
      map->AddError(access->policy_key, error);
  }

  for (const StringPolicyAccess* access = &kStringPolicyAccess[0];
       access->policy_key; access++) {
    if (!(policy.*access->has_proto)())
      continue;

    const em::StringPolicyProto& proto = (policy.*access->get_proto)();
    if (!GetPolicyLevel(proto, &level))
      continue;

    std::string error;
    std::unique_ptr<base::Value> value =
        (access->type == StringPolicyType::STRING)
            ? DecodeStringProto(proto)
            : DecodeJsonProto(proto, &error);

    std::unique_ptr<ExternalDataFetcher> external_data_fetcher =
        (access->type == StringPolicyType::EXTERNAL)
            ? std::make_unique<ExternalDataFetcher>(external_data_manager,
                                                    access->policy_key)
            : nullptr;

    map->Set(access->policy_key, level, scope, source, std::move(value),
             std::move(external_data_fetcher));
    if (!error.empty())
      map->AddError(access->policy_key, error);
  }

  for (const StringListPolicyAccess* access = &kStringListPolicyAccess[0];
       access->policy_key; access++) {
    if (!(policy.*access->has_proto)())
      continue;

    const em::StringListPolicyProto& proto = (policy.*access->get_proto)();
    if (!GetPolicyLevel(proto, &level))
      continue;

    map->Set(access->policy_key, level, scope, source,
             DecodeStringListProto(proto), nullptr);
  }
}

}  // namespace policy
