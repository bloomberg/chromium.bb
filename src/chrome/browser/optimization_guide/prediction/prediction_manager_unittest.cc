// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/prediction/prediction_manager.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/optimization_guide/optimization_guide_navigation_data.h"
#include "chrome/browser/optimization_guide/optimization_guide_util.h"
#include "chrome/browser/optimization_guide/optimization_guide_web_contents_observer.h"
#include "chrome/browser/optimization_guide/prediction/prediction_model.h"
#include "chrome/browser/optimization_guide/prediction/prediction_model_fetcher.h"
#include "chrome/test/base/testing_profile.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/optimization_guide_prefs.h"
#include "components/optimization_guide/optimization_guide_service.h"
#include "components/optimization_guide/optimization_guide_store.h"
#include "components/optimization_guide/optimization_guide_switches.h"
#include "components/optimization_guide/proto/hint_cache.pb.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/optimization_guide/proto_database_provider_test_base.h"
#include "components/optimization_guide/top_host_provider.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/web_contents_tester.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

using leveldb_proto::test::FakeDB;

namespace {
// Retry delay is 16 minutes to allow for kFetchRetryDelaySecs +
// kFetchRandomMaxDelaySecs to pass.
constexpr int kTestFetchRetryDelaySecs = 60 * 16;
constexpr int kUpdateFetchModelAndFeaturesTimeSecs = 24 * 60 * 60;  // 24 hours.

}  // namespace

namespace optimization_guide {

std::unique_ptr<proto::PredictionModel> CreatePredictionModel() {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      std::make_unique<optimization_guide::proto::PredictionModel>();

  optimization_guide::proto::ModelInfo* model_info =
      prediction_model->mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_features(
      proto::CLIENT_MODEL_FEATURE_EFFECTIVE_CONNECTION_TYPE);
  prediction_model->mutable_model_info()->add_supported_host_model_features(
      "host_feat1");
  model_info->set_optimization_target(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  model_info->add_supported_model_types(
      proto::ModelType::MODEL_TYPE_DECISION_TREE);
  prediction_model->mutable_model()->mutable_threshold()->set_value(5.0);
  return prediction_model;
}

std::unique_ptr<proto::GetModelsResponse> BuildGetModelsResponse(
    const std::vector<std::string>& hosts,
    const std::vector<proto::ClientModelFeature>& client_model_features) {
  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      std::make_unique<proto::GetModelsResponse>();

  for (const auto& host : hosts) {
    proto::HostModelFeatures* host_model_features =
        get_models_response->add_host_model_features();
    host_model_features->set_host(host);
    proto::ModelFeature* model_feature =
        host_model_features->add_model_features();
    model_feature->set_feature_name("host_feat1");
    model_feature->set_double_value(2.0);
  }

  std::unique_ptr<proto::PredictionModel> prediction_model =
      CreatePredictionModel();
  for (const auto& client_model_feature : client_model_features) {
    prediction_model->mutable_model_info()->add_supported_model_features(
        client_model_feature);
  }
  prediction_model->mutable_model_info()->add_supported_host_model_features(
      "host_feat1");
  prediction_model->mutable_model_info()->set_version(2);
  *get_models_response->add_models() = *prediction_model.get();

  return get_models_response;
}

class TestPredictionModel : public PredictionModel {
 public:
  explicit TestPredictionModel(
      std::unique_ptr<proto::PredictionModel> prediction_model)
      : PredictionModel(std::move(prediction_model)) {}
  ~TestPredictionModel() override = default;

  optimization_guide::OptimizationTargetDecision Predict(
      const base::flat_map<std::string, float>& model_features,
      double* prediction_score) override {
    *prediction_score = 0.0;
    // Check to make sure the all model_features were provided.
    for (const auto& model_feature : GetModelFeatures()) {
      if (!model_features.contains(model_feature))
        return OptimizationTargetDecision::kUnknown;
    }
    *prediction_score = 0.6;
    model_evaluated_ = true;
    last_evaluated_features_ =
        base::flat_map<std::string, float>(model_features);
    return OptimizationTargetDecision::kPageLoadMatches;
  }

  bool WasModelEvaluated() { return model_evaluated_; }

  void ResetModelEvaluationState() { model_evaluated_ = false; }

  base::flat_map<std::string, float> last_evaluated_features() {
    return last_evaluated_features_;
  }

 private:
  bool ValidatePredictionModel() const override { return true; }

  bool model_evaluated_ = false;
  base::flat_map<std::string, float> last_evaluated_features_;
};

// A mock class implementation of TopHostProvider.
class FakeTopHostProvider : public TopHostProvider {
 public:
  explicit FakeTopHostProvider(const std::vector<std::string>& top_hosts)
      : top_hosts_(top_hosts) {}

  std::vector<std::string> GetTopHosts() override {
    num_top_hosts_called_++;
    return top_hosts_;
  }

  int num_top_hosts_called() const { return num_top_hosts_called_; }

 private:
  std::vector<std::string> top_hosts_;
  int num_top_hosts_called_ = 0;
};

enum class PredictionModelFetcherEndState {
  kFetchFailed = 0,
  kFetchSuccessWithModelsAndHostsModelFeatures = 1,
  kFetchSuccessWithEmptyResponse = 2,
};

// A mock class implementation of PredictionModelFetcher.
class TestPredictionModelFetcher : public PredictionModelFetcher {
 public:
  TestPredictionModelFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      GURL optimization_guide_service_get_models_url,
      PredictionModelFetcherEndState fetch_state)
      : PredictionModelFetcher(url_loader_factory,
                               optimization_guide_service_get_models_url),
        fetch_state_(fetch_state) {}

  bool FetchOptimizationGuideServiceModels(
      const std::vector<optimization_guide::proto::ModelInfo>&
          models_request_info,
      const std::vector<std::string>& hosts,
      optimization_guide::proto::RequestContext request_context,
      ModelsFetchedCallback models_fetched_callback) override {
    if (!ValidateModelsInfoForFetch(models_request_info)) {
      std::move(models_fetched_callback).Run(base::nullopt);
      return false;
    }

    count_hosts_fetched_ = hosts.size();
    switch (fetch_state_) {
      case PredictionModelFetcherEndState::kFetchFailed:
        std::move(models_fetched_callback).Run(base::nullopt);
        return false;
      case PredictionModelFetcherEndState::
          kFetchSuccessWithModelsAndHostsModelFeatures:
        models_fetched_ = true;
        std::move(models_fetched_callback)
            .Run(BuildGetModelsResponse(hosts, {}));
        return true;
      case PredictionModelFetcherEndState::kFetchSuccessWithEmptyResponse:
        models_fetched_ = true;
        std::move(models_fetched_callback)
            .Run(BuildGetModelsResponse({} /* hosts */,
                                        {} /* client model features */));
        return true;
    }
    return true;
  }

  bool ValidateModelsInfoForFetch(
      const std::vector<proto::ModelInfo>& models_request_info) {
    for (const auto& model_info : models_request_info) {
      if (model_info.supported_model_types_size() == 0 ||
          !proto::ModelType_IsValid(model_info.supported_model_types(0))) {
        return false;
      }
      if (!model_info.has_optimization_target() ||
          !proto::OptimizationTarget_IsValid(
              model_info.optimization_target())) {
        return false;
      }
    }
    return true;
  }

  bool models_fetched() { return models_fetched_; }
  size_t hosts_fetched() { return count_hosts_fetched_; }

 private:
  bool models_fetched_ = false;
  size_t count_hosts_fetched_ = 0;
  // The desired behavior of the TestPredictionModelFetcher.
  PredictionModelFetcherEndState fetch_state_;
};

class TestOptimizationGuideStore : public OptimizationGuideStore {
 public:
  explicit TestOptimizationGuideStore(
      std::unique_ptr<StoreEntryProtoDatabase> database)
      : OptimizationGuideStore(std::move(database)) {}

