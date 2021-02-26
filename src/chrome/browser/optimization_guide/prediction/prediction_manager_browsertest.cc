// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/base64.h"
#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_session_statistic.h"
#include "chrome/browser/optimization_guide/prediction/prediction_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/services/machine_learning/public/cpp/service_connection.h"
#include "chrome/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/optimization_guide/optimization_guide_constants.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/optimization_guide_store.h"
#include "components/optimization_guide/optimization_guide_switches.h"
#include "components/optimization_guide/optimization_guide_test_util.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/optimization_guide/store_update_data.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/previews/core/previews_switches.h"
#include "components/variations/hashing.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/network_connection_change_simulator.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace {

// Fetch and calculate the total number of samples from all the bins for
// |histogram_name|. Note: from some browertests run (such as chromeos) there
// might be two profiles created, and this will return the total sample count
// across profiles.
int GetTotalHistogramSamples(const base::HistogramTester* histogram_tester,
                             const std::string& histogram_name) {
  std::vector<base::Bucket> buckets =
      histogram_tester->GetAllSamples(histogram_name);
  int total = 0;
  for (const auto& bucket : buckets)
    total += bucket.count;

  return total;
}

// Retries fetching |histogram_name| until it contains at least |count| samples.
void RetryForHistogramUntilCountReached(
    const base::HistogramTester* histogram_tester,
    const std::string& histogram_name,
    int count) {
  while (true) {
    base::ThreadPoolInstance::Get()->FlushForTesting();
    base::RunLoop().RunUntilIdle();

    content::FetchHistogramsFromChildProcesses();
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

    int total = GetTotalHistogramSamples(histogram_tester, histogram_name);
    if (total >= count)
      return;
  }
}

std::unique_ptr<optimization_guide::proto::PredictionModel>
GetValidDecisionTreePredictionModel() {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      GetMinimalDecisionTreePredictionModel(/* threshold= */ 5.0,
                                            /* weight= */ 2.0);

  optimization_guide::proto::DecisionTree* decision_tree_model =
      prediction_model->mutable_model()->mutable_decision_tree();

  optimization_guide::proto::TreeNode* tree_node =
      decision_tree_model->mutable_nodes(0);
  tree_node->mutable_binary_node()->mutable_left_child_id()->set_value(1);
  tree_node->mutable_binary_node()->mutable_right_child_id()->set_value(2);
  tree_node->mutable_binary_node()
      ->mutable_inequality_left_child_test()
      ->mutable_feature_id()
      ->mutable_id()
      ->set_value("agg1");
  tree_node->mutable_binary_node()
      ->mutable_inequality_left_child_test()
      ->set_type(optimization_guide::proto::InequalityTest::LESS_OR_EQUAL);
  tree_node->mutable_binary_node()
      ->mutable_inequality_left_child_test()
      ->mutable_threshold()
      ->set_float_value(1.0);

  tree_node = decision_tree_model->add_nodes();
  tree_node->mutable_node_id()->set_value(1);
  tree_node->mutable_leaf()->mutable_vector()->add_value()->set_double_value(
      2.);

  tree_node = decision_tree_model->add_nodes();
  tree_node->mutable_node_id()->set_value(2);
  tree_node->mutable_leaf()->mutable_vector()->add_value()->set_double_value(
      4.);

  return prediction_model;
}

std::unique_ptr<optimization_guide::proto::PredictionModel>
GetValidEnsemblePredictionModel() {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      std::make_unique<optimization_guide::proto::PredictionModel>();
  prediction_model->mutable_model()->mutable_threshold()->set_value(5.0);

  optimization_guide::proto::Model valid_decision_tree_model =
      GetValidDecisionTreePredictionModel()->model();
  optimization_guide::proto::Ensemble* ensemble =
      prediction_model->mutable_model()->mutable_ensemble();
  *ensemble->add_members()->mutable_submodel() = valid_decision_tree_model;
  *ensemble->add_members()->mutable_submodel() = valid_decision_tree_model;
  return prediction_model;
}

