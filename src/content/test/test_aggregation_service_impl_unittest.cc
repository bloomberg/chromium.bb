// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_aggregation_service_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/aggregation_service/public_key.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class TestAggregationServiceImplTest : public testing::Test {
 public:
  TestAggregationServiceImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        service_impl_(std::make_unique<TestAggregationServiceImpl>(
            task_environment_.GetMockClock(),
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_))) {}

 private:
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;

 protected:
  std::unique_ptr<TestAggregationServiceImpl> service_impl_;
};

TEST_F(TestAggregationServiceImplTest, SetPublicKeys) {
  aggregation_service::TestHpkeKey generated_key =
      aggregation_service::GenerateKey("abcd");

  std::string json_string = base::ReplaceStringPlaceholders(
      R"({
            "version": "",
            "keys": [
                {
                   "id": "abcd",
                   "key": "$1"
                }
            ]
         })",
      {generated_key.base64_encoded_public_key}, /*offsets=*/nullptr);

  url::Origin origin = url::Origin::Create(GURL("https://a.com"));

  service_impl_->SetPublicKeys(origin, json_string,
                               base::BindLambdaForTesting([&](bool succeeded) {
                                 EXPECT_TRUE(succeeded);
                               }));

  base::RunLoop run_loop;
  service_impl_->GetPublicKeys(
      origin, base::BindLambdaForTesting([&](std::vector<PublicKey> keys) {
        EXPECT_TRUE(content::aggregation_service::PublicKeysEqual(
            {generated_key.public_key}, keys));
        run_loop.Quit();
      }));
  run_loop.Run();
}

}  // namespace content