  ~TestOptimizationGuideStore() override = default;

  void Initialize(bool purge_existing_data,
                  base::OnceClosure callback) override {
    init_callback_ = std::move(callback);
    status_ = Status::kAvailable;
  }

  void RunInitCallback(bool load_models = true,
                       bool load_host_model_features = true,
                       bool have_models_in_store = true) {
    load_models_ = load_models;
    load_host_model_features_ = load_host_model_features;
    have_models_in_store_ = have_models_in_store;
    std::move(init_callback_).Run();
  }

  void RunUpdateHostModelFeaturesCallback() {
    std::move(update_host_models_callback_).Run();
  }

  void LoadPredictionModel(const EntryKey& prediction_model_entry_key,
                           PredictionModelLoadedCallback callback) override {
    model_loaded_ = true;
    if (load_models_) {
      std::move(callback).Run(CreatePredictionModel());
    } else {
      std::move(callback).Run(nullptr);
    }
  }

  void LoadAllHostModelFeatures(
      AllHostModelFeaturesLoadedCallback callback) override {
    host_model_features_loaded_ = true;
    if (load_host_model_features_) {
      proto::HostModelFeatures host_model_features;
      host_model_features.set_host("foo.com");
      proto::ModelFeature* model_feature =
          host_model_features.add_model_features();
      model_feature->set_feature_name("host_feat1");
      model_feature->set_double_value(2.0);
      std::unique_ptr<std::vector<proto::HostModelFeatures>>
          all_host_model_features =
              std::make_unique<std::vector<proto::HostModelFeatures>>();
      all_host_model_features->emplace_back(host_model_features);
      std::move(callback).Run(std::move(all_host_model_features));
    } else {
      std::move(callback).Run(nullptr);
    }
  }

  bool FindPredictionModelEntryKey(
      proto::OptimizationTarget optimization_target,
      OptimizationGuideStore::EntryKey* out_prediction_model_entry_key)
      override {
    if (have_models_in_store_) {
      *out_prediction_model_entry_key =
          "4_" + base::NumberToString(static_cast<int>(optimization_target));
      return true;
    }
    return false;
  }

  void UpdateHostModelFeatures(
      std::unique_ptr<StoreUpdateData> host_model_features_update_data,
      base::OnceClosure callback) override {
    host_model_features_update_time_ =
        *host_model_features_update_data->update_time();
    update_host_models_callback_ = std::move(callback);
  }

  void UpdatePredictionModels(
      std::unique_ptr<StoreUpdateData> prediction_models_update_data,
      base::OnceClosure callback) override {
    std::move(callback).Run();
  }

  bool WasModelLoaded() const { return model_loaded_; }
  bool WasHostModelFeaturesLoaded() const {
    return host_model_features_loaded_;
  }
 private:
  base::OnceClosure init_callback_;
  base::OnceClosure update_host_models_callback_;
  bool model_loaded_ = false;
  bool host_model_features_loaded_ = false;
  bool load_models_ = true;
  bool load_host_model_features_ = true;
  bool have_models_in_store_ = true;
};

class TestPredictionManager : public PredictionManager {
 public:
  using StoreEntry = proto::StoreEntry;
  using StoreEntryMap = std::map<OptimizationGuideStore::EntryKey, StoreEntry>;
  TestPredictionManager(
      const std::vector<optimization_guide::proto::OptimizationTarget>&
          optimization_targets_at_initialization,
      const base::FilePath& profile_path,
      leveldb_proto::ProtoDatabaseProvider* database_provider,
      TopHostProvider* top_host_provider,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService* pref_service,
      Profile* profile)
      : PredictionManager(optimization_targets_at_initialization,
                          CreateModelAndHostModelFeaturesStore(),
                          top_host_provider,
                          url_loader_factory,
                          pref_service,
                          profile) {}

  ~TestPredictionManager() override = default;

  std::unique_ptr<PredictionModel> CreatePredictionModel(
      const proto::PredictionModel& model) const override {
    if (!create_valid_prediction_model_)
      return nullptr;
    std::unique_ptr<PredictionModel> prediction_model =
        std::make_unique<TestPredictionModel>(
            std::make_unique<proto::PredictionModel>(model));
    return prediction_model;
  }

  void set_create_valid_prediction_model(bool create_valid_prediction_model) {
    create_valid_prediction_model_ = create_valid_prediction_model;
  }

  using PredictionManager::GetHostModelFeaturesForHost;
  using PredictionManager::GetHostModelFeaturesForTesting;
  using PredictionManager::GetPredictionModelForTesting;

  std::unique_ptr<OptimizationGuideStore>
  CreateModelAndHostModelFeaturesStore() {
    // Setup the fake db and the class under test.
    auto db = std::make_unique<FakeDB<StoreEntry>>(&db_store_);

    std::unique_ptr<OptimizationGuideStore> model_and_features_store =
        std::make_unique<TestOptimizationGuideStore>(std::move(db));
    return model_and_features_store;
  }