std::unique_ptr<optimization_guide::proto::PredictionModel>
CreatePredictionModel() {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      GetValidEnsemblePredictionModel();

  optimization_guide::proto::ModelInfo* model_info =
      prediction_model->mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_features(
      optimization_guide::proto::
          CLIENT_MODEL_FEATURE_EFFECTIVE_CONNECTION_TYPE);
  model_info->add_supported_host_model_features("agg1");
  model_info->set_optimization_target(
      optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  model_info->add_supported_model_types(
      optimization_guide::proto::ModelType::MODEL_TYPE_DECISION_TREE);
  return prediction_model;
}

std::unique_ptr<optimization_guide::proto::GetModelsResponse>
BuildGetModelsResponse(
    const std::vector<std::string>& hosts,
    const std::vector<optimization_guide::proto::ClientModelFeature>&
        client_model_features) {
  std::unique_ptr<optimization_guide::proto::GetModelsResponse>
      get_models_response =
          std::make_unique<optimization_guide::proto::GetModelsResponse>();

  for (const auto& host : hosts) {
    optimization_guide::proto::HostModelFeatures* host_model_features =
        get_models_response->add_host_model_features();
    host_model_features->set_host(host);
    optimization_guide::proto::ModelFeature* model_feature =
        host_model_features->add_model_features();
    model_feature->set_feature_name("agg1");
    model_feature->set_double_value(2.0);
  }

  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      CreatePredictionModel();
  for (const auto& client_model_feature : client_model_features) {
    prediction_model->mutable_model_info()->add_supported_model_features(
        client_model_feature);
  }
  prediction_model->mutable_model_info()->set_version(2);
  *get_models_response->add_models() = *prediction_model.get();

  return get_models_response;
}

enum class PredictionModelsFetcherRemoteResponseType {
  kSuccessfulWithModelsAndFeatures = 0,
  kSuccessfulWithFeaturesAndNoModels = 1,
  kSuccessfulWithModelsAndNoFeatures = 2,
  kUnsuccessful = 3,
};

// A WebContentsObserver that asks whether an optimization target can be
// applied.
class OptimizationGuideConsumerWebContentsObserver
    : public content::WebContentsObserver {
 public:
  explicit OptimizationGuideConsumerWebContentsObserver(
      content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}
  ~OptimizationGuideConsumerWebContentsObserver() override = default;

  // contents::WebContentsObserver implementation:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    OptimizationGuideKeyedService* service =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
    if (callback_) {
      // Intentionally do not set client model feature values to override to
      // make sure decisions are the same in both sync and async variants.
      service->ShouldTargetNavigationAsync(
          navigation_handle,
          optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, {},
          std::move(callback_));
    }
  }

  void set_callback(
      optimization_guide::OptimizationGuideTargetDecisionCallback callback) {
    callback_ = std::move(callback);
  }

 private:
  optimization_guide::OptimizationGuideTargetDecisionCallback callback_;
};

// A ServiceProcessHost::Observer that monitors ML Service launch events.
class MLServiceProcessObserver : public content::ServiceProcessHost::Observer {
 public:
  MLServiceProcessObserver() { content::ServiceProcessHost::AddObserver(this); }

  ~MLServiceProcessObserver() override {
    content::ServiceProcessHost::RemoveObserver(this);
  }

  MLServiceProcessObserver(const MLServiceProcessObserver&) = delete;
  MLServiceProcessObserver& operator=(const MLServiceProcessObserver&) = delete;

  // Whether the service is launched.
  int IsLaunched() const { return is_launched_; }

  // Launch |launch_wait_loop_| to wait until a service launch is detected.
  void WaitForLaunch() {
    if (!is_launched_)
      launch_wait_loop_.Run();
  }

  void OnServiceProcessLaunched(
      const content::ServiceProcessInfo& info) override {
    if (info.IsService<machine_learning::mojom::MachineLearningService>()) {
      is_launched_ = true;
      if (launch_wait_loop_.running())
        launch_wait_loop_.Quit();
    }
  }

 private:
  base::RunLoop launch_wait_loop_;
  bool is_launched_ = false;
};

}  // namespace

namespace optimization_guide {

// Abstract base class for browser testing Prediction Manager.
// Actual class fixtures should implement InitializeFeatureList to set up
// features used in tests.
class PredictionManagerBrowserTestBase : public InProcessBrowserTest {
 public:
  PredictionManagerBrowserTestBase() = default;
  ~PredictionManagerBrowserTestBase() override = default;

  PredictionManagerBrowserTestBase(const PredictionManagerBrowserTestBase&) =
      delete;
  PredictionManagerBrowserTestBase& operator=(
      const PredictionManagerBrowserTestBase&) = delete;

