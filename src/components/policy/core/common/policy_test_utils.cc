// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_test_utils.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/policy_constants.h"

#if defined(OS_APPLE)
#include <CoreFoundation/CoreFoundation.h>

#include "base/mac/scoped_cftyperef.h"
#endif

namespace policy {

PolicyDetailsMap::PolicyDetailsMap() {}

PolicyDetailsMap::~PolicyDetailsMap() {}

GetChromePolicyDetailsCallback PolicyDetailsMap::GetCallback() const {
  return base::BindRepeating(&PolicyDetailsMap::Lookup, base::Unretained(this));
}

void PolicyDetailsMap::SetDetails(const std::string& policy,
                                  const PolicyDetails* details) {
  map_[policy] = details;
}

const PolicyDetails* PolicyDetailsMap::Lookup(const std::string& policy) const {
  auto it = map_.find(policy);
  return it == map_.end() ? NULL : it->second;
}

bool PolicyServiceIsEmpty(const PolicyService* service) {
  const PolicyMap& map = service->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  if (!map.empty()) {
    base::DictionaryValue dict;
    for (auto it = map.begin(); it != map.end(); ++it)
      dict.SetKey(it->first, it->second.value()->Clone());
    LOG(WARNING) << "There are pre-existing policies in this machine: " << dict;
#if defined(OS_WIN)
    LOG(WARNING) << "From: " << kRegistryChromePolicyKey;
#endif
  }
  return map.empty();
}

#if defined(OS_APPLE)
CFPropertyListRef ValueToProperty(const base::Value& value) {
  switch (value.type()) {
    case base::Value::Type::NONE:
      return kCFNull;

    case base::Value::Type::BOOLEAN:
      return value.GetBool() ? kCFBooleanTrue : kCFBooleanFalse;

    case base::Value::Type::INTEGER: {
      const int int_value = value.GetInt();
      return CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &int_value);
    }

    case base::Value::Type::DOUBLE: {
      const double double_value = value.GetDouble();
      return CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType,
                            &double_value);
    }

    case base::Value::Type::STRING: {
      const std::string& string_value = value.GetString();
      return base::SysUTF8ToCFStringRef(string_value).release();
    }

    case base::Value::Type::DICTIONARY: {
      // |dict| is owned by the caller.
      CFMutableDictionaryRef dict = CFDictionaryCreateMutable(
          kCFAllocatorDefault, value.DictSize(), &kCFTypeDictionaryKeyCallBacks,
          &kCFTypeDictionaryValueCallBacks);
      for (const auto key_value_pair : value.DictItems()) {
        // CFDictionaryAddValue() retains both |key| and |value|, so make sure
        // the references are balanced.
        base::ScopedCFTypeRef<CFStringRef> key(
            base::SysUTF8ToCFStringRef(key_value_pair.first));
        base::ScopedCFTypeRef<CFPropertyListRef> cf_value(
            ValueToProperty(key_value_pair.second));
        if (cf_value)
          CFDictionaryAddValue(dict, key, cf_value);
      }
      return dict;
    }

    case base::Value::Type::LIST: {
      base::Value::ConstListView list_view = value.GetList();
      CFMutableArrayRef array =
          CFArrayCreateMutable(NULL, list_view.size(), &kCFTypeArrayCallBacks);
      for (const base::Value& entry : list_view) {
        // CFArrayAppendValue() retains |cf_value|, so make sure the reference
        // created by ValueToProperty() is released.
        base::ScopedCFTypeRef<CFPropertyListRef> cf_value(
            ValueToProperty(entry));
        if (cf_value)
          CFArrayAppendValue(array, cf_value);
      }
      return array;
    }

    case base::Value::Type::BINARY:
      // This type isn't converted (though it can be represented as CFData)
      // because there's no equivalent JSON type, and policy values can only
      // take valid JSON values.
      break;
  }

  return NULL;
}
#endif  // defined(OS_APPLE)

}  // namespace policy

std::ostream& operator<<(std::ostream& os, const policy::PolicyBundle& bundle) {
  os << "{" << std::endl;
  for (const auto& entry : bundle)
    os << "  \"" << entry.first << "\": " << entry.second << "," << std::endl;
  os << "}";
  return os;
}

std::ostream& operator<<(std::ostream& os, policy::PolicyScope scope) {
  switch (scope) {
    case policy::POLICY_SCOPE_USER:
      return os << "POLICY_SCOPE_USER";
    case policy::POLICY_SCOPE_MACHINE:
      return os << "POLICY_SCOPE_MACHINE";
  }
  return os << "POLICY_SCOPE_UNKNOWN(" << int(scope) << ")";
}

std::ostream& operator<<(std::ostream& os, policy::PolicyLevel level) {
  switch (level) {
    case policy::POLICY_LEVEL_RECOMMENDED:
      return os << "POLICY_LEVEL_RECOMMENDED";
    case policy::POLICY_LEVEL_MANDATORY:
      return os << "POLICY_LEVEL_MANDATORY";
  }
  return os << "POLICY_LEVEL_UNKNOWN(" << int(level) << ")";
}

std::ostream& operator<<(std::ostream& os, policy::PolicyDomain domain) {
  switch (domain) {
    case policy::POLICY_DOMAIN_CHROME:
      return os << "POLICY_DOMAIN_CHROME";
    case policy::POLICY_DOMAIN_EXTENSIONS:
      return os << "POLICY_DOMAIN_EXTENSIONS";
    case policy::POLICY_DOMAIN_SIGNIN_EXTENSIONS:
      return os << "POLICY_DOMAIN_SIGNIN_EXTENSIONS";
    case policy::POLICY_DOMAIN_SIZE:
      break;
  }
  return os << "POLICY_DOMAIN_UNKNOWN(" << int(domain) << ")";
}

std::ostream& operator<<(std::ostream& os, const policy::PolicyMap& policies) {
  os << "{" << std::endl;
  for (const auto& iter : policies)
    os << "  \"" << iter.first << "\": " << iter.second << "," << std::endl;
  os << "}";
  return os;
}

std::ostream& operator<<(std::ostream& os, const policy::PolicyMap::Entry& e) {
  return os << "{" << std::endl
            << "  \"level\": " << e.level << "," << std::endl
            << "  \"scope\": " << e.scope << "," << std::endl
            << "  \"value\": " << *e.value() << "}";
}

std::ostream& operator<<(std::ostream& os, const policy::PolicyNamespace& ns) {
  return os << ns.domain << "/" << ns.component_id;
}