  void UpdateHostModelFeaturesForTesting(
      proto::GetModelsResponse* get_models_response) {
    UpdateHostModelFeatures(get_models_response->host_model_features());
  }

  void UpdatePredictionModelsForTesting(
      proto::GetModelsResponse* get_models_response) {
    UpdatePredictionModels(get_models_response->models());
  }

 private:
  StoreEntryMap db_store_;
  bool create_valid_prediction_model_ = true;
};

class PredictionManagerTest
    : public optimization_guide::ProtoDatabaseProviderTestBase,
      public testing::WithParamInterface<proto::ClientModelFeature> {
 public:
  PredictionManagerTest() = default;
  ~PredictionManagerTest() override = default;

  void SetUp() override {
    optimization_guide::ProtoDatabaseProviderTestBase::SetUp();
    web_contents_factory_.reset(new content::TestWebContentsFactory);

    top_host_provider_ = std::make_unique<FakeTopHostProvider>(
        std::vector<std::string>({"example1.com", "example2.com"}));

    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    optimization_guide::prefs::RegisterProfilePrefs(pref_service_->registry());

    url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableCheckingUserPermissionsForTesting);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kFetchModelsAndHostModelFeaturesOverrideTimer);
  }

  void CreatePredictionManager(
      const std::vector<optimization_guide::proto::OptimizationTarget>&
          optimization_targets_at_initialization) {
    if (prediction_manager_)
      prediction_manager_.reset();

    prediction_manager_ = std::make_unique<TestPredictionManager>(
        optimization_targets_at_initialization, temp_dir(), db_provider_.get(),
        top_host_provider_.get(), url_loader_factory_, pref_service_.get(),
        &testing_profile_);
    prediction_manager_->SetClockForTesting(task_environment_.GetMockClock());
  }

  void CreatePredictionManagerWithoutTopHostProvider(
      const std::vector<optimization_guide::proto::OptimizationTarget>&
          optimization_targets_at_initialization) {
    if (prediction_manager_)
      prediction_manager_.reset();

    prediction_manager_ = std::make_unique<TestPredictionManager>(
        optimization_targets_at_initialization, temp_dir(), db_provider_.get(),
        nullptr, url_loader_factory_, pref_service_.get(), &testing_profile_);
    prediction_manager_->SetClockForTesting(task_environment_.GetMockClock());
  }

  TestPredictionManager* prediction_manager() const {
    return prediction_manager_.get();
  }

  // Creates a navigation handle with the OptimizationGuideWebContentsObserver
  // attached.
  std::unique_ptr<content::MockNavigationHandle>
  CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
      const GURL& url) {
    content::WebContents* web_contents =
        web_contents_factory_->CreateWebContents(&testing_profile_);
    OptimizationGuideWebContentsObserver::CreateForWebContents(web_contents);
    std::unique_ptr<content::MockNavigationHandle> navigation_handle =
        std::make_unique<content::MockNavigationHandle>(web_contents);
    navigation_handle->set_url(url);
    return navigation_handle;
  }

  bool IsSameOriginNavigationFeature() {
    return GetParam() == proto::CLIENT_MODEL_FEATURE_SAME_ORIGIN_NAVIGATION;
  }

  bool IsUnknownFeature() {
    return GetParam() == proto::CLIENT_MODEL_FEATURE_UNKNOWN;
  }

  void TearDown() override {
    optimization_guide::ProtoDatabaseProviderTestBase::TearDown();
  }

  FakeTopHostProvider* top_host_provider() const {
    return top_host_provider_.get();
  }

  std::unique_ptr<TestPredictionModelFetcher> BuildTestPredictionModelFetcher(
      PredictionModelFetcherEndState end_state) {
    std::unique_ptr<TestPredictionModelFetcher> prediction_model_fetcher =
        std::make_unique<TestPredictionModelFetcher>(
            url_loader_factory_, GURL("https://hintsserver.com"), end_state);
    return prediction_model_fetcher;
  }

  void SetStoreInitialized(bool load_models = true,
                           bool load_host_model_features = true,
                           bool have_models_in_store = true) {
    models_and_features_store()->RunInitCallback(
        load_models, load_host_model_features, have_models_in_store);
    RunUntilIdle();
    // Move clock forward for any short delays added for the fetcher.
    MoveClockForwardBy(base::TimeDelta::FromSeconds(2));
  }

  void MoveClockForwardBy(base::TimeDelta time_delta) {
    task_environment_.FastForwardBy(time_delta);
    RunUntilIdle();
  }

  TestPredictionModelFetcher* prediction_model_fetcher() const {
    return static_cast<TestPredictionModelFetcher*>(
        prediction_manager()->prediction_model_fetcher());
  }

  TestOptimizationGuideStore* models_and_features_store() const {
    return static_cast<TestOptimizationGuideStore*>(
        prediction_manager()->model_and_features_store());
  }

  base::FilePath temp_dir() const { return temp_dir_.GetPath(); }

  TestingPrefServiceSimple* pref_service() const { return pref_service_.get(); }

  void RunUntilIdle() {
    task_environment_.RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestPredictionManager> prediction_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<FakeTopHostProvider> top_host_provider_;
  TestingProfile testing_profile_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<content::TestWebContentsFactory> web_contents_factory_;

  DISALLOW_COPY_AND_ASSIGN(PredictionManagerTest);
};

// No support for Mac, Windows or ChromeOS.
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
#define DISABLE_ON_WIN_MAC_CHROMEOS(x) DISABLED_##x
#else
#define DISABLE_ON_WIN_MAC_CHROMEOS(x) x
#endif

TEST_F(PredictionManagerTest,
       OptimizationTargetProvidedAtInitializationIsRegistered) {
  CreatePredictionManager(
      {optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});

  EXPECT_FALSE(prediction_manager()->registered_optimization_targets().empty());
}