  void SetUp() override {
    InitializeFeatureList();

    models_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    models_server_->ServeFilesFromSourceDirectory("chrome/test/data/previews");
    models_server_->RegisterRequestHandler(base::BindRepeating(
        &PredictionManagerBrowserTestBase::HandleGetModelsRequest,
        base::Unretained(this)));

    ASSERT_TRUE(models_server_->Start());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    content::NetworkConnectionChangeSimulator().SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_2G);
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_->Start());
    https_url_with_content_ = https_server_->GetURL("/english_page.html");
    https_url_without_content_ = https_server_->GetURL("/empty.html");

    // Set up an OptimizationGuideKeyedService consumer.
    consumer_ = std::make_unique<OptimizationGuideConsumerWebContentsObserver>(
        browser()->tab_strip_model()->GetActiveWebContents());

    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(https_server_->ShutdownAndWaitUntilComplete());
    EXPECT_TRUE(models_server_->ShutdownAndWaitUntilComplete());
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch("enable-spdy-proxy-auth");
    cmd->AppendSwitch(optimization_guide::switches::
                          kFetchModelsAndHostModelFeaturesOverrideTimer);

    cmd->AppendSwitch(optimization_guide::switches::
                          kDisableCheckingUserPermissionsForTesting);
    cmd->AppendSwitchASCII(optimization_guide::switches::kFetchHintsOverride,
                           "whatever.com,somehost.com");
    cmd->AppendSwitchASCII(
        optimization_guide::switches::kOptimizationGuideServiceGetModelsURL,
        models_server_
            ->GetURL(GURL(optimization_guide::
                              kOptimizationGuideServiceGetModelsDefaultURL)
                         .host(),
                     "/")
            .spec());
    cmd->AppendSwitchASCII("host-rules", "MAP * 127.0.0.1");
    cmd->AppendSwitchASCII("force-variation-ids", "4");
  }

  void SetResponseType(
      PredictionModelsFetcherRemoteResponseType response_type) {
    response_type_ = response_type;
  }

  void RegisterWithKeyedService() {
    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->RegisterOptimizationTargets(
            {optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});
  }

  // Sets the callback on the consumer of the OptimizationGuideKeyedService. If
  // set, this will call the async version of ShouldTargetNavigation.
  void SetCallbackOnConsumer(
      optimization_guide::OptimizationGuideTargetDecisionCallback callback) {
    ASSERT_TRUE(consumer_);

    consumer_->set_callback(std::move(callback));
  }

  OptimizationGuideConsumerWebContentsObserver* consumer() {
    return consumer_.get();
  }

  PredictionManager* GetPredictionManager() {
    OptimizationGuideKeyedService* optimization_guide_keyed_service =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            browser()->profile());
    return optimization_guide_keyed_service->GetPredictionManager();
  }

  std::unique_ptr<page_load_metrics::PageLoadMetricsTestWaiter>
  CreatePageLoadMetricsTestWaiter() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
        web_contents);
  }

  void SetExpectedFieldTrialNames(
      const base::flat_set<uint32_t>& expected_field_trial_name_hashes) {
    expected_field_trial_name_hashes_ = expected_field_trial_name_hashes;
  }

  bool using_ml_service() const { return using_ml_service_; }
  GURL https_url_with_content() { return https_url_with_content_; }
  GURL https_url_without_content() { return https_url_without_content_; }

 protected:
  // Virtualize for testing different feature configurations.
  virtual void InitializeFeatureList() = 0;

  base::test::ScopedFeatureList scoped_feature_list_;

  // Feature that the model server should return in response to
  // GetModelsRequest.
  proto::ClientModelFeature client_model_feature_ =
      optimization_guide::proto::CLIENT_MODEL_FEATURE_SITE_ENGAGEMENT_SCORE;

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleGetModelsRequest(
      const net::test_server::HttpRequest& request) {
    std::unique_ptr<net::test_server::BasicHttpResponse> response;

    response = std::make_unique<net::test_server::BasicHttpResponse>();
    // The request to the remote Optimization Guide Service should always be a
    // POST.
    EXPECT_EQ(request.method, net::test_server::METHOD_POST);
    EXPECT_NE(request.headers.end(), request.headers.find("X-Client-Data"));
    optimization_guide::proto::GetModelsRequest models_request;
    EXPECT_TRUE(models_request.ParseFromString(request.content));
    // Make sure we actually filter field trials appropriately.
    EXPECT_EQ(expected_field_trial_name_hashes_.size(),
              static_cast<size_t>(models_request.active_field_trials_size()));
    base::flat_set<uint32_t> seen_field_trial_name_hashes;
    for (const auto& field_trial : models_request.active_field_trials()) {
      EXPECT_TRUE(
          expected_field_trial_name_hashes_.find(field_trial.name_hash()) !=
          expected_field_trial_name_hashes_.end());
      seen_field_trial_name_hashes.insert(field_trial.name_hash());
    }
    EXPECT_EQ(seen_field_trial_name_hashes.size(),
              expected_field_trial_name_hashes_.size());

    response->set_code(net::HTTP_OK);
    std::unique_ptr<optimization_guide::proto::GetModelsResponse>
        get_models_response = BuildGetModelsResponse(
            {"example1.com", https_server_->GetURL("/").host()},
            {client_model_feature_});
    if (response_type_ == PredictionModelsFetcherRemoteResponseType::
                              kSuccessfulWithFeaturesAndNoModels) {
      get_models_response->clear_models();
    } else if (response_type_ == PredictionModelsFetcherRemoteResponseType::
                                     kSuccessfulWithModelsAndNoFeatures) {
      get_models_response->clear_host_model_features();
    } else if (response_type_ ==
               PredictionModelsFetcherRemoteResponseType::kUnsuccessful) {
      response->set_code(net::HTTP_NOT_FOUND);
    }

    std::string serialized_response;
    get_models_response->SerializeToString(&serialized_response);
    response->set_content(serialized_response);
    return std::move(response);
  }

  bool using_ml_service_ = false;
  GURL https_url_with_content_, https_url_without_content_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unique_ptr<net::EmbeddedTestServer> models_server_;
  PredictionModelsFetcherRemoteResponseType response_type_ =
      PredictionModelsFetcherRemoteResponseType::
          kSuccessfulWithModelsAndFeatures;
  std::unique_ptr<OptimizationGuideConsumerWebContentsObserver> consumer_;
  base::flat_set<uint32_t> expected_field_trial_name_hashes_;
};

