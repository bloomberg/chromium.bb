// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_loader_command_line.h"

#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/core/common/policy_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class PolicyLoaderCommandLineTest : public ::testing::Test {
 public:
  void SetCommandLinePolicy(const std::string& policies) {
    command_line_.AppendSwitchASCII(switches::kChromePolicy, policies);
  }

  void LoadAndVerifyPolicies(PolicyLoaderCommandLine* loader,
                             const base::Value& expected_policies) {
    DCHECK(expected_policies.is_dict());
    std::unique_ptr<PolicyBundle> bundle = loader->Load();
    PolicyMap& map =
        bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
    EXPECT_EQ(expected_policies.DictSize(), map.size());
    for (auto expected_policy : expected_policies.DictItems()) {
      const PolicyMap::Entry* actual_policy = map.Get(expected_policy.first);
      ASSERT_TRUE(actual_policy);
      EXPECT_EQ(POLICY_LEVEL_MANDATORY, actual_policy->level);
      EXPECT_EQ(POLICY_SCOPE_MACHINE, actual_policy->scope);
      EXPECT_EQ(POLICY_SOURCE_COMMAND_LINE, actual_policy->source);

      ASSERT_TRUE(actual_policy->value());
      EXPECT_EQ(expected_policy.second, *(actual_policy->value()));
    }
  }

  std::unique_ptr<PolicyLoaderCommandLine> CreatePolicyLoader() {
    return std::make_unique<PolicyLoaderCommandLine>(command_line_);
  }

 private:
  base::CommandLine command_line_{base::CommandLine::NO_PROGRAM};
  base::test::TaskEnvironment task_environment_;
};

TEST_F(PolicyLoaderCommandLineTest, NoSwitch) {
  LoadAndVerifyPolicies(CreatePolicyLoader().get(),
                        base::Value(base::Value::Type::DICTIONARY));
}

TEST_F(PolicyLoaderCommandLineTest, InvalidSwitch) {
  SetCommandLinePolicy("a");
  LoadAndVerifyPolicies(CreatePolicyLoader().get(),
                        base::Value(base::Value::Type::DICTIONARY));

  SetCommandLinePolicy("a:b");
  LoadAndVerifyPolicies(CreatePolicyLoader().get(),
                        base::Value(base::Value::Type::DICTIONARY));

  SetCommandLinePolicy("[a]");
  LoadAndVerifyPolicies(CreatePolicyLoader().get(),
                        base::Value(base::Value::Type::DICTIONARY));
}

TEST_F(PolicyLoaderCommandLineTest, ParseSwitchValue) {
  SetCommandLinePolicy(R"({
    "int_policy": 42,
    "string_policy": "string",
    "bool_policy": true,
    "list_policy": [1,2],
    "dict_policy": {"k1":1, "k2": {"k3":true}}
  })");
  base::Value policies(base::Value::Type::DICTIONARY);
  policies.SetIntKey("int_policy", 42);
  policies.SetStringKey("string_policy", "string");
  policies.SetBoolKey("bool_policy", true);

  // list policy
  base::Value::ListStorage list_storage;
  list_storage.emplace_back(1);
  list_storage.emplace_back(2);
  policies.SetKey("list_policy", base::Value(list_storage));

  // dict policy
  policies.SetIntPath({"dict_policy.k1"}, 1);
  policies.SetBoolPath({"dict_policy.k2.k3"}, true);

  LoadAndVerifyPolicies(CreatePolicyLoader().get(), policies);
}

}  // namespace policy