TEST_F(PredictionManagerTest, OptimizationTargetNotRegisteredForNavigation) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://foo.com"));

  CreatePredictionManager({});

  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::
              kFetchSuccessWithModelsAndHostsModelFeatures));

  prediction_manager()->RegisterOptimizationTargets(
      {optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});
  SetStoreInitialized();

  EXPECT_TRUE(prediction_model_fetcher()->models_fetched());

  EXPECT_EQ(OptimizationTargetDecision::kUnknown,
            prediction_manager()->ShouldTargetNavigation(
                navigation_handle.get(), proto::OPTIMIZATION_TARGET_UNKNOWN));
  // OptimizationGuideNavData should not be populated.
  OptimizationGuideNavigationData* nav_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  EXPECT_FALSE(nav_data
                   ->GetModelVersionForOptimizationTarget(
                       optimization_guide::proto::OPTIMIZATION_TARGET_UNKNOWN)
                   .has_value());
  EXPECT_FALSE(nav_data
                   ->GetModelPredictionScoreForOptimizationTarget(
                       optimization_guide::proto::OPTIMIZATION_TARGET_UNKNOWN)
                   .has_value());
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelEvaluationLatency." +
          GetStringNameForOptimizationTarget(
              optimization_guide::proto::OPTIMIZATION_TARGET_UNKNOWN),
      0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelEvaluationLatency." +
          GetStringNameForOptimizationTarget(
              optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      0);
}

