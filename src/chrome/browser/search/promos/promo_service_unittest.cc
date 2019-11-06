// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/promos/promo_service.h"

#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "chrome/browser/search/promos/promo_data.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_service_manager_context.h"
#include "services/data_decoder/public/cpp/testing_json_parser.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::Eq;
using testing::StartsWith;

class PromoServiceTest : public testing::Test {
 public:
  PromoServiceTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  void SetUp() override {
    testing::Test::SetUp();

    service_ = std::make_unique<PromoService>(test_shared_loader_factory_);
  }

  void SetUpResponseWithData(const GURL& load_url,
                             const std::string& response) {
    test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {}));
    test_url_loader_factory_.AddResponse(load_url.spec(), response);
  }

  void SetUpResponseWithNetworkError(const GURL& load_url) {
    test_url_loader_factory_.AddResponse(
        load_url, network::ResourceResponseHead(), std::string(),
        network::URLLoaderCompletionStatus(net::HTTP_NOT_FOUND));
  }

  PromoService* service() { return service_.get(); }

 private:
  // Required to run tests from UI and threads.
  content::BrowserTaskEnvironment task_environment_;

  // Required to use SafeJsonParser.
  content::TestServiceManagerContext service_manager_context_;

  data_decoder::TestingJsonParser::ScopedFactoryOverride factory_override_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;

  std::unique_ptr<PromoService> service_;
};

TEST_F(PromoServiceTest, PromoDataNetworkError) {
  SetUpResponseWithNetworkError(service()->GetLoadURLForTesting());

  ASSERT_EQ(service()->promo_data(), base::nullopt);

  service()->Refresh();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(service()->promo_data(), base::nullopt);
  EXPECT_EQ(service()->promo_status(), PromoService::Status::TRANSIENT_ERROR);
}

TEST_F(PromoServiceTest, BadPromoResponse) {
  SetUpResponseWithData(service()->GetLoadURLForTesting(),
                        "{\"update\":{\"promotions\":{}}}");

  ASSERT_EQ(service()->promo_data(), base::nullopt);

  service()->Refresh();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(service()->promo_data(), base::nullopt);
  EXPECT_EQ(service()->promo_status(), PromoService::Status::FATAL_ERROR);
}

TEST_F(PromoServiceTest, BadPromoResponseNoLogUrl) {
  SetUpResponseWithData(
      service()->GetLoadURLForTesting(),
      "{\"update\":{\"promos\":{\"middle\":\"<style></style><div><script></"
      "script></div>\"}}}");

  ASSERT_EQ(service()->promo_data(), base::nullopt);

  service()->Refresh();
  base::RunLoop().RunUntilIdle();

  PromoData data;
  EXPECT_EQ(service()->promo_data(), data);
  EXPECT_EQ(service()->promo_status(), PromoService::Status::OK_WITHOUT_PROMO);
}

TEST_F(PromoServiceTest, PromoResponseMissingData) {
  SetUpResponseWithData(service()->GetLoadURLForTesting(),
                        "{\"update\":{\"promos\":{}}}");

  ASSERT_EQ(service()->promo_data(), base::nullopt);

  service()->Refresh();
  base::RunLoop().RunUntilIdle();

  PromoData data;
  EXPECT_EQ(service()->promo_data(), data);
  EXPECT_EQ(service()->promo_status(), PromoService::Status::OK_WITHOUT_PROMO);
}

TEST_F(PromoServiceTest, GoodPromoResponse) {
  std::string response_string =
      "{\"update\":{\"promos\":{\"middle\":\"<style></style><div><script></"
      "script></div>\", \"log_url\":\"/log_url?param=1\"}}}";
  PromoData promo;
  promo.promo_html = "<style></style><div><script></script></div>";
  promo.promo_log_url = GURL("https://www.google.com/log_url?param=1");

  SetUpResponseWithData(service()->GetLoadURLForTesting(), response_string);

  ASSERT_EQ(service()->promo_data(), base::nullopt);

  service()->Refresh();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(service()->promo_data(), promo);
  EXPECT_EQ(service()->promo_status(), PromoService::Status::OK_WITH_PROMO);
}
