// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_TEST_UTILS_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_TEST_UTILS_H_

#include <map>
#include <ostream>
#include <string>

#include "base/macros.h"
#include "build/build_config.h"
#include "components/policy/core/common/policy_details.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_types.h"

namespace policy {

class PolicyBundle;
struct PolicyNamespace;

// A mapping of policy names to PolicyDetails that can be used to set the
// PolicyDetails for test policies.
class PolicyDetailsMap {
 public:
  PolicyDetailsMap();
  ~PolicyDetailsMap();

  // The returned callback's lifetime is tied to |this| object.
  GetChromePolicyDetailsCallback GetCallback() const;

  // Does not take ownership of |details|.
  void SetDetails(const std::string& policy, const PolicyDetails* details);

 private:
  typedef std::map<std::string, const PolicyDetails*> PolicyDetailsMapping;

  const PolicyDetails* Lookup(const std::string& policy) const;

  PolicyDetailsMapping map_;

  DISALLOW_COPY_AND_ASSIGN(PolicyDetailsMap);
};

// Returns true if |service| is not serving any policies. Otherwise logs the
// current policies and returns false.
bool PolicyServiceIsEmpty(const PolicyService* service);

#if defined(OS_IOS) || defined(OS_MACOSX)

// Converts a base::Value to the equivalent CFPropertyListRef.
// The returned value is owned by the caller.
CFPropertyListRef ValueToProperty(const base::Value& value);

#endif

}  // namespace policy

std::ostream& operator<<(std::ostream& os, const policy::PolicyBundle& bundle);
std::ostream& operator<<(std::ostream& os, policy::PolicyScope scope);
std::ostream& operator<<(std::ostream& os, policy::PolicyLevel level);
std::ostream& operator<<(std::ostream& os, policy::PolicyDomain domain);
std::ostream& operator<<(std::ostream& os, const policy::PolicyMap& policies);
std::ostream& operator<<(std::ostream& os, const policy::PolicyMap::Entry& e);
std::ostream& operator<<(std::ostream& os, const policy::PolicyNamespace& ns);

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_TEST_UTILS_H_