TEST_F(PredictionManagerTest,
       DISABLE_ON_WIN_MAC_CHROMEOS(
           NoPredictionModelForRegisteredOptimizationTarget)) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://foo.com"));

  CreatePredictionManager({proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});
  EXPECT_EQ(OptimizationTargetDecision::kModelNotAvailableOnClient,
            prediction_manager()->ShouldTargetNavigation(
                navigation_handle.get(),
                proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

  // OptimizationGuideNavData should not be populated.
  OptimizationGuideNavigationData* nav_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  EXPECT_FALSE(
      nav_data
          ->GetModelVersionForOptimizationTarget(
              optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD)
          .has_value());
  EXPECT_FALSE(
      nav_data
          ->GetModelPredictionScoreForOptimizationTarget(
              optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD)
          .has_value());

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelEvaluationLatency." +
          GetStringNameForOptimizationTarget(
              optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      0);
}

TEST_F(PredictionManagerTest, EvaluatePredictionModel) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://foo.com"));

  CreatePredictionManager({});
  // The model will be loaded from the store.
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchSuccessWithEmptyResponse));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});
  SetStoreInitialized();
  EXPECT_TRUE(prediction_model_fetcher()->models_fetched());

  EXPECT_EQ(OptimizationTargetDecision::kPageLoadMatches,
            prediction_manager()->ShouldTargetNavigation(
                navigation_handle.get(),
                proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

  TestPredictionModel* test_prediction_model =
      static_cast<TestPredictionModel*>(
          prediction_manager()->GetPredictionModelForTesting(
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  EXPECT_TRUE(test_prediction_model);
  EXPECT_TRUE(test_prediction_model->WasModelEvaluated());

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelEvaluationLatency." +
          GetStringNameForOptimizationTarget(
              optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      1);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.IsPredictionModelValid." +
          GetStringNameForOptimizationTarget(
              optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      true, 1);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.IsPredictionModelValid", true, 1);

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelValidationLatency." +
          GetStringNameForOptimizationTarget(
              optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelValidationLatency", 1);
}

TEST_F(PredictionManagerTest, UpdatePredictionModelsWithInvalidModel) {
  base::HistogramTester histogram_tester;
  CreatePredictionManager({});
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});

  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      BuildGetModelsResponse({} /* hosts */, {} /* client features */);

  // Override the manager so that any prediction model updates will be seen as
  // invalid.
  prediction_manager()->set_create_valid_prediction_model(false);
  prediction_manager()->UpdatePredictionModelsForTesting(
      get_models_response.get());

  histogram_tester.ExpectBucketCount("OptimizationGuide.IsPredictionModelValid",
                                     false, 1);

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelValidationLatency", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelUpdatedVersion.PainfulPageLoad", 0);
}

TEST_F(PredictionManagerTest, UpdateModelWithSameVersion) {
  base::HistogramTester histogram_tester;
  CreatePredictionManager({});
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});

  // Seed the PredictionManager with a prediction model with a higher version
  // to try to be updated.
  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      BuildGetModelsResponse({} /* hosts */, {} /* client features */);
  get_models_response->mutable_models(0)->mutable_model_info()->set_version(3);

  prediction_manager()->UpdatePredictionModelsForTesting(
      get_models_response.get());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.PredictionModelsStored", true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad", 3, 1);

  get_models_response =
      BuildGetModelsResponse({} /* hosts */, {} /* client features */);

  get_models_response->mutable_models(0)->mutable_model_info()->set_version(3);
  prediction_manager()->UpdatePredictionModelsForTesting(
      get_models_response.get());

  TestPredictionModel* stored_prediction_model =
      static_cast<TestPredictionModel*>(
          prediction_manager()->GetPredictionModelForTesting(
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  EXPECT_TRUE(stored_prediction_model);
  EXPECT_EQ(3, stored_prediction_model->GetVersion());

  histogram_tester.ExpectBucketCount("OptimizationGuide.IsPredictionModelValid",
                                     true, 2);
}

TEST_F(PredictionManagerTest,
       EvaluatePredictionModelUsesDecisionFromPostiveEvalIfModelWasEvaluated) {
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://foo.com"));

  CreatePredictionManager({});
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::
              kFetchSuccessWithModelsAndHostsModelFeatures));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});

  SetStoreInitialized();
  EXPECT_TRUE(prediction_model_fetcher()->models_fetched());

  OptimizationGuideNavigationData* nav_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  nav_data->SetDecisionForOptimizationTarget(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      OptimizationTargetDecision::kPageLoadMatches);

  TestPredictionModel* test_prediction_model =
      static_cast<TestPredictionModel*>(
          prediction_manager()->GetPredictionModelForTesting(
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  EXPECT_TRUE(test_prediction_model);

  // Make sure the cached decision is returned and that the model was not
  // evaluated.
  EXPECT_EQ(OptimizationTargetDecision::kPageLoadMatches,
            prediction_manager()->ShouldTargetNavigation(
                navigation_handle.get(),
                proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  EXPECT_FALSE(test_prediction_model->WasModelEvaluated());
}

TEST_F(PredictionManagerTest,
       EvaluatePredictionModelUsesDecisionFromNegativeEvalIfModelWasEvaluated) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://foo.com"));

  CreatePredictionManager({});
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::
              kFetchSuccessWithModelsAndHostsModelFeatures));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});

  SetStoreInitialized();
  EXPECT_TRUE(prediction_model_fetcher()->models_fetched());

  OptimizationGuideNavigationData* nav_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  nav_data->SetDecisionForOptimizationTarget(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      OptimizationTargetDecision::kPageLoadDoesNotMatch);

  TestPredictionModel* test_prediction_model =
      static_cast<TestPredictionModel*>(
          prediction_manager()->GetPredictionModelForTesting(
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  EXPECT_TRUE(test_prediction_model);

  // Make sure the previous decision is reused and that the model was not
  // evaluated.
  EXPECT_EQ(OptimizationTargetDecision::kPageLoadDoesNotMatch,
            prediction_manager()->ShouldTargetNavigation(
                navigation_handle.get(),
                proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  EXPECT_FALSE(test_prediction_model->WasModelEvaluated());
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ShouldTargetNavigation.PredictionModelStatus", 0);
}

TEST_F(PredictionManagerTest,
       EvaluatePredictionModelUsesDecisionFromHoldbackEvalIfModelWasEvaluated) {
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://foo.com"));

  CreatePredictionManager({});
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::
              kFetchSuccessWithModelsAndHostsModelFeatures));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});

  SetStoreInitialized();
  EXPECT_TRUE(prediction_model_fetcher()->models_fetched());

  OptimizationGuideNavigationData* nav_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  nav_data->SetDecisionForOptimizationTarget(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      OptimizationTargetDecision::kModelPredictionHoldback);

  TestPredictionModel* test_prediction_model =
      static_cast<TestPredictionModel*>(
          prediction_manager()->GetPredictionModelForTesting(
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  EXPECT_TRUE(test_prediction_model);

  // Make sure the cached decision is returned and that the model was not
  // evaluated.
  EXPECT_EQ(OptimizationTargetDecision::kModelPredictionHoldback,
            prediction_manager()->ShouldTargetNavigation(
                navigation_handle.get(),
                proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  EXPECT_FALSE(test_prediction_model->WasModelEvaluated());
}

TEST_F(PredictionManagerTest, EvaluatePredictionModelPopulatesNavData) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://foo.com"));

  CreatePredictionManager({});
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::
              kFetchSuccessWithModelsAndHostsModelFeatures));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});

  SetStoreInitialized();
  EXPECT_TRUE(prediction_model_fetcher()->models_fetched());

  models_and_features_store()->RunUpdateHostModelFeaturesCallback();
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.HostModelFeaturesStored", true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.PredictionModelsStored", true, 1);

  OptimizationGuideNavigationData* nav_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  nav_data->SetDecisionForOptimizationTarget(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      OptimizationTargetDecision::kModelNotAvailableOnClient);

  // Make sure model gets evaluated despite there already being a decision in
  // the navigation data.
  EXPECT_EQ(OptimizationTargetDecision::kPageLoadMatches,
            prediction_manager()->ShouldTargetNavigation(
                navigation_handle.get(),
                proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

  TestPredictionModel* test_prediction_model =
      static_cast<TestPredictionModel*>(
          prediction_manager()->GetPredictionModelForTesting(
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  EXPECT_TRUE(test_prediction_model);
  EXPECT_TRUE(test_prediction_model->WasModelEvaluated());

  EXPECT_EQ(2, *nav_data->GetModelVersionForOptimizationTarget(
                   proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  EXPECT_EQ(0.6, *nav_data->GetModelPredictionScoreForOptimizationTarget(
                     proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
}

TEST_F(PredictionManagerTest,
       EvaluatePredictionModelPopulatesNavDataEvenWithHoldback) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {base::test::ScopedFeatureList::FeatureAndParams(
          features::kOptimizationTargetPrediction,
          {{"painful_page_load_metrics_only", "true"}})},
      {});

  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://foo.com"));

  CreatePredictionManager({});
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::
              kFetchSuccessWithModelsAndHostsModelFeatures));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});

  SetStoreInitialized();
  EXPECT_TRUE(prediction_model_fetcher()->models_fetched());
  models_and_features_store()->RunUpdateHostModelFeaturesCallback();

  EXPECT_EQ(OptimizationTargetDecision::kModelPredictionHoldback,
            prediction_manager()->ShouldTargetNavigation(
                navigation_handle.get(),
                proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

  TestPredictionModel* test_prediction_model =
      static_cast<TestPredictionModel*>(
          prediction_manager()->GetPredictionModelForTesting(
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  EXPECT_TRUE(test_prediction_model);
  EXPECT_TRUE(test_prediction_model->WasModelEvaluated());

  OptimizationGuideNavigationData* nav_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  EXPECT_EQ(2, *nav_data->GetModelVersionForOptimizationTarget(
                   proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  EXPECT_EQ(0.6, *nav_data->GetModelPredictionScoreForOptimizationTarget(
                     proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.ShouldTargetNavigation.PredictionModelStatus",
      PredictionManagerModelStatus::kModelAvailable, 1);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.ShouldTargetNavigation.PredictionModelStatus." +
          GetStringNameForOptimizationTarget(
              optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      PredictionManagerModelStatus::kModelAvailable, 1);
}

TEST_F(PredictionManagerTest, ShouldTargetNavigationStoreAvailableNoModel) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://foo.com"));

  CreatePredictionManager({});
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchSuccessWithEmptyResponse));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});

  SetStoreInitialized(/*load_models=*/false,
                      /*load_host_model_features=*/true,
                      /*have_models_in_store=)*/ false);

  EXPECT_EQ(OptimizationTargetDecision::kModelNotAvailableOnClient,
            prediction_manager()->ShouldTargetNavigation(
                navigation_handle.get(),
                proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.ShouldTargetNavigation.PredictionModelStatus",
      PredictionManagerModelStatus::kStoreAvailableNoModelForTarget, 1);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.ShouldTargetNavigation.PredictionModelStatus." +
          GetStringNameForOptimizationTarget(
              optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      PredictionManagerModelStatus::kStoreAvailableNoModelForTarget, 1);
}

TEST_F(PredictionManagerTest,
       ShouldTargetNavigationStoreAvailableModelNotLoaded) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://foo.com"));

  CreatePredictionManager({});
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchSuccessWithEmptyResponse));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});

  SetStoreInitialized(/*load_models=*/false,
                      /*load_host_model_features=*/true,
                      /*have_models_in_store=)*/ true);

  EXPECT_EQ(OptimizationTargetDecision::kModelNotAvailableOnClient,
            prediction_manager()->ShouldTargetNavigation(
                navigation_handle.get(),
                proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.ShouldTargetNavigation.PredictionModelStatus",
      PredictionManagerModelStatus::kStoreAvailableModelNotLoaded, 1);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.ShouldTargetNavigation.PredictionModelStatus." +
          GetStringNameForOptimizationTarget(
              optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      PredictionManagerModelStatus::kStoreAvailableModelNotLoaded, 1);

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 0);
}

