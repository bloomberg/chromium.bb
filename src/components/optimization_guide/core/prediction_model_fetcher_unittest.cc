// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/prediction_model_fetcher_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace optimization_guide {

constexpr char optimization_guide_service_url[] =
    "https://optimizationguideservice.com/";

class PredictionModelFetcherTest : public testing::Test {
 public:
  PredictionModelFetcherTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI),
        shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        network_tracker_(network::TestNetworkConnectionTracker::GetInstance()) {
    prediction_model_fetcher_ = std::make_unique<PredictionModelFetcherImpl>(
        shared_url_loader_factory_, GURL(optimization_guide_service_url),
        network_tracker_);
  }

  PredictionModelFetcherTest(const PredictionModelFetcherTest&) = delete;
  PredictionModelFetcherTest& operator=(const PredictionModelFetcherTest&) =
      delete;

  ~PredictionModelFetcherTest() override {}

  void OnModelsFetched(absl::optional<std::unique_ptr<proto::GetModelsResponse>>
                           get_models_response) {
    if (get_models_response)
      models_fetched_ = true;
  }

  bool models_fetched() { return models_fetched_; }

  void SetConnectionOffline() {
    network_tracker_->SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_NONE);
  }

  void SetConnectionOnline() {
    network_tracker_->SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_4G);
  }

 protected:
  bool FetchModels(const std::vector<proto::ModelInfo> models_request_info,
                   const std::vector<proto::FieldTrial>& active_field_trials,
                   proto::RequestContext request_context,
                   const std::string& locale) {
    bool status =
        prediction_model_fetcher_->FetchOptimizationGuideServiceModels(
            models_request_info, active_field_trials, request_context, locale,
            base::BindOnce(&PredictionModelFetcherTest::OnModelsFetched,
                           base::Unretained(this)));
    RunUntilIdle();
    return status;
  }

  // Return a 200 response with provided content to any pending requests.
  bool SimulateResponse(const std::string& content,
                        net::HttpStatusCode http_status) {
    return test_url_loader_factory_.SimulateResponseForPendingRequest(
        optimization_guide_service_url, content, http_status,
        network::TestURLLoaderFactory::kUrlMatchPrefix);
  }

  void VerifyHasPendingFetchRequests() {
    EXPECT_GE(test_url_loader_factory_.NumPending(), 1);
    std::string key_value;
    for (const auto& pending_request :
         *test_url_loader_factory_.pending_requests()) {
      EXPECT_EQ(pending_request.request.method, "POST");
      EXPECT_TRUE(net::GetValueForKeyInQuery(pending_request.request.url, "key",
                                             &key_value));
    }
  }

 private:
  void RunUntilIdle() {
    task_environment_.RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

  bool models_fetched_ = false;
  base::test::TaskEnvironment task_environment_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};

  std::unique_ptr<PredictionModelFetcherImpl> prediction_model_fetcher_;

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  raw_ptr<network::TestNetworkConnectionTracker> network_tracker_;
};

TEST_F(PredictionModelFetcherTest, FetchOptimizationGuideServiceModels) {
  std::string response_content;
  proto::ModelInfo model_info;
  model_info.set_optimization_target(
      proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  EXPECT_TRUE(FetchModels({model_info}, /*active_field_trials=*/{},
                          proto::RequestContext::CONTEXT_BATCH_UPDATE_MODELS,
                          "en-US"));
  VerifyHasPendingFetchRequests();

  EXPECT_TRUE(SimulateResponse(response_content, net::HTTP_OK));
  EXPECT_TRUE(models_fetched());
}

// Tests 404 response from request.
TEST_F(PredictionModelFetcherTest, FetchReturned404) {
  base::HistogramTester histogram_tester;
  std::string response_content;

  proto::ModelInfo model_info;
  model_info.set_optimization_target(
      proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  EXPECT_TRUE(FetchModels({model_info}, /*active_field_trials=*/{},
                          proto::RequestContext::CONTEXT_BATCH_UPDATE_MODELS,
                          "en-US"));
  // Send a 404 to HintsFetcher.
  SimulateResponse(response_content, net::HTTP_NOT_FOUND);
  EXPECT_FALSE(models_fetched());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelFetcher."
      "GetModelsResponse.Status",
      net::HTTP_NOT_FOUND, 1);

  // Net error codes are negative but UMA histograms require positive values.
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelFetcher."
      "GetModelsResponse.NetErrorCode",
      -net::ERR_HTTP_RESPONSE_CODE_FAILURE, 1);
}

TEST_F(PredictionModelFetcherTest, FetchReturnBadResponse) {
  std::string response_content = "not proto";

  proto::ModelInfo model_info;
  model_info.set_optimization_target(
      proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  EXPECT_TRUE(FetchModels({model_info}, /*active_field_trials=*/{},
                          proto::RequestContext::CONTEXT_BATCH_UPDATE_MODELS,
                          "en-US"));
  VerifyHasPendingFetchRequests();
  EXPECT_TRUE(SimulateResponse(response_content, net::HTTP_OK));
  EXPECT_FALSE(models_fetched());
}

TEST_F(PredictionModelFetcherTest, FetchAttemptWhenNetworkOffline) {
  SetConnectionOffline();
  std::string response_content;
  proto::ModelInfo model_info;
  model_info.set_optimization_target(
      proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  EXPECT_FALSE(FetchModels({model_info}, /*active_field_trials=*/{},
                           proto::RequestContext::CONTEXT_BATCH_UPDATE_MODELS,
                           "en-US"));
  EXPECT_FALSE(models_fetched());

  SetConnectionOnline();
  EXPECT_TRUE(FetchModels({model_info}, /*active_field_trials=*/{},
                          proto::RequestContext::CONTEXT_BATCH_UPDATE_MODELS,
                          "en-US"));
  VerifyHasPendingFetchRequests();
  EXPECT_TRUE(SimulateResponse(response_content, net::HTTP_OK));
  EXPECT_TRUE(models_fetched());
}

TEST_F(PredictionModelFetcherTest, EmptyModelInfo) {
  base::HistogramTester histogram_tester;
  std::string response_content;
  proto::FieldTrial field_trial;
  field_trial.set_name_hash(123);
  EXPECT_FALSE(FetchModels(/*models_request_info=*/{}, {field_trial},
                           proto::RequestContext::CONTEXT_BATCH_UPDATE_MODELS,
                           "en-US"));

  EXPECT_FALSE(models_fetched());
}

}  // namespace optimization_guide