// Parametrized on whether the ML Service path is enabled.
class PredictionManagerBrowserTest
    : public PredictionManagerBrowserTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  PredictionManagerBrowserTest() : using_ml_service_(GetParam()) {}
  ~PredictionManagerBrowserTest() override = default;

  PredictionManagerBrowserTest(const PredictionManagerBrowserTest&) = delete;
  PredictionManagerBrowserTest& operator=(const PredictionManagerBrowserTest&) =
      delete;

  bool using_ml_service() const { return using_ml_service_; }

 private:
  void InitializeFeatureList() override {
    if (using_ml_service_) {
      scoped_feature_list_.InitWithFeatures(
          {optimization_guide::features::kOptimizationHints,
           optimization_guide::features::kRemoteOptimizationGuideFetching,
           optimization_guide::features::kOptimizationTargetPrediction,
           optimization_guide::features::
               kOptimizationTargetPredictionUsingMLService},
          {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {optimization_guide::features::kOptimizationHints,
           optimization_guide::features::kRemoteOptimizationGuideFetching,
           optimization_guide::features::kOptimizationTargetPrediction},
          {});
    }
  }

  bool using_ml_service_ = false;
};

#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_CHROMEOS)
#define DISABLE_ON_WIN_MAC_CHROMEOS(x) DISABLED_##x
#else
#define DISABLE_ON_WIN_MAC_CHROMEOS(x) x
#endif

INSTANTIATE_TEST_SUITE_P(UsingMLService,
                         PredictionManagerBrowserTest,
                         ::testing::Bool(),
                         ::testing::PrintToStringParamName());

IN_PROC_BROWSER_TEST_P(
    PredictionManagerBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(FCPReachedSessionStatisticsUpdated)) {
  RegisterWithKeyedService();
  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(
      page_load_metrics::PageLoadMetricsTestWaiter::TimingField::kFirstPaint);
  ui_test_utils::NavigateToURL(browser(), https_url_with_content());
  waiter->Wait();

  const OptimizationGuideSessionStatistic* session_fcp =
      GetPredictionManager()->GetFCPSessionStatisticsForTesting();
  EXPECT_TRUE(session_fcp);
  EXPECT_EQ(1u, session_fcp->GetNumberOfSamples());
}