TEST_F(PredictionManagerTest,
       DISABLE_ON_WIN_MAC_CHROMEOS(
           ShouldTargetNavigationStoreUnavailableModelUnknown)) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://foo.com"));

  CreatePredictionManager({});
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchSuccessWithEmptyResponse));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});

  EXPECT_EQ(OptimizationTargetDecision::kModelNotAvailableOnClient,
            prediction_manager()->ShouldTargetNavigation(
                navigation_handle.get(),
                proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.ShouldTargetNavigation.PredictionModelStatus",
      PredictionManagerModelStatus::kStoreUnavailableModelUnknown, 1);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.ShouldTargetNavigation.PredictionModelStatus." +
          GetStringNameForOptimizationTarget(
              optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      PredictionManagerModelStatus::kStoreUnavailableModelUnknown, 1);
}

TEST_F(PredictionManagerTest, UpdateModelForUnregisteredTarget) {
  base::HistogramTester histogram_tester;
  CreatePredictionManager({});
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::
              kFetchSuccessWithModelsAndHostsModelFeatures));

  prediction_manager()->RegisterOptimizationTargets({});
  SetStoreInitialized();

  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());

  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      BuildGetModelsResponse({} /* hosts */, {} /* client features */);

  prediction_manager()->UpdatePredictionModelsForTesting(
      get_models_response.get());

  TestPredictionModel* test_prediction_model =
      static_cast<TestPredictionModel*>(
          prediction_manager()->GetPredictionModelForTesting(
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  EXPECT_FALSE(test_prediction_model);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionManager.PredictionModelsStored", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionManager.HostModelFeaturesStored", 0);
}

TEST_F(
    PredictionManagerTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(UpdateModelWithUnsupportedOptimizationTarget)) {
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://foo.com"));

  CreatePredictionManager({});
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});

  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());
  EXPECT_FALSE(models_and_features_store()->WasModelLoaded());

  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      BuildGetModelsResponse({}, {});
  get_models_response->mutable_models(0)
      ->mutable_model_info()
      ->clear_optimization_target();
  prediction_manager()->UpdatePredictionModelsForTesting(
      get_models_response.get());

  EXPECT_EQ(OptimizationTargetDecision::kModelNotAvailableOnClient,
            prediction_manager()->ShouldTargetNavigation(
                navigation_handle.get(),
                proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

  TestPredictionModel* test_prediction_model =
      static_cast<TestPredictionModel*>(
          prediction_manager()->GetPredictionModelForTesting(
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  EXPECT_FALSE(test_prediction_model);
  EXPECT_FALSE(models_and_features_store()->WasModelLoaded());
}

TEST_F(PredictionManagerTest, HasHostModelFeaturesForHost) {
  base::HistogramTester histogram_tester;

  CreatePredictionManager({});
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});
  SetStoreInitialized();

  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      BuildGetModelsResponse({"example1.com", "example2.com"}, {});
  prediction_manager()->UpdateHostModelFeaturesForTesting(
      get_models_response.get());

  base::Optional<base::flat_map<std::string, float>> host_model_features_map =
      prediction_manager()->GetHostModelFeaturesForHost("example1.com");
  EXPECT_TRUE(host_model_features_map);
  EXPECT_TRUE(host_model_features_map->contains("host_feat1"));
  EXPECT_EQ(2.0, (*host_model_features_map)["host_feat1"]);

  host_model_features_map =
      prediction_manager()->GetHostModelFeaturesForHost("example2.com");
  EXPECT_TRUE(host_model_features_map);
  EXPECT_TRUE(host_model_features_map->contains("host_feat1"));
  EXPECT_EQ(2.0, (*host_model_features_map)["host_feat1"]);
}

TEST_F(PredictionManagerTest, NoHostModelFeaturesForHost) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://bar.com"));

  CreatePredictionManager({});
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});

  SetStoreInitialized();

  EXPECT_EQ(OptimizationTargetDecision::kPageLoadMatches,
            prediction_manager()->ShouldTargetNavigation(
                navigation_handle.get(),
                proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

  TestPredictionModel* test_prediction_model =
      static_cast<TestPredictionModel*>(
          prediction_manager()->GetPredictionModelForTesting(
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

  EXPECT_TRUE(test_prediction_model);
  EXPECT_TRUE(test_prediction_model->WasModelEvaluated());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager."
      "HasHostModelFeaturesForHost",
      false, 1);
  EXPECT_LT(test_prediction_model->last_evaluated_features()["host_feat1"], 0);

  EXPECT_FALSE(prediction_manager()->GetHostModelFeaturesForHost("bar.com"));
  // One item loaded from the store when initialized.
  EXPECT_EQ(1u, prediction_manager()->GetHostModelFeaturesForTesting()->size());
}

TEST_F(PredictionManagerTest, UpdateHostModelFeaturesMissingHost) {

  CreatePredictionManager({});
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});

  SetStoreInitialized();

  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      BuildGetModelsResponse({"example1.com"}, {});
  get_models_response->mutable_host_model_features(0)->clear_host();

  prediction_manager()->UpdateHostModelFeaturesForTesting(
      get_models_response.get());

  EXPECT_FALSE(
      prediction_manager()->GetHostModelFeaturesForHost("example1.com"));
  // One item loaded from the store when initialized.
  EXPECT_EQ(1u, prediction_manager()->GetHostModelFeaturesForTesting()->size());
}

TEST_F(PredictionManagerTest, UpdateHostModelFeaturesNoFeature) {

  CreatePredictionManager({});
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});
  SetStoreInitialized();

  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      BuildGetModelsResponse({"example1.com"}, {});
  get_models_response->mutable_host_model_features(0)->clear_model_features();

  prediction_manager()->UpdateHostModelFeaturesForTesting(
      get_models_response.get());

  EXPECT_FALSE(
      prediction_manager()->GetHostModelFeaturesForHost("example1.com"));
  // One item loaded from the store when initialized.
  EXPECT_EQ(1u, prediction_manager()->GetHostModelFeaturesForTesting()->size());
}

