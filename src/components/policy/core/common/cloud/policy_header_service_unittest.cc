// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/policy_header_service.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/values.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {
using enterprise_management::PolicyData;

namespace {
const char kDMServerURL[] = "http://server_url";
const char kPolicyHeaderName[] = "Chrome-Policy-Posture";

class TestCloudPolicyStore : public MockCloudPolicyStore {
 public:
  void SetPolicy(std::unique_ptr<PolicyData> policy) {
    policy_ = std::move(policy);
    // Notify observers.
    NotifyStoreLoaded();
  }
};

class PolicyHeaderServiceTest : public testing::Test {
 public:
  PolicyHeaderServiceTest() {
    task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  }
  ~PolicyHeaderServiceTest() override {}

  void SetUp() override {
    service_.reset(new PolicyHeaderService(kDMServerURL,
                                           kPolicyVerificationKeyHash,
                                           &user_store_));
  }

  void TearDown() override {
    task_runner_->RunUntilIdle();
    // Helper should outlive the service.
    service_.reset();
  }

  void ValidateHeader(const std::string& header_name,
                      const std::string& header_value,
                      const std::string& expected_dmtoken,
                      const std::string& expected_policy_token) {
    EXPECT_EQ(kPolicyHeaderName, header_name);

    // Decode the base64 value into JSON.
    std::string decoded;
    base::Base64Decode(header_value, &decoded);

    // Parse the JSON.
    std::unique_ptr<base::Value> value =
        base::JSONReader::ReadDeprecated(decoded);
    EXPECT_TRUE(value);
    base::DictionaryValue* dict;
    EXPECT_TRUE(value->GetAsDictionary(&dict));

    // Read the values and verify them vs the expected values.
    std::string dm_token;
    dict->GetString("user_dmtoken", &dm_token);
    EXPECT_EQ(dm_token, expected_dmtoken);
    std::string policy_token;
    dict->GetString("user_policy_token", &policy_token);
    EXPECT_EQ(policy_token, expected_policy_token);
  }

  base::test::ScopedTaskEnvironment task_environment_;
  std::unique_ptr<PolicyHeaderService> service_;
  TestCloudPolicyStore user_store_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
};

void AddHeader(std::string* header_name,
               std::string* header_value,
               const std::string& name,
               const std::string& value) {
  *header_name = name;
  *header_value = value;
}

void ExpectNoHeaderAdded(const std::string&, const std::string&) {
  FAIL();
}

}  // namespace

TEST_F(PolicyHeaderServiceTest, TestCreationAndShutdown) {
  // Just tests that the objects can be created and shutdown properly.
  EXPECT_TRUE(service_);
}

TEST_F(PolicyHeaderServiceTest, TestWithAndWithoutPolicyHeader) {
  // Set policy.
  std::unique_ptr<PolicyData> policy(new PolicyData());
  std::string expected_dmtoken = "expected_dmtoken";
  std::string expected_policy_token = "expected_dmtoken";
  policy->set_request_token(expected_dmtoken);
  policy->set_policy_token(expected_policy_token);
  user_store_.SetPolicy(std::move(policy));
  task_runner_->RunUntilIdle();
  std::string header_name, header_value;
  service_->AddPolicyHeaders(
      GURL(kDMServerURL),
      base::BindOnce(AddHeader, base::Unretained(&header_name),
                     base::Unretained(&header_value)));
  ValidateHeader(header_name, header_value, expected_dmtoken,
                 expected_policy_token);

  // Now blow away the policy data.
  user_store_.SetPolicy(std::unique_ptr<PolicyData>());
  task_runner_->RunUntilIdle();
  service_->AddPolicyHeaders(GURL(kDMServerURL),
                             base::BindOnce(&ExpectNoHeaderAdded));
}

TEST_F(PolicyHeaderServiceTest, NoHeaderOnNonMatchingURL) {
  service_->SetHeaderForTest("new_header");
  service_->AddPolicyHeaders(GURL("http://non-matching.com"),
                             base::BindOnce(&ExpectNoHeaderAdded));
}

TEST_F(PolicyHeaderServiceTest, HeaderChange) {
  std::string new_header = "new_header";
  service_->SetHeaderForTest(new_header);
  std::string header_name, header_value;
  service_->AddPolicyHeaders(
      GURL(kDMServerURL),
      base::BindOnce(AddHeader, base::Unretained(&header_name),
                     base::Unretained(&header_value)));
  EXPECT_EQ(kPolicyHeaderName, header_name);
  EXPECT_EQ(new_header, header_value);
}

TEST_F(PolicyHeaderServiceTest, ChangeToNoHeader) {
  service_->SetHeaderForTest("");
  service_->AddPolicyHeaders(GURL(kDMServerURL),
                             base::BindOnce(&ExpectNoHeaderAdded));
}

}  // namespace policy