IN_PROC_BROWSER_TEST_P(
    PredictionManagerBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(NoFCPSessionStatisticsUnchanged)) {
  RegisterWithKeyedService();
  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(
      page_load_metrics::PageLoadMetricsTestWaiter::TimingField::kFirstPaint);
  ui_test_utils::NavigateToURL(browser(), https_url_with_content());
  waiter->Wait();

  const OptimizationGuideSessionStatistic* session_fcp =
      GetPredictionManager()->GetFCPSessionStatisticsForTesting();
  float current_mean = session_fcp->GetMean();

  waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(
      page_load_metrics::PageLoadMetricsTestWaiter::TimingField::kLoadEvent);
  ui_test_utils::NavigateToURL(browser(), https_url_without_content());
  waiter->Wait();
  EXPECT_EQ(1u, session_fcp->GetNumberOfSamples());
  EXPECT_EQ(current_mean, session_fcp->GetMean());
}

IN_PROC_BROWSER_TEST_P(
    PredictionManagerBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(ModelsAndFeaturesStoreInitialized)) {
  base::HistogramTester histogram_tester;
  MLServiceProcessObserver ml_service_observer;
  content::NetworkConnectionChangeSimulator().SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_2G);

  RegisterWithKeyedService();
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionManager.PredictionModelsStored", 1);
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 1);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.HostModelFeaturesStored", true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.PredictionModelsStored", true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 2, 1);
  EXPECT_EQ(ml_service_observer.IsLaunched(), using_ml_service());
}

IN_PROC_BROWSER_TEST_P(
    PredictionManagerBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(OnlyHostModelFeaturesInGetModelsResponse)) {
  base::HistogramTester histogram_tester;
  MLServiceProcessObserver ml_service_observer;

  SetResponseType(PredictionModelsFetcherRemoteResponseType::
                      kSuccessfulWithFeaturesAndNoModels);
  RegisterWithKeyedService();
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionManager.HostModelFeaturesStored", 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.HostModelFeaturesStored", true, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionManager.PredictionModelsStored", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 0);
  EXPECT_FALSE(ml_service_observer.IsLaunched());
}

IN_PROC_BROWSER_TEST_P(
    PredictionManagerBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(OnlyPredictionModelsInGetModelsResponse)) {
  base::HistogramTester histogram_tester;
  MLServiceProcessObserver ml_service_observer;

  SetResponseType(PredictionModelsFetcherRemoteResponseType::
                      kSuccessfulWithModelsAndNoFeatures);
  RegisterWithKeyedService();
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionManager.PredictionModelsStored", 1);
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 1);

  // A metadata entry will always be stored for host model features, regardless
  // of whether any host model features were actually returned.
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.HostModelFeaturesStored", true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.PredictionModelsStored", true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 2, 1);
  EXPECT_EQ(ml_service_observer.IsLaunched(), using_ml_service());
}

IN_PROC_BROWSER_TEST_P(
    PredictionManagerBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(PredictionModelFetchFailed)) {
  SetResponseType(PredictionModelsFetcherRemoteResponseType::kUnsuccessful);
  base::HistogramTester histogram_tester;
  MLServiceProcessObserver ml_service_observer;

  RegisterWithKeyedService();

  // Wait until histograms have been updated before performing checks for
  // correct behavior based on the response.
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionModelFetcher.GetModelsResponse.Status", 1);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PredictionModelFetcher.GetModelsResponse.Status",
      net::HTTP_NOT_FOUND, 1);

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionManager.HostModelFeaturesStored", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionManager.PredictionModelsStored", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 0);
  EXPECT_FALSE(ml_service_observer.IsLaunched());
}