TEST_F(PredictionManagerTest, UpdateHostModelFeaturesNoFeatureName) {

  CreatePredictionManager({});
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});

  SetStoreInitialized();

  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      BuildGetModelsResponse({"example1.com"}, {});
  get_models_response->mutable_host_model_features(0)
      ->mutable_model_features(0)
      ->clear_feature_name();

  prediction_manager()->UpdateHostModelFeaturesForTesting(
      get_models_response.get());

  EXPECT_FALSE(
      prediction_manager()->GetHostModelFeaturesForHost("example1.com"));
  // One item loaded from the store when initialized.
  EXPECT_EQ(1u, prediction_manager()->GetHostModelFeaturesForTesting()->size());
}

TEST_F(PredictionManagerTest, UpdateHostModelFeaturesDoubleValue) {
  CreatePredictionManager({});
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});

  SetStoreInitialized();
  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      BuildGetModelsResponse({"example1.com"}, {});
  get_models_response->mutable_host_model_features(0)
      ->mutable_model_features(0)
      ->set_double_value(3.0);

  prediction_manager()->UpdateHostModelFeaturesForTesting(
      get_models_response.get());

  auto host_model_features =
      prediction_manager()->GetHostModelFeaturesForHost("example1.com");
  EXPECT_TRUE(host_model_features);
  EXPECT_EQ(3.0, (*host_model_features)["host_feat1"]);
}

TEST_F(PredictionManagerTest, UpdateHostModelFeaturesIntValue) {

  CreatePredictionManager({});
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});

  SetStoreInitialized();
  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      BuildGetModelsResponse({"example1.com"}, {});
  get_models_response->mutable_host_model_features(0)
      ->mutable_model_features(0)
      ->set_int64_value(4);

  prediction_manager()->UpdateHostModelFeaturesForTesting(
      get_models_response.get());

  auto host_model_features =
      prediction_manager()->GetHostModelFeaturesForHost("example1.com");
  EXPECT_TRUE(host_model_features);
  // We expect the value to be stored as a float but is created from an int64
  // value.
  EXPECT_EQ(4.0, (*host_model_features)["host_feat1"]);
}

TEST_F(PredictionManagerTest, RestrictHostModelFeaturesCacheSize) {
  CreatePredictionManager({});
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});

  SetStoreInitialized();
  std::vector<std::string> hosts;
  for (size_t i = 0; i <= features::MaxHostModelFeaturesCacheSize() + 1; i++)
    hosts.push_back("host" + base::NumberToString(i) + ".com");
  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      BuildGetModelsResponse(hosts, {});

  prediction_manager()->UpdateHostModelFeaturesForTesting(
      get_models_response.get());

  auto* host_model_features_cache =
      prediction_manager()->GetHostModelFeaturesForTesting();
  EXPECT_EQ(features::MaxHostModelFeaturesCacheSize(),
            host_model_features_cache->size());
}

TEST_F(PredictionManagerTest, FetchWithoutTopHostProvider) {
  base::HistogramTester histogram_tester;

  CreatePredictionManagerWithoutTopHostProvider({});
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::
              kFetchSuccessWithModelsAndHostsModelFeatures));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});

  SetStoreInitialized();

  EXPECT_TRUE(prediction_model_fetcher()->models_fetched());

  // No hosts should be included in the fetch as the top host provider is not
  // available.
  EXPECT_EQ(prediction_model_fetcher()->hosts_fetched(), 0ul);
}

TEST_F(PredictionManagerTest, UpdateHostModelFeaturesUpdateDataInMap) {
  base::HistogramTester histogram_tester;

  CreatePredictionManager({});
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});

  SetStoreInitialized();
  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      BuildGetModelsResponse({"example1.com"}, {});
  get_models_response->mutable_host_model_features(0)
      ->mutable_model_features(0)
      ->set_int64_value(4);

  prediction_manager()->UpdateHostModelFeaturesForTesting(
      get_models_response.get());

  auto host_model_features =
      prediction_manager()->GetHostModelFeaturesForHost("example1.com");
  EXPECT_TRUE(host_model_features);
  // We expect the value to be stored as a float but is created from an int64
  // value.
  EXPECT_EQ(4.0, (*host_model_features)["host_feat1"]);

  get_models_response = BuildGetModelsResponse({"example1.com"}, {});
  get_models_response->mutable_host_model_features(0)
      ->mutable_model_features(0)
      ->set_int64_value(5);
  proto::ModelFeature* model_feature =
      get_models_response->mutable_host_model_features(0)->add_model_features();
  model_feature->set_feature_name("host_feat_added");
  model_feature->set_double_value(6.0);

  prediction_manager()->UpdateHostModelFeaturesForTesting(
      get_models_response.get());

  host_model_features =
      prediction_manager()->GetHostModelFeaturesForHost("example1.com");
  EXPECT_TRUE(host_model_features);

  // We expect the value to be stored as a float but is created from an int64
  // value.
  EXPECT_EQ(5.0, (*host_model_features)["host_feat1"]);
  EXPECT_TRUE((*host_model_features).contains("host_feat_added"));
  EXPECT_EQ(6.0, (*host_model_features)["host_feat_added"]);
}

