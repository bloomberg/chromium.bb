// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/test/scoped_task_environment.h"
#include "services/network/origin_policy/origin_policy_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
class OriginPolicyManagerTest : public testing::Test {
 public:
  OriginPolicyManagerTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO),
        manager_(std::make_unique<OriginPolicyManager>()) {}

 protected:
  // testing::Test implementation.
  void SetUp() override {}

  base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::unique_ptr<OriginPolicyManager> manager_;

  DISALLOW_COPY_AND_ASSIGN(OriginPolicyManagerTest);
};

TEST_F(OriginPolicyManagerTest, AddBinding) {
  mojom::OriginPolicyManagerPtr origin_policy_ptr;
  mojom::OriginPolicyManagerRequest origin_policy_request(
      mojo::MakeRequest(&origin_policy_ptr));

  EXPECT_EQ(0u, manager_->GetBindingsForTesting().size());

  manager_->AddBinding(std::move(origin_policy_request));

  EXPECT_EQ(1u, manager_->GetBindingsForTesting().size());
}

}  // namespace network