IN_PROC_BROWSER_TEST_P(
    PredictionManagerBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(HostModelFeaturesClearedOnHistoryClear)) {
  base::HistogramTester histogram_tester;
  MLServiceProcessObserver ml_service_observer;

  RegisterWithKeyedService();

  // Wait until histograms have been updated before performing checks for
  // correct behavior based on the response.
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionModelFetcher.GetModelsResponse.Status", 1);

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionManager.HostModelFeaturesStored", 1);

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionManager.PredictionModelsStored", 1);

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 1);

  EXPECT_EQ(ml_service_observer.IsLaunched(), using_ml_service());

  SetCallbackOnConsumer(base::DoNothing());
  ui_test_utils::NavigateToURL(browser(), https_url_with_content());

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.HasHostModelFeaturesForHost", true,
      1);

  // Wipe the browser history - clears all the host model features.
  browser()->profile()->Wipe();
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.ClearHostModelFeatures.StoreAvailable", true, 1);

  SetCallbackOnConsumer(base::DoNothing());
  ui_test_utils::NavigateToURL(browser(), https_url_with_content());
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PredictionManager.HasHostModelFeaturesForHost", false,
      1);
}

class PredictionManagerBrowserSameOriginTest
    : public PredictionManagerBrowserTest {
 public:
  PredictionManagerBrowserSameOriginTest() = default;
  ~PredictionManagerBrowserSameOriginTest() override = default;

  void SetUp() override {
    client_model_feature_ =
        optimization_guide::proto::CLIENT_MODEL_FEATURE_SAME_ORIGIN_NAVIGATION;
    PredictionManagerBrowserTest::SetUp();
  }
};

INSTANTIATE_TEST_SUITE_P(UsingMLService,
                         PredictionManagerBrowserSameOriginTest,
                         ::testing::Bool(),
                         ::testing::PrintToStringParamName());

// Regression test for https://crbug.com/1037945. Tests that the origin of the
// previous navigation is computed correctly.
IN_PROC_BROWSER_TEST_P(PredictionManagerBrowserSameOriginTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(IsSameOriginNavigation)) {
  base::HistogramTester histogram_tester;
  MLServiceProcessObserver ml_service_observer;

  RegisterWithKeyedService();

  // Wait until histograms have been updated before performing checks for
  // correct behavior based on the response.
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionModelFetcher.GetModelsResponse.Status", 1);

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionManager.HostModelFeaturesStored", 1);

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionManager.PredictionModelsStored", 1);

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 1);

  EXPECT_EQ(ml_service_observer.IsLaunched(), using_ml_service());

  SetCallbackOnConsumer(base::DoNothing());
  ui_test_utils::NavigateToURL(browser(), https_url_with_content());
  RetryForHistogramUntilCountReached(
      &histogram_tester, "OptimizationGuide.PredictionManager.IsSameOrigin", 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.IsSameOrigin", false, 1);

  // Navigate to the same URL in the same tab. This should count as a
  // same-origin navigation.
  SetCallbackOnConsumer(base::DoNothing());
  ui_test_utils::NavigateToURL(browser(), https_url_with_content());
  RetryForHistogramUntilCountReached(
      &histogram_tester, "OptimizationGuide.PredictionManager.IsSameOrigin", 2);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PredictionManager.IsSameOrigin", false, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PredictionManager.IsSameOrigin", true, 1);

  // Navigate to a cross-origin URL. This should count as a cross-origin
  // navigation.
  SetCallbackOnConsumer(base::DoNothing());
  ui_test_utils::NavigateToURL(browser(), GURL("https://www.google.com/"));
  RetryForHistogramUntilCountReached(
      &histogram_tester, "OptimizationGuide.PredictionManager.IsSameOrigin", 3);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PredictionManager.IsSameOrigin", false, 2);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PredictionManager.IsSameOrigin", true, 1);
}

IN_PROC_BROWSER_TEST_P(
    PredictionManagerBrowserSameOriginTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(ShouldTargetNavigationAsync)) {
  base::HistogramTester histogram_tester;
  MLServiceProcessObserver ml_service_observer;

  RegisterWithKeyedService();

  // Wait until histograms have been updated before performing checks for
  // correct behavior based on the response.
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionModelFetcher.GetModelsResponse.Status", 1);

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionManager.HostModelFeaturesStored", 1);

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionManager.PredictionModelsStored", 1);

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 1);

  EXPECT_EQ(ml_service_observer.IsLaunched(), using_ml_service());

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  SetCallbackOnConsumer(base::BindOnce(
      [](base::RunLoop* run_loop,
         OptimizationGuideConsumerWebContentsObserver* consumer,
         optimization_guide::OptimizationGuideDecision decision) {
        // The model should be evaluated with an actual decision since the model
        // and all features provided are valid.
        EXPECT_NE(decision,
                  optimization_guide::OptimizationGuideDecision::kUnknown);
        run_loop->Quit();
      },
      run_loop.get(), consumer()));

  ui_test_utils::NavigateToURL(browser(), https_url_with_content());
  run_loop->Run();
}