TEST_P(PredictionManagerTest, ClientFeature) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://foo.com"));
  GURL previous_url = GURL("https://foo.com");
  navigation_handle->set_url(previous_url);
  navigation_handle->set_page_transition(
      ui::PageTransition::PAGE_TRANSITION_RELOAD);

  CreatePredictionManager({});
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});

  SetStoreInitialized();

  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      BuildGetModelsResponse({}, {GetParam()});
  prediction_manager()->UpdateHostModelFeaturesForTesting(
      get_models_response.get());
  prediction_manager()->UpdatePredictionModelsForTesting(
      get_models_response.get());

  EXPECT_EQ(OptimizationTargetDecision::kPageLoadMatches,
            prediction_manager()->ShouldTargetNavigation(
                navigation_handle.get(),
                proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

  TestPredictionModel* test_prediction_model =
      static_cast<TestPredictionModel*>(
          prediction_manager()->GetPredictionModelForTesting(
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

  EXPECT_TRUE(test_prediction_model);
  EXPECT_TRUE(test_prediction_model->WasModelEvaluated());
  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  EXPECT_TRUE(navigation_data);
  if (IsUnknownFeature()) {
    EXPECT_FALSE(
        navigation_data->GetValueForModelFeatureForTesting(GetParam()));
  } else {
    EXPECT_TRUE(navigation_data->GetValueForModelFeatureForTesting(GetParam()));
  }
}

INSTANTIATE_TEST_SUITE_P(ClientFeature,
                         PredictionManagerTest,
                         testing::Range(proto::ClientModelFeature_MIN,
                                        proto::ClientModelFeature_MAX));

TEST_F(PredictionManagerTest, PreviousSessionStatisticsUsed) {
  base::HistogramTester histogram_tester;
  GURL previous_url = GURL("https://foo.com");
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          previous_url);
  navigation_handle->set_url(previous_url);
  navigation_handle->set_page_transition(
      ui::PageTransition::PAGE_TRANSITION_RELOAD);

  pref_service()->SetDouble(optimization_guide::prefs::kSessionStatisticFCPMean,
                            200.0);
  pref_service()->SetDouble(
      optimization_guide::prefs::kSessionStatisticFCPStdDev, 50.0);

  CreatePredictionManager({});
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});

  SetStoreInitialized();

  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      BuildGetModelsResponse(
          {},
          {proto::CLIENT_MODEL_FEATURE_FIRST_CONTENTFUL_PAINT_SESSION_MEAN,
           proto::
               CLIENT_MODEL_FEATURE_FIRST_CONTENTFUL_PAINT_SESSION_STANDARD_DEVIATION,
           proto::
               CLIENT_MODEL_FEATURE_FIRST_CONTENTFUL_PAINT_PREVIOUS_PAGE_LOAD});
  prediction_manager()->UpdateHostModelFeaturesForTesting(
      get_models_response.get());
  prediction_manager()->UpdatePredictionModelsForTesting(
      get_models_response.get());

  EXPECT_EQ(OptimizationTargetDecision::kPageLoadMatches,
            prediction_manager()->ShouldTargetNavigation(
                navigation_handle.get(),
                proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

  TestPredictionModel* test_prediction_model =
      static_cast<TestPredictionModel*>(
          prediction_manager()->GetPredictionModelForTesting(
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

  EXPECT_TRUE(test_prediction_model);
  EXPECT_TRUE(test_prediction_model->WasModelEvaluated());

  base::flat_map<std::string, float> evaluated_features =
      test_prediction_model->last_evaluated_features();
  EXPECT_FLOAT_EQ(
      evaluated_features
          ["CLIENT_MODEL_FEATURE_FIRST_CONTENTFUL_PAINT_SESSION_MEAN"],
      200.0);
  EXPECT_FLOAT_EQ(
      evaluated_features["CLIENT_MODEL_FEATURE_FIRST_CONTENTFUL_PAINT_"
                         "SESSION_STANDARD_DEVIATION"],
      50.0);
  EXPECT_FLOAT_EQ(
      evaluated_features
          ["CLIENT_MODEL_FEATURE_FIRST_CONTENTFUL_PAINT_PREVIOUS_PAGE_LOAD"],
      200.0);
}

TEST_F(PredictionManagerTest,
       StoreInitializedAfterOptimizationTargetRegistered) {
  base::HistogramTester histogram_tester;
  CreatePredictionManager({});
  // Ensure that the fetch does not cause any models or features to load.
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));
  prediction_manager()->RegisterOptimizationTargets(
      {optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});
  EXPECT_FALSE(models_and_features_store()->WasHostModelFeaturesLoaded());
  EXPECT_FALSE(models_and_features_store()->WasModelLoaded());
  EXPECT_FALSE(prediction_manager()->GetHostModelFeaturesForHost("foo.com"));

  SetStoreInitialized();
  EXPECT_TRUE(models_and_features_store()->WasHostModelFeaturesLoaded());
  EXPECT_TRUE(models_and_features_store()->WasModelLoaded());
  EXPECT_TRUE(prediction_manager()->GetHostModelFeaturesForHost("foo.com"));

  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 1, 1);
}

TEST_F(PredictionManagerTest,
       StoreInitializedBeforeOptimizationTargetRegistered) {
  base::HistogramTester histogram_tester;
  CreatePredictionManager({});
  // Ensure that the fetch does not cause any models or features to load.
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));
  SetStoreInitialized();

  EXPECT_FALSE(models_and_features_store()->WasHostModelFeaturesLoaded());
  EXPECT_FALSE(models_and_features_store()->WasModelLoaded());
  EXPECT_FALSE(prediction_manager()->GetHostModelFeaturesForHost("foo.com"));
  prediction_manager()->RegisterOptimizationTargets(
      {optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});
  RunUntilIdle();

  EXPECT_TRUE(models_and_features_store()->WasHostModelFeaturesLoaded());
  EXPECT_TRUE(models_and_features_store()->WasModelLoaded());
  EXPECT_TRUE(prediction_manager()->GetHostModelFeaturesForHost("foo.com"));

  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 1, 1);
}

TEST_F(PredictionManagerTest, ModelFetcherTimerRetryDelay) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {optimization_guide::features::kRemoteOptimizationGuideFetching}, {});

  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kFetchModelsAndHostModelFeaturesOverrideTimer);

  CreatePredictionManager({});
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});

  SetStoreInitialized();
  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());

  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());

  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::
              kFetchSuccessWithModelsAndHostsModelFeatures));

  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  EXPECT_TRUE(prediction_model_fetcher()->models_fetched());
}

TEST_F(PredictionManagerTest, ModelFetcherTimerFetchSucceeds) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {optimization_guide::features::kRemoteOptimizationGuideFetching}, {});

  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kFetchModelsAndHostModelFeaturesOverrideTimer);

  CreatePredictionManager({});
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::
              kFetchSuccessWithModelsAndHostsModelFeatures));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});

  SetStoreInitialized();
  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  EXPECT_TRUE(prediction_model_fetcher()->models_fetched());

  // Reset the prediction model fetcher to detect when the next fetch occurs.
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::
              kFetchSuccessWithModelsAndHostsModelFeatures));
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());
  MoveClockForwardBy(
      base::TimeDelta::FromSeconds(kUpdateFetchModelAndFeaturesTimeSecs));
  EXPECT_TRUE(prediction_model_fetcher()->models_fetched());
}

}  // namespace optimization_guide