class PredictionManagerUsingMLServiceBrowserSameOriginTest
    : public PredictionManagerBrowserSameOriginTest {};

// Only instantiate with ML Service enabled.
INSTANTIATE_TEST_SUITE_P(UsingMLService,
                         PredictionManagerUsingMLServiceBrowserSameOriginTest,
                         ::testing::Values(true),
                         ::testing::PrintToStringParamName());

IN_PROC_BROWSER_TEST_P(
    PredictionManagerUsingMLServiceBrowserSameOriginTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(
        ShouldTargetNavigationAsyncWithServiceDisconnection)) {
  base::HistogramTester histogram_tester;
  MLServiceProcessObserver ml_service_observer;

  RegisterWithKeyedService();

  // Wait until histograms have been updated before performing checks for
  // correct behavior based on the response.
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionModelFetcher.GetModelsResponse.Status", 1);

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionManager.HostModelFeaturesStored", 1);

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionManager.PredictionModelsStored", 1);

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 1);

  EXPECT_TRUE(ml_service_observer.IsLaunched());

  // Force termination of the service: model predictors will become invalid.
  machine_learning::ServiceConnection::GetInstance()->ResetServiceForTesting();

  SetCallbackOnConsumer(base::BindOnce(
      [](OptimizationGuideConsumerWebContentsObserver* consumer,
         optimization_guide::OptimizationGuideDecision decision) {
        EXPECT_EQ(decision,
                  optimization_guide::OptimizationGuideDecision::kUnknown);
      },
      consumer()));

  ui_test_utils::NavigateToURL(browser(), https_url_with_content());
}

class PredictionManagerUsingMLServiceMetricsOnlyBrowserTest
    : public PredictionManagerBrowserTestBase {
 public:
  PredictionManagerUsingMLServiceMetricsOnlyBrowserTest() = default;
  ~PredictionManagerUsingMLServiceMetricsOnlyBrowserTest() override = default;

 private:
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {optimization_guide::features::kOptimizationHints, {}},
            {optimization_guide::features::kRemoteOptimizationGuideFetching,
             {}},
            {optimization_guide::features::kOptimizationTargetPrediction,
             {{"painful_page_load_metrics_only", "true"}}},
            {optimization_guide::features::
                 kOptimizationTargetPredictionUsingMLService,
             {}},
            {optimization_guide::features::kOptimizationHintsFieldTrials,
             {{"allowed_field_trial_names",
               "scoped_feature_list_trial_for_OptimizationHints,scoped_feature_"
               "list_trial_for_OptimizationHintsFetching"}}},
        },
        {});
    SetExpectedFieldTrialNames(base::flat_set<uint32_t>(
        {variations::HashName(
             "scoped_feature_list_trial_for_OptimizationHints"),
         variations::HashName(
             "scoped_feature_list_trial_for_OptimizationHintsFetching")}));
  }
};

IN_PROC_BROWSER_TEST_F(
    PredictionManagerUsingMLServiceMetricsOnlyBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(ShouldTargetNavigationAsync)) {
  base::HistogramTester histogram_tester;
  MLServiceProcessObserver ml_service_observer;

  EXPECT_TRUE(
      features::ShouldOverrideOptimizationTargetDecisionForMetricsPurposes(
          proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

  RegisterWithKeyedService();

  // Wait until histograms have been updated before performing checks for
  // correct behavior based on the response.
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionModelFetcher.GetModelsResponse.Status", 1);

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionManager.HostModelFeaturesStored", 1);

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionManager.PredictionModelsStored", 1);

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 1);

  EXPECT_TRUE(ml_service_observer.IsLaunched());

  SetCallbackOnConsumer(base::BindOnce(
      [](OptimizationGuideConsumerWebContentsObserver* consumer,
         optimization_guide::OptimizationGuideDecision decision) {
        EXPECT_EQ(decision,
                  optimization_guide::OptimizationGuideDecision::kFalse);
      },
      consumer()));

  ui_test_utils::NavigateToURL(browser(), https_url_with_content());
}

}  // namespace optimization_guide
