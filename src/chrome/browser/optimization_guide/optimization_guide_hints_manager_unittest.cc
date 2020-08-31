// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_hints_manager.h"

#include <string>
#include <utility>

#include "base/base64.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/optimization_guide/optimization_guide_navigation_data.h"
#include "chrome/browser/optimization_guide/optimization_guide_web_contents_observer.h"
#include "chrome/browser/previews/previews_https_notification_infobar_decider.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/optimization_guide/bloom_filter.h"
#include "components/optimization_guide/hints_component_util.h"
#include "components/optimization_guide/hints_fetcher.h"
#include "components/optimization_guide/hints_fetcher_factory.h"
#include "components/optimization_guide/optimization_guide_constants.h"
#include "components/optimization_guide/optimization_guide_decider.h"
#include "components/optimization_guide/optimization_guide_enums.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/optimization_guide_prefs.h"
#include "components/optimization_guide/optimization_guide_service.h"
#include "components/optimization_guide/optimization_guide_switches.h"
#include "components/optimization_guide/proto_database_provider_test_base.h"
#include "components/optimization_guide/top_host_provider.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_web_contents_factory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Retry delay is 16 minutes to allow for kFetchRetryDelaySecs +
// kFetchRandomMaxDelaySecs to pass.
constexpr int kTestFetchRetryDelaySecs = 60 * 16;
constexpr int kUpdateFetchHintsTimeSecs = 24 * 60 * 60;  // 24 hours.

const int kBlackBlacklistBloomFilterNumHashFunctions = 7;
const int kBlackBlacklistBloomFilterNumBits = 511;

void PopulateBlackBlacklistBloomFilter(
    optimization_guide::BloomFilter* bloom_filter) {
  bloom_filter->Add("black.com");
}

void AddBlacklistBloomFilterToConfig(
    optimization_guide::proto::OptimizationType optimization_type,
    const optimization_guide::BloomFilter& blacklist_bloom_filter,
    int num_hash_functions,
    int num_bits,
    optimization_guide::proto::Configuration* config) {
  std::string blacklist_data(
      reinterpret_cast<const char*>(&blacklist_bloom_filter.bytes()[0]),
      blacklist_bloom_filter.bytes().size());
  optimization_guide::proto::OptimizationFilter* blacklist_proto =
      config->add_optimization_blacklists();
  blacklist_proto->set_optimization_type(optimization_type);
  std::unique_ptr<optimization_guide::proto::BloomFilter> bloom_filter_proto =
      std::make_unique<optimization_guide::proto::BloomFilter>();
  bloom_filter_proto->set_num_hash_functions(num_hash_functions);
  bloom_filter_proto->set_num_bits(num_bits);
  bloom_filter_proto->set_data(blacklist_data);
  blacklist_proto->set_allocated_bloom_filter(bloom_filter_proto.release());
}

std::unique_ptr<optimization_guide::proto::GetHintsResponse> BuildHintsResponse(
    const std::vector<std::string>& hosts,
    const std::vector<std::string>& urls) {
  std::unique_ptr<optimization_guide::proto::GetHintsResponse>
      get_hints_response =
          std::make_unique<optimization_guide::proto::GetHintsResponse>();

  for (const auto& host : hosts) {
    optimization_guide::proto::Hint* hint = get_hints_response->add_hints();
    hint->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
    hint->set_key(host);
    optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
    page_hint->set_page_pattern("page pattern");
    optimization_guide::proto::Optimization* opt =
        page_hint->add_whitelisted_optimizations();
    opt->set_optimization_type(optimization_guide::proto::DEFER_ALL_SCRIPT);
  }
  for (const auto& url : urls) {
    optimization_guide::proto::Hint* hint = get_hints_response->add_hints();
    hint->set_key_representation(optimization_guide::proto::FULL_URL);
    hint->set_key(url);
    hint->mutable_max_cache_duration()->set_seconds(60 * 60);
    optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
    page_hint->set_page_pattern(url);
    optimization_guide::proto::Optimization* opt =
        page_hint->add_whitelisted_optimizations();
    opt->set_optimization_type(
        optimization_guide::proto::COMPRESS_PUBLIC_IMAGES);
    opt->mutable_public_image_metadata()->add_url("someurl");
  }
  return get_hints_response;
}

void RunHintsFetchedCallbackWithResponse(
    optimization_guide::HintsFetchedCallback hints_fetched_callback,
    std::unique_ptr<optimization_guide::proto::GetHintsResponse> response) {
  std::move(hints_fetched_callback).Run(std::move(response));
}

}  // namespace

class TestOptimizationGuideService
    : public optimization_guide::OptimizationGuideService {
 public:
  explicit TestOptimizationGuideService(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner)
      : OptimizationGuideService(ui_task_runner) {}

  ~TestOptimizationGuideService() override = default;

  void AddObserver(
      optimization_guide::OptimizationGuideServiceObserver* observer) override {
    add_observer_called_ = true;
  }

  void RemoveObserver(
      optimization_guide::OptimizationGuideServiceObserver* observer) override {
    remove_observer_called_ = true;
  }

  bool AddObserverCalled() const { return add_observer_called_; }
  bool RemoveObserverCalled() const { return remove_observer_called_; }

 private:
  bool add_observer_called_ = false;
  bool remove_observer_called_ = false;
};

// A mock class implementation of TopHostProvider.
class FakeTopHostProvider : public optimization_guide::TopHostProvider {
 public:
  explicit FakeTopHostProvider(const std::vector<std::string>& top_hosts)
      : top_hosts_(top_hosts) {}

  std::vector<std::string> GetTopHosts() override {
    num_top_hosts_called_++;

      return top_hosts_;
  }

  int get_num_top_hosts_called() const { return num_top_hosts_called_; }

 private:
  std::vector<std::string> top_hosts_;
  int num_top_hosts_called_ = 0;
};

enum class HintsFetcherEndState {
  kFetchFailed = 0,
  kFetchSuccessWithHostHints = 1,
  kFetchSuccessWithNoHints = 2,
  kFetchSuccessWithURLHints = 3,
};

// A mock class implementation of HintsFetcher. It will iterate through the
// provided fetch states each time it is called. If it reaches the end of the
// loop, it will just continue using the last fetch state.
class TestHintsFetcher : public optimization_guide::HintsFetcher {
 public:
  TestHintsFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      GURL optimization_guide_service_url,
      PrefService* pref_service,
      const std::vector<HintsFetcherEndState>& fetch_states)
      : HintsFetcher(url_loader_factory,
                     optimization_guide_service_url,
                     pref_service),
        fetch_states_(fetch_states) {
    DCHECK(!fetch_states_.empty());
  }

  bool FetchOptimizationGuideServiceHints(
      const std::vector<std::string>& hosts,
      const std::vector<GURL>& urls,
      const base::flat_set<optimization_guide::proto::OptimizationType>&
          optimization_types,
      optimization_guide::proto::RequestContext request_context,
      optimization_guide::HintsFetchedCallback hints_fetched_callback)
      override {
    HintsFetcherEndState fetch_state =
        num_fetches_requested_ < static_cast<int>(fetch_states_.size())
            ? fetch_states_[num_fetches_requested_]
            : fetch_states_.back();
    num_fetches_requested_++;
    switch (fetch_state) {
      case HintsFetcherEndState::kFetchFailed:
        std::move(hints_fetched_callback).Run(base::nullopt);
        return false;
      case HintsFetcherEndState::kFetchSuccessWithHostHints:
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(&RunHintsFetchedCallbackWithResponse,
                                      std::move(hints_fetched_callback),
                                      BuildHintsResponse({"host.com"}, {})));
        return true;
      case HintsFetcherEndState::kFetchSuccessWithURLHints:
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE,
            base::BindOnce(&RunHintsFetchedCallbackWithResponse,
                           std::move(hints_fetched_callback),
                           BuildHintsResponse(
                               {}, {"https://somedomain.org/news/whatever"})));
        return true;
      case HintsFetcherEndState::kFetchSuccessWithNoHints:
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(&RunHintsFetchedCallbackWithResponse,
                                      std::move(hints_fetched_callback),
                                      BuildHintsResponse({}, {})));
        return true;
    }
    return true;
  }

  int num_fetches_requested() { return num_fetches_requested_; }

 private:
  std::vector<HintsFetcherEndState> fetch_states_;
  int num_fetches_requested_ = 0;
};

// A mock class of HintsFetcherFactory that returns instances of
// TestHintsFetchers with the provided fetch state.
class TestHintsFetcherFactory : public optimization_guide::HintsFetcherFactory {
 public:
  TestHintsFetcherFactory(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      GURL optimization_guide_service_url,
      PrefService* pref_service,
      std::vector<HintsFetcherEndState> fetch_states)
      : HintsFetcherFactory(url_loader_factory,
                            optimization_guide_service_url,
                            pref_service),
        fetch_states_(fetch_states) {}

  std::unique_ptr<optimization_guide::HintsFetcher> BuildInstance() override {
    return std::make_unique<TestHintsFetcher>(url_loader_factory_,
                                              optimization_guide_service_url_,
                                              pref_service_, fetch_states_);
  }

 private:
  std::vector<HintsFetcherEndState> fetch_states_;
};

class OptimizationGuideHintsManagerTest
    : public optimization_guide::ProtoDatabaseProviderTestBase {
 public:
  OptimizationGuideHintsManagerTest() = default;
  ~OptimizationGuideHintsManagerTest() override = default;

  void SetUp() override {
    optimization_guide::ProtoDatabaseProviderTestBase::SetUp();
    web_contents_factory_.reset(new content::TestWebContentsFactory);
    CreateServiceAndHintsManager(
        /*optimization_types_at_initialization=*/{},
        /*top_host_provider=*/nullptr);
  }

  void TearDown() override {
    optimization_guide::ProtoDatabaseProviderTestBase::TearDown();
    ResetHintsManager();
  }

  void CreateServiceAndHintsManager(
      const std::vector<optimization_guide::proto::OptimizationType>&
          optimization_types_at_initialization,
      optimization_guide::TopHostProvider* top_host_provider) {
    if (hints_manager_) {
      ResetHintsManager();
    }
    optimization_guide_service_ =
        std::make_unique<TestOptimizationGuideService>(
            task_environment_.GetMainThreadTaskRunner());
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    optimization_guide::prefs::RegisterProfilePrefs(pref_service_->registry());

    url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    hints_manager_ = std::make_unique<OptimizationGuideHintsManager>(
        optimization_types_at_initialization, optimization_guide_service_.get(),
        &testing_profile_, temp_dir(), pref_service_.get(), db_provider_.get(),
        top_host_provider, url_loader_factory_);
    hints_manager_->SetClockForTesting(task_environment_.GetMockClock());

    // Add observer is called after the HintCache is fully initialized,
    // indicating that the OptimizationGuideHintsManager is ready to process
    // hints.
    while (!optimization_guide_service_->AddObserverCalled()) {
      RunUntilIdle();
    }
  }

  void ResetHintsManager() {
    hints_manager_.reset();
    RunUntilIdle();
  }

  void ProcessInvalidHintsComponentInfo(const std::string& version) {
    optimization_guide::HintsComponentInfo info(
        base::Version(version),
        temp_dir().Append(FILE_PATH_LITERAL("notaconfigfile")));

    base::RunLoop run_loop;
    hints_manager_->ListenForNextUpdateForTesting(run_loop.QuitClosure());
    hints_manager_->OnHintsComponentAvailable(info);
    run_loop.Run();
  }

  void ProcessHints(const optimization_guide::proto::Configuration& config,
                    const std::string& version) {
    optimization_guide::HintsComponentInfo info(
        base::Version(version),
        temp_dir().Append(FILE_PATH_LITERAL("somefile.pb")));
    ASSERT_NO_FATAL_FAILURE(WriteConfigToFile(config, info.path));

    base::RunLoop run_loop;
    hints_manager_->ListenForNextUpdateForTesting(run_loop.QuitClosure());
    hints_manager_->OnHintsComponentAvailable(info);
    run_loop.Run();
  }

  void InitializeWithDefaultConfig(const std::string& version) {
    optimization_guide::proto::Configuration config;
    optimization_guide::proto::Hint* hint1 = config.add_hints();
    hint1->set_key("somedomain.org");
    hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
    hint1->set_version("someversion");
    optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
    page_hint1->set_page_pattern("/news/");
    page_hint1->set_max_ect_trigger(
        optimization_guide::proto::EFFECTIVE_CONNECTION_TYPE_3G);
    optimization_guide::proto::Optimization* experimental_opt =
        page_hint1->add_whitelisted_optimizations();
    experimental_opt->set_optimization_type(
        optimization_guide::proto::NOSCRIPT);
    experimental_opt->set_experiment_name("experiment");
    optimization_guide::proto::PreviewsMetadata* experimental_opt_metadata =
        experimental_opt->mutable_previews_metadata();
    experimental_opt_metadata->set_inflation_percent(12345);
    optimization_guide::proto::Optimization* default_opt =
        page_hint1->add_whitelisted_optimizations();
    default_opt->set_optimization_type(optimization_guide::proto::NOSCRIPT);
    optimization_guide::proto::PreviewsMetadata* default_opt_metadata =
        default_opt->mutable_previews_metadata();
    default_opt_metadata->set_inflation_percent(1234);

    ProcessHints(config, version);
  }

  std::unique_ptr<optimization_guide::HintsFetcherFactory>
  BuildTestHintsFetcherFactory(
      const std::vector<HintsFetcherEndState>& fetch_states) {
    return std::make_unique<TestHintsFetcherFactory>(
        url_loader_factory_, GURL("https://hintsserver.com"), pref_service(),
        fetch_states);
  }

  void MoveClockForwardBy(base::TimeDelta time_delta) {
    task_environment_.FastForwardBy(time_delta);
    RunUntilIdle();
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

  OptimizationGuideHintsManager* hints_manager() const {
    return hints_manager_.get();
  }

  TestHintsFetcher* batch_update_hints_fetcher() const {
    return static_cast<TestHintsFetcher*>(
        hints_manager()->batch_update_hints_fetcher());
  }

  GURL url_with_hints() const {
    return GURL("https://somedomain.org/news/whatever");
  }

  GURL url_with_url_keyed_hint() const {
    return GURL("https://somedomain.org/news/whatever");
  }

  GURL url_without_hints() const {
    return GURL("https://url_without_hints.org/");
  }

  base::FilePath temp_dir() const { return temp_dir_.GetPath(); }

  TestingPrefServiceSimple* pref_service() const { return pref_service_.get(); }

  void RunUntilIdle() {
    task_environment_.RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

 private:
  void WriteConfigToFile(const optimization_guide::proto::Configuration& config,
                         const base::FilePath& filePath) {
    std::string serialized_config;
    ASSERT_TRUE(config.SerializeToString(&serialized_config));
    ASSERT_EQ(static_cast<int32_t>(serialized_config.size()),
              base::WriteFile(filePath, serialized_config.data(),
                              serialized_config.size()));
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfile testing_profile_;
  std::unique_ptr<content::TestWebContentsFactory> web_contents_factory_;
  std::unique_ptr<OptimizationGuideHintsManager> hints_manager_;
  std::unique_ptr<TestOptimizationGuideService> optimization_guide_service_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(OptimizationGuideHintsManagerTest);
};

TEST_F(OptimizationGuideHintsManagerTest,
       OptimizationTypesProvidedAtInitializationAreRegistered) {
  std::vector<optimization_guide::proto::OptimizationType>
      optimization_targets =
          std::vector<optimization_guide::proto::OptimizationType>{
              optimization_guide::proto::DEFER_ALL_SCRIPT};
  CreateServiceAndHintsManager(optimization_targets,
                               /*top_host_provider=*/nullptr);

  EXPECT_FALSE(hints_manager()->registered_optimization_types().empty());
  optimization_guide::proto::Configuration config;
  optimization_guide::BloomFilter blacklist_bloom_filter(
      kBlackBlacklistBloomFilterNumHashFunctions,
      kBlackBlacklistBloomFilterNumBits);
  PopulateBlackBlacklistBloomFilter(&blacklist_bloom_filter);
  AddBlacklistBloomFilterToConfig(optimization_guide::proto::DEFER_ALL_SCRIPT,
                                  blacklist_bloom_filter,
                                  kBlackBlacklistBloomFilterNumHashFunctions,
                                  kBlackBlacklistBloomFilterNumBits, &config);
  AddBlacklistBloomFilterToConfig(optimization_guide::proto::NOSCRIPT,
                                  blacklist_bloom_filter,
                                  kBlackBlacklistBloomFilterNumHashFunctions,
                                  kBlackBlacklistBloomFilterNumBits, &config);

  base::HistogramTester histogram_tester;

  ProcessHints(config, "1.0.0.0");

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationFilterStatus.DeferAllScript",
      optimization_guide::OptimizationFilterStatus::kFoundServerBlacklistConfig,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationFilterStatus.DeferAllScript",
      optimization_guide::OptimizationFilterStatus::kCreatedServerBlacklist, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.OptimizationFilterStatus.NoScript", 0);
}

TEST_F(OptimizationGuideHintsManagerTest,
       ProcessHintsWithValidCommandLineOverride) {
  base::HistogramTester histogram_tester;

  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint = config.add_hints();
  hint->set_key("somedomain.org");
  hint->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("noscript_default_2g");
  optimization_guide::proto::Optimization* optimization =
      page_hint->add_whitelisted_optimizations();
  optimization->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  std::string encoded_config;
  config.SerializeToString(&encoded_config);
  base::Base64Encode(encoded_config, &encoded_config);

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      optimization_guide::switches::kHintsProtoOverride, encoded_config);
  CreateServiceAndHintsManager(/*optimization_types_at_initialization=*/{},
                               /*top_host_provider=*/nullptr);

  // The below histogram should not be recorded since hints weren't coming
  // directly from the component.
  histogram_tester.ExpectTotalCount("OptimizationGuide.ProcessHintsResult", 0);
  // However, we still expect the local histogram for the hints being updated to
  // be recorded.
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.UpdateComponentHints.Result", true, 1);
}

TEST_F(OptimizationGuideHintsManagerTest,
       ProcessHintsWithInvalidCommandLineOverride) {
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      optimization_guide::switches::kHintsProtoOverride, "this-is-not-a-proto");
  CreateServiceAndHintsManager(/*optimization_types_at_initialization=*/{},
                               /*top_host_provider=*/nullptr);

  // The below histogram should not be recorded since hints weren't coming
  // directly from the component.
  histogram_tester.ExpectTotalCount("OptimizationGuide.ProcessHintsResult", 0);
  // We also do not expect to update the component hints with bad hints either.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.UpdateComponentHints.Result", 0);
}

TEST_F(OptimizationGuideHintsManagerTest,
       ProcessHintsWithCommandLineOverrideShouldNotBeOverriddenByNewComponent) {
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint = config.add_hints();
  hint->set_key("somedomain.org");
  hint->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("noscript_default_2g");
  optimization_guide::proto::Optimization* optimization =
      page_hint->add_whitelisted_optimizations();
  optimization->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  std::string encoded_config;
  config.SerializeToString(&encoded_config);
  base::Base64Encode(encoded_config, &encoded_config);

  {
    base::HistogramTester histogram_tester;
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        optimization_guide::switches::kHintsProtoOverride, encoded_config);
    CreateServiceAndHintsManager(
        /*optimization_types_at_initialization=*/{},
        /*top_host_provider=*/nullptr);
    // The below histogram should not be recorded since hints weren't coming
    // directly from the component.
    histogram_tester.ExpectTotalCount("OptimizationGuide.ProcessHintsResult",
                                      0);
    // However, we still expect the local histogram for the hints being updated
    // to be recorded.
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.UpdateComponentHints.Result", true, 1);
  }

  // Test that a new component coming in does not update the component hints.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("3.0.0.0");
    // The below histograms should not be recorded since component hints
    // processing is disabled.
    histogram_tester.ExpectTotalCount("OptimizationGuide.ProcessHintsResult",
                                      0);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.UpdateComponentHints.Result", 0);
  }
}

TEST_F(OptimizationGuideHintsManagerTest, ParseTwoConfigVersions) {
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("somedomain.org");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  hint1->set_version("someversion");
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("/news/");
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);
  optimization_guide::proto::ResourceLoadingHint* resource_loading_hint1 =
      optimization1->mutable_previews_metadata()->add_resource_loading_hints();
  resource_loading_hint1->set_loading_optimization_type(
      optimization_guide::proto::LOADING_BLOCK_RESOURCE);
  resource_loading_hint1->set_resource_pattern("news_cruft.js");

  // Test the first time parsing the config.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("1.0.0.0");
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        optimization_guide::ProcessHintsComponentResult::kSuccess, 1);
  }

  // Test the second time parsing the config. This should also update the hints.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("2.0.0.0");
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        optimization_guide::ProcessHintsComponentResult::kSuccess, 1);
  }
}

TEST_F(OptimizationGuideHintsManagerTest, ParseOlderConfigVersions) {
  // Test the first time parsing the config.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("10.0.0.0");
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        optimization_guide::ProcessHintsComponentResult::kSuccess, 1);
  }

  // Test the second time parsing the config. This will be treated by the cache
  // as an older version.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("2.0.0.0");
    // If we have already parsed a version later than this version, we expect
    // for the hints to not be updated.
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        optimization_guide::ProcessHintsComponentResult::
            kSkippedProcessingHints,
        1);
  }
}

TEST_F(OptimizationGuideHintsManagerTest, ParseDuplicateConfigVersions) {
  const std::string version = "3.0.0.0";

  // Test the first time parsing the config.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig(version);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        optimization_guide::ProcessHintsComponentResult::kSuccess, 1);
  }

  // Test the second time parsing the config. This will be treated by the cache
  // as a duplicate version.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig(version);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        optimization_guide::ProcessHintsComponentResult::
            kSkippedProcessingHints,
        1);
  }
}

TEST_F(OptimizationGuideHintsManagerTest, ComponentInfoDidNotContainConfig) {
  base::HistogramTester histogram_tester;
  ProcessInvalidHintsComponentInfo("1.0.0.0");
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ProcessHintsResult",
      optimization_guide::ProcessHintsComponentResult::kFailedReadingFile, 1);
}

TEST_F(OptimizationGuideHintsManagerTest, ProcessHintsWithExistingPref) {
  // Write hints processing pref for version 2.0.0.
  pref_service()->SetString(
      optimization_guide::prefs::kPendingHintsProcessingVersion, "2.0.0");

  // Verify config not processed for same version (2.0.0) and pref not cleared.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("2.0.0");
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        optimization_guide::ProcessHintsComponentResult::
            kFailedFinishProcessing,
        1);
    EXPECT_FALSE(
        pref_service()
            ->GetString(
                optimization_guide::prefs::kPendingHintsProcessingVersion)
            .empty());
  }

  // Now verify config is processed for different version and pref cleared.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("3.0.0");
    EXPECT_TRUE(
        pref_service()
            ->GetString(
                optimization_guide::prefs::kPendingHintsProcessingVersion)
            .empty());
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        optimization_guide::ProcessHintsComponentResult::kSuccess, 1);
  }
}

TEST_F(OptimizationGuideHintsManagerTest, ProcessHintsWithInvalidPref) {
  // Create pref file with invalid version.
  pref_service()->SetString(
      optimization_guide::prefs::kPendingHintsProcessingVersion, "bad-2.0.0");

  // Verify config not processed for existing pref with bad value but
  // that the pref is cleared.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("2.0.0");
    EXPECT_TRUE(
        pref_service()
            ->GetString(
                optimization_guide::prefs::kPendingHintsProcessingVersion)
            .empty());
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        optimization_guide::ProcessHintsComponentResult::
            kFailedFinishProcessing,
        1);
  }

  // Now verify config is processed with pref cleared.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("2.0.0");
    EXPECT_TRUE(
        pref_service()
            ->GetString(
                optimization_guide::prefs::kPendingHintsProcessingVersion)
            .empty());
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        optimization_guide::ProcessHintsComponentResult::kSuccess, 1);
  }
}

TEST_F(OptimizationGuideHintsManagerTest,
       OnNavigationStartOrRedirectWithHintAfterCommit) {
  base::HistogramTester histogram_tester;
  InitializeWithDefaultConfig("3.0.0.0");

  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_with_hints());
  navigation_handle->set_has_committed(true);

  base::RunLoop run_loop;
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               run_loop.QuitClosure());
  run_loop.Run();

  histogram_tester.ExpectUniqueSample("OptimizationGuide.LoadedHint.Result",
                                      true, 1);
}

TEST_F(OptimizationGuideHintsManagerTest, OnNavigationStartOrRedirectWithHint) {
  base::HistogramTester histogram_tester;
  InitializeWithDefaultConfig("3.0.0.0");

  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_with_hints());

  base::RunLoop run_loop;
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               run_loop.QuitClosure());
  run_loop.Run();

  histogram_tester.ExpectUniqueSample("OptimizationGuide.LoadedHint.Result",
                                      true, 1);
}

TEST_F(OptimizationGuideHintsManagerTest, OnNavigationStartOrRedirectNoHint) {
  base::HistogramTester histogram_tester;
  InitializeWithDefaultConfig("3.0.0.0");

  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://notinhints.com"));

  base::RunLoop run_loop;
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               run_loop.QuitClosure());
  run_loop.Run();

  histogram_tester.ExpectUniqueSample("OptimizationGuide.LoadedHint.Result",
                                      false, 1);
}

TEST_F(OptimizationGuideHintsManagerTest, OnNavigationStartOrRedirectNoHost) {
  base::HistogramTester histogram_tester;
  InitializeWithDefaultConfig("3.0.0.0");

  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("blargh"));

  base::RunLoop run_loop;
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               run_loop.QuitClosure());
  run_loop.Run();

  histogram_tester.ExpectTotalCount("OptimizationGuide.LoadedHint.Result", 0);
}

TEST_F(OptimizationGuideHintsManagerTest,
       OptimizationFiltersAreOnlyLoadedIfTypeIsRegistered) {
  optimization_guide::proto::Configuration config;
  optimization_guide::BloomFilter blacklist_bloom_filter(
      kBlackBlacklistBloomFilterNumHashFunctions,
      kBlackBlacklistBloomFilterNumBits);
  PopulateBlackBlacklistBloomFilter(&blacklist_bloom_filter);
  AddBlacklistBloomFilterToConfig(optimization_guide::proto::LITE_PAGE_REDIRECT,
                                  blacklist_bloom_filter,
                                  kBlackBlacklistBloomFilterNumHashFunctions,
                                  kBlackBlacklistBloomFilterNumBits, &config);
  AddBlacklistBloomFilterToConfig(optimization_guide::proto::NOSCRIPT,
                                  blacklist_bloom_filter,
                                  kBlackBlacklistBloomFilterNumHashFunctions,
                                  kBlackBlacklistBloomFilterNumBits, &config);

  {
    base::HistogramTester histogram_tester;

    ProcessHints(config, "1.0.0.0");

    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect", 0);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.NoScript", 0);
  }

  // Now register the optimization type and see that it is loaded.
  {
    base::HistogramTester histogram_tester;

    base::RunLoop run_loop;
    hints_manager()->ListenForNextUpdateForTesting(run_loop.QuitClosure());
    hints_manager()->RegisterOptimizationTypes(
        {optimization_guide::proto::LITE_PAGE_REDIRECT});
    run_loop.Run();

    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
        optimization_guide::OptimizationFilterStatus::
            kFoundServerBlacklistConfig,
        1);
    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
        optimization_guide::OptimizationFilterStatus::kCreatedServerBlacklist,
        1);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.NoScript", 0);
    EXPECT_TRUE(hints_manager()->HasLoadedOptimizationFilter(
        optimization_guide::proto::LITE_PAGE_REDIRECT));
    EXPECT_FALSE(hints_manager()->HasLoadedOptimizationFilter(
        optimization_guide::proto::NOSCRIPT));
  }

  // Re-registering the same optimization type does not re-load the filter.
  {
    base::HistogramTester histogram_tester;

    base::RunLoop run_loop;
    hints_manager()->ListenForNextUpdateForTesting(run_loop.QuitClosure());
    hints_manager()->RegisterOptimizationTypes(
        {optimization_guide::proto::LITE_PAGE_REDIRECT});
    run_loop.Run();

    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect", 0);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.NoScript", 0);
  }

  // Registering a new optimization type without a filter does not trigger a
  // reload of the filter.
  {
    base::HistogramTester histogram_tester;

    base::RunLoop run_loop;
    hints_manager()->ListenForNextUpdateForTesting(run_loop.QuitClosure());
    hints_manager()->RegisterOptimizationTypes(
        {optimization_guide::proto::DEFER_ALL_SCRIPT});
    run_loop.Run();

    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect", 0);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.NoScript", 0);
  }

  // Registering a new optimization type with a filter does trigger a
  // reload of the filters.
  {
    base::HistogramTester histogram_tester;

    base::RunLoop run_loop;
    hints_manager()->ListenForNextUpdateForTesting(run_loop.QuitClosure());
    hints_manager()->RegisterOptimizationTypes(
        {optimization_guide::proto::NOSCRIPT});
    run_loop.Run();

    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
        optimization_guide::OptimizationFilterStatus::
            kFoundServerBlacklistConfig,
        1);
    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
        optimization_guide::OptimizationFilterStatus::kCreatedServerBlacklist,
        1);
    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.NoScript",
        optimization_guide::OptimizationFilterStatus::
            kFoundServerBlacklistConfig,
        1);
    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.NoScript",
        optimization_guide::OptimizationFilterStatus::kCreatedServerBlacklist,
        1);
    EXPECT_TRUE(hints_manager()->HasLoadedOptimizationFilter(
        optimization_guide::proto::LITE_PAGE_REDIRECT));
    EXPECT_TRUE(hints_manager()->HasLoadedOptimizationFilter(
        optimization_guide::proto::NOSCRIPT));
  }
}

TEST_F(OptimizationGuideHintsManagerTest,
       OptimizationFiltersOnlyLoadOncePerType) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LITE_PAGE_REDIRECT});

  base::HistogramTester histogram_tester;

  optimization_guide::proto::Configuration config;
  optimization_guide::BloomFilter blacklist_bloom_filter(
      kBlackBlacklistBloomFilterNumHashFunctions,
      kBlackBlacklistBloomFilterNumBits);
  PopulateBlackBlacklistBloomFilter(&blacklist_bloom_filter);
  AddBlacklistBloomFilterToConfig(optimization_guide::proto::LITE_PAGE_REDIRECT,
                                  blacklist_bloom_filter,
                                  kBlackBlacklistBloomFilterNumHashFunctions,
                                  kBlackBlacklistBloomFilterNumBits, &config);
  AddBlacklistBloomFilterToConfig(optimization_guide::proto::LITE_PAGE_REDIRECT,
                                  blacklist_bloom_filter,
                                  kBlackBlacklistBloomFilterNumHashFunctions,
                                  kBlackBlacklistBloomFilterNumBits, &config);
  ProcessHints(config, "1.0.0.0");

  // We found 2 LPR blacklists: parsed one and duped the other.
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
      optimization_guide::OptimizationFilterStatus::kFoundServerBlacklistConfig,
      2);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
      optimization_guide::OptimizationFilterStatus::kCreatedServerBlacklist, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
      optimization_guide::OptimizationFilterStatus::
          kFailedServerBlacklistDuplicateConfig,
      1);
}

TEST_F(OptimizationGuideHintsManagerTest, InvalidOptimizationFilterNotLoaded) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LITE_PAGE_REDIRECT});

  base::HistogramTester histogram_tester;

  int too_many_bits =
      optimization_guide::features::MaxServerBloomFilterByteSize() * 8 + 1;

  optimization_guide::proto::Configuration config;
  optimization_guide::BloomFilter blacklist_bloom_filter(
      kBlackBlacklistBloomFilterNumHashFunctions, too_many_bits);
  PopulateBlackBlacklistBloomFilter(&blacklist_bloom_filter);
  AddBlacklistBloomFilterToConfig(
      optimization_guide::proto::LITE_PAGE_REDIRECT, blacklist_bloom_filter,
      kBlackBlacklistBloomFilterNumHashFunctions, too_many_bits, &config);
  ProcessHints(config, "1.0.0.0");

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
      optimization_guide::OptimizationFilterStatus::kFoundServerBlacklistConfig,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
      optimization_guide::OptimizationFilterStatus::
          kFailedServerBlacklistTooBig,
      1);
  EXPECT_FALSE(hints_manager()->HasLoadedOptimizationFilter(
      optimization_guide::proto::LITE_PAGE_REDIRECT));
}

TEST_F(OptimizationGuideHintsManagerTest, CanApplyOptimizationUrlWithNoHost) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LITE_PAGE_REDIRECT});

  optimization_guide::proto::Configuration config;
  optimization_guide::BloomFilter blacklist_bloom_filter(
      kBlackBlacklistBloomFilterNumHashFunctions,
      kBlackBlacklistBloomFilterNumBits);
  PopulateBlackBlacklistBloomFilter(&blacklist_bloom_filter);
  AddBlacklistBloomFilterToConfig(optimization_guide::proto::LITE_PAGE_REDIRECT,
                                  blacklist_bloom_filter,
                                  kBlackBlacklistBloomFilterNumHashFunctions,
                                  kBlackBlacklistBloomFilterNumBits, &config);
  ProcessHints(config, "1.0.0.0");

  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("urlwithnohost"));

  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_handle.get(),
          optimization_guide::proto::LITE_PAGE_REDIRECT,
          /*optimization_metadata=*/nullptr);

  // Make sure decisions are logged correctly.
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kNoHintAvailable,
            optimization_type_decision);
  // Make sure navigation data is populated correctly.
  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  EXPECT_FALSE(navigation_data->has_page_hint_value());
}

TEST_F(OptimizationGuideHintsManagerTest,
       ShouldTargetNavigationUrlWithNoHostECTSlowerThanDefault) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LITE_PAGE_REDIRECT});

  optimization_guide::proto::Configuration config;
  optimization_guide::BloomFilter blacklist_bloom_filter(
      kBlackBlacklistBloomFilterNumHashFunctions,
      kBlackBlacklistBloomFilterNumBits);
  PopulateBlackBlacklistBloomFilter(&blacklist_bloom_filter);
  AddBlacklistBloomFilterToConfig(optimization_guide::proto::LITE_PAGE_REDIRECT,
                                  blacklist_bloom_filter,
                                  kBlackBlacklistBloomFilterNumHashFunctions,
                                  kBlackBlacklistBloomFilterNumBits, &config);
  ProcessHints(config, "1.0.0.0");

  // Set ECT estimate to be "painful".
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_2G);
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("urlwithnohost"));

  optimization_guide::OptimizationTargetDecision optimization_target_decision =
      hints_manager()->ShouldTargetNavigation(
          navigation_handle.get(),
          optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);

  // Make sure decisions are logged correctly.
  EXPECT_EQ(optimization_guide::OptimizationTargetDecision::kPageLoadMatches,
            optimization_target_decision);
}

TEST_F(OptimizationGuideHintsManagerTest,
       ShouldTargetNavigationUrlWithNoHostECTFasterThanDefault) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LITE_PAGE_REDIRECT});

  optimization_guide::proto::Configuration config;
  optimization_guide::BloomFilter blacklist_bloom_filter(
      kBlackBlacklistBloomFilterNumHashFunctions,
      kBlackBlacklistBloomFilterNumBits);
  PopulateBlackBlacklistBloomFilter(&blacklist_bloom_filter);
  AddBlacklistBloomFilterToConfig(optimization_guide::proto::LITE_PAGE_REDIRECT,
                                  blacklist_bloom_filter,
                                  kBlackBlacklistBloomFilterNumHashFunctions,
                                  kBlackBlacklistBloomFilterNumBits, &config);
  ProcessHints(config, "1.0.0.0");

  // Set ECT estimate to be "fast".
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_3G);
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("urlwithnohost"));

  optimization_guide::OptimizationTargetDecision optimization_target_decision =
      hints_manager()->ShouldTargetNavigation(
          navigation_handle.get(),
          optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);

  // Make sure decisions are logged correctly.
  EXPECT_EQ(
      optimization_guide::OptimizationTargetDecision::kPageLoadDoesNotMatch,
      optimization_target_decision);
}

TEST_F(OptimizationGuideHintsManagerTest,
       CanApplyOptimizationHasFilterForTypeButNotLoadedYet) {
  optimization_guide::proto::Configuration config;
  optimization_guide::BloomFilter blacklist_bloom_filter(
      kBlackBlacklistBloomFilterNumHashFunctions,
      kBlackBlacklistBloomFilterNumBits);
  PopulateBlackBlacklistBloomFilter(&blacklist_bloom_filter);
  AddBlacklistBloomFilterToConfig(optimization_guide::proto::LITE_PAGE_REDIRECT,
                                  blacklist_bloom_filter,
                                  kBlackBlacklistBloomFilterNumHashFunctions,
                                  kBlackBlacklistBloomFilterNumBits, &config);
  ProcessHints(config, "1.0.0.0");
  // Append the switch for processing hints to force the filter to not get
  // loaded.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kHintsProtoOverride);

  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://whatever.com/123"));
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LITE_PAGE_REDIRECT});
  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_handle.get(),
          optimization_guide::proto::LITE_PAGE_REDIRECT,
          /*optimization_metadata=*/nullptr);

  // Make sure decisions are logged correctly.
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::
                kHadOptimizationFilterButNotLoadedInTime,
            optimization_type_decision);
  // Make sure navigation data is populated correctly.
  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  EXPECT_FALSE(navigation_data->has_page_hint_value());

  // Run until idle to ensure we don't crash because the test object has gone
  // away.
  RunUntilIdle();
}

TEST_F(OptimizationGuideHintsManagerTest,
       CanApplyOptimizationHasLoadedFilterForTypeUrlInBlacklistFilter) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LITE_PAGE_REDIRECT});

  optimization_guide::proto::Configuration config;
  optimization_guide::BloomFilter blacklist_bloom_filter(
      kBlackBlacklistBloomFilterNumHashFunctions,
      kBlackBlacklistBloomFilterNumBits);
  PopulateBlackBlacklistBloomFilter(&blacklist_bloom_filter);
  AddBlacklistBloomFilterToConfig(optimization_guide::proto::LITE_PAGE_REDIRECT,
                                  blacklist_bloom_filter,
                                  kBlackBlacklistBloomFilterNumHashFunctions,
                                  kBlackBlacklistBloomFilterNumBits, &config);
  ProcessHints(config, "1.0.0.0");

  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://m.black.com/123"));

  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_handle.get(),
          optimization_guide::proto::LITE_PAGE_REDIRECT,
          /*optimization_metadata=*/nullptr);

  // Make sure decisions are logged correctly.
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::
                kNotAllowedByOptimizationFilter,
            optimization_type_decision);
  // Make sure navigation data is populated correctly.
  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  EXPECT_FALSE(navigation_data->has_page_hint_value());
}

TEST_F(OptimizationGuideHintsManagerTest,
       CanApplyOptimizationHasLoadedFilterForTypeUrlNotInBlacklistFilter) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LITE_PAGE_REDIRECT});

  optimization_guide::proto::Configuration config;
  optimization_guide::BloomFilter blacklist_bloom_filter(
      kBlackBlacklistBloomFilterNumHashFunctions,
      kBlackBlacklistBloomFilterNumBits);
  PopulateBlackBlacklistBloomFilter(&blacklist_bloom_filter);
  AddBlacklistBloomFilterToConfig(optimization_guide::proto::LITE_PAGE_REDIRECT,
                                  blacklist_bloom_filter,
                                  kBlackBlacklistBloomFilterNumHashFunctions,
                                  kBlackBlacklistBloomFilterNumBits, &config);
  ProcessHints(config, "1.0.0.0");

  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://whatever.com/123"));

  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_handle.get(),
          optimization_guide::proto::LITE_PAGE_REDIRECT,
          /*optimization_metadata=*/nullptr);

  // Make sure decisions are logged correctly.
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::
                kAllowedByOptimizationFilter,
            optimization_type_decision);
  // Make sure navigation data is populated correctly.
  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  EXPECT_FALSE(navigation_data->has_page_hint_value());
}

TEST_F(OptimizationGuideHintsManagerTest, ShouldTargetNavigationNoECTEstimate) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LITE_PAGE_REDIRECT});

  optimization_guide::proto::Configuration config;
  optimization_guide::BloomFilter blacklist_bloom_filter(
      kBlackBlacklistBloomFilterNumHashFunctions,
      kBlackBlacklistBloomFilterNumBits);
  PopulateBlackBlacklistBloomFilter(&blacklist_bloom_filter);
  AddBlacklistBloomFilterToConfig(optimization_guide::proto::LITE_PAGE_REDIRECT,
                                  blacklist_bloom_filter,
                                  kBlackBlacklistBloomFilterNumHashFunctions,
                                  kBlackBlacklistBloomFilterNumBits, &config);
  ProcessHints(config, "1.0.0.0");

  // Explicitly set ECT estimate to be unknown.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://whatever.com/123"));

  optimization_guide::OptimizationTargetDecision optimization_target_decision =
      hints_manager()->ShouldTargetNavigation(
          navigation_handle.get(),
          optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);

  // Make sure decisions are logged correctly.
  EXPECT_EQ(
      optimization_guide::OptimizationTargetDecision::kPageLoadDoesNotMatch,
      optimization_target_decision);
}

TEST_F(OptimizationGuideHintsManagerTest,
       ShouldTargetNavigationNoHintToTriggerHigherThan2G) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LITE_PAGE_REDIRECT});

  optimization_guide::proto::Configuration config;
  optimization_guide::BloomFilter blacklist_bloom_filter(
      kBlackBlacklistBloomFilterNumHashFunctions,
      kBlackBlacklistBloomFilterNumBits);
  PopulateBlackBlacklistBloomFilter(&blacklist_bloom_filter);
  AddBlacklistBloomFilterToConfig(optimization_guide::proto::LITE_PAGE_REDIRECT,
                                  blacklist_bloom_filter,
                                  kBlackBlacklistBloomFilterNumHashFunctions,
                                  kBlackBlacklistBloomFilterNumBits, &config);
  ProcessHints(config, "1.0.0.0");

  // Explicitly set ECT estimate to be unknown.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_3G);
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://whatever.com/123"));

  optimization_guide::OptimizationTargetDecision optimization_target_decision =
      hints_manager()->ShouldTargetNavigation(
          navigation_handle.get(),
          optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);

  // Make sure decisions are logged correctly.
  EXPECT_EQ(
      optimization_guide::OptimizationTargetDecision::kPageLoadDoesNotMatch,
      optimization_target_decision);
}

TEST_F(OptimizationGuideHintsManagerTest,
       CanApplyOptimizationAndPopulatesMetadataWithFirstOptThatMatchesNoExp) {
  InitializeWithDefaultConfig("1.0.0.0");
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::NOSCRIPT});

  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_with_hints());
  base::RunLoop run_loop;
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               run_loop.QuitClosure());
  run_loop.Run();

  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(navigation_handle.get(),
                                            optimization_guide::proto::NOSCRIPT,
                                            &optimization_metadata);
  EXPECT_EQ(
      1234,
      optimization_metadata.previews_metadata().value().inflation_percent());

  // Make sure decisions are logged correctly.
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kAllowedByHint,
            optimization_type_decision);
  // Make sure navigation data is populated correctly.
  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  ASSERT_TRUE(navigation_data->page_hint());
}

TEST_F(OptimizationGuideHintsManagerTest,
       ShouldTargetNavigationButNotSlowEnough) {
  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so hint is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_4G);
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_with_hints());
  base::RunLoop run_loop;
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               run_loop.QuitClosure());
  run_loop.Run();

  optimization_guide::OptimizationTargetDecision optimization_target_decision =
      hints_manager()->ShouldTargetNavigation(
          navigation_handle.get(),
          optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);

  // Make sure decisions are logged correctly.
  EXPECT_EQ(
      optimization_guide::OptimizationTargetDecision::kPageLoadDoesNotMatch,
      optimization_target_decision);
}

TEST_F(OptimizationGuideHintsManagerTest,
       ShouldTargetNavigationWithNonPainfulPageLoadTarget) {
  InitializeWithDefaultConfig("1.0.0.0");

  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_with_hints());
  base::RunLoop run_loop;
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               run_loop.QuitClosure());
  run_loop.Run();

  optimization_guide::OptimizationTargetDecision optimization_target_decision =
      hints_manager()->ShouldTargetNavigation(
          navigation_handle.get(),
          optimization_guide::proto::OPTIMIZATION_TARGET_UNKNOWN);

  // Make sure decisions are logged correctly.
  EXPECT_EQ(optimization_guide::OptimizationTargetDecision::
                kModelNotAvailableOnClient,
            optimization_target_decision);
}

TEST_F(OptimizationGuideHintsManagerTest,
       CanApplyOptimizationHasPageHintButNoMatchingOptType) {
  InitializeWithDefaultConfig("1.0.0.0");
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});

  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_with_hints());
  base::RunLoop run_loop;
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               run_loop.QuitClosure());
  run_loop.Run();

  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_handle.get(), optimization_guide::proto::DEFER_ALL_SCRIPT,
          /*optimization_metadata=*/nullptr);

  // Make sure decisions are logged correctly.
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kNotAllowedByHint,
            optimization_type_decision);
  // Make sure navigation data is populated correctly.
  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  ASSERT_TRUE(navigation_data->page_hint());
}

TEST_F(OptimizationGuideHintsManagerTest,
       CanApplyOptimizationUsesCachedPageHintFromNavigationData) {
  InitializeWithDefaultConfig("1.0.0.0");
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});

  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_with_hints());
  base::RunLoop run_loop;
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               run_loop.QuitClosure());
  run_loop.Run();

  // Purposely set the page hint to be null to show that we override the page
  // hint information from the navigation handle.
  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  navigation_data->set_page_hint(nullptr);

  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_handle.get(), optimization_guide::proto::DEFER_ALL_SCRIPT,
          /*optimization_metadata=*/nullptr);

  // Make sure decisions are logged correctly.
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kNotAllowedByHint,
            optimization_type_decision);
  // Make sure navigation data is populated correctly.
  EXPECT_EQ(nullptr, navigation_data->page_hint());
}

TEST_F(OptimizationGuideHintsManagerTest,
       CanApplyOptimizationAndPopulatesPerformanceHintsMetadata) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::PERFORMANCE_HINTS});
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint = config.add_hints();
  hint->set_key("somedomain.org");
  hint->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  hint->set_version("someversion");
  optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("/news/");
  optimization_guide::proto::Optimization* opt =
      page_hint->add_whitelisted_optimizations();
  opt->set_optimization_type(optimization_guide::proto::PERFORMANCE_HINTS);
  optimization_guide::proto::PerformanceHint* performance_hint =
      opt->mutable_performance_hints_metadata()->add_performance_hints();
  performance_hint->set_wildcard_pattern("somedomain.org");
  performance_hint->set_performance_class(
      optimization_guide::proto::PERFORMANCE_SLOW);

  ProcessHints(config, "1.0.0.0");

  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_with_hints());
  base::RunLoop run_loop;
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               run_loop.QuitClosure());
  run_loop.Run();

  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_handle.get(), optimization_guide::proto::PERFORMANCE_HINTS,
          &optimization_metadata);
  // Make sure performance hints metadata is populated.
  EXPECT_TRUE(optimization_metadata.performance_hints_metadata().has_value());

  // Make sure decisions are logged correctly.
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kAllowedByHint,
            optimization_type_decision);
}

TEST_F(OptimizationGuideHintsManagerTest,
       CanApplyOptimizationAndPopulatesPublicImageMetadata) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint = config.add_hints();
  hint->set_key("somedomain.org");
  hint->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  hint->set_version("someversion");
  optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("/news/");
  optimization_guide::proto::Optimization* opt =
      page_hint->add_whitelisted_optimizations();
  opt->set_optimization_type(optimization_guide::proto::COMPRESS_PUBLIC_IMAGES);
  opt->mutable_public_image_metadata()->add_url("someimage");

  ProcessHints(config, "1.0.0.0");

  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_with_hints());
  base::RunLoop run_loop;
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               run_loop.QuitClosure());
  run_loop.Run();

  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_handle.get(),
          optimization_guide::proto::COMPRESS_PUBLIC_IMAGES,
          &optimization_metadata);
  // Make sure public images metadata is populated.
  EXPECT_TRUE(optimization_metadata.public_image_metadata().has_value());

  // Make sure decisions are logged correctly.
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kAllowedByHint,
            optimization_type_decision);
}

TEST_F(OptimizationGuideHintsManagerTest,
       CanApplyOptimizationAndPopulatesLoadingPredictorMetadata) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LOADING_PREDICTOR});
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint = config.add_hints();
  hint->set_key("somedomain.org");
  hint->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  hint->set_version("someversion");
  optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("/news/");
  optimization_guide::proto::Optimization* opt =
      page_hint->add_whitelisted_optimizations();
  opt->set_optimization_type(optimization_guide::proto::LOADING_PREDICTOR);
  opt->mutable_loading_predictor_metadata()->add_subresources()->set_url(
      "https://resource.com/");

  ProcessHints(config, "1.0.0.0");

  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_with_hints());
  base::RunLoop run_loop;
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               run_loop.QuitClosure());
  run_loop.Run();

  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_handle.get(), optimization_guide::proto::LOADING_PREDICTOR,
          &optimization_metadata);
  // Make sure loading predictor metadata is populated.
  EXPECT_TRUE(optimization_metadata.loading_predictor_metadata().has_value());

  // Make sure decisions are logged correctly.
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kAllowedByHint,
            optimization_type_decision);
}

TEST_F(OptimizationGuideHintsManagerTest, IsGoogleURL) {
  const struct {
    const char* url;
    bool expect_is_google_url;
  } tests[] = {
      {"https://www.google.com/"
       "search?q=cats&oq=cq&aqs=foo&ie=UTF-8",
       true},
      {"https://www.google.com/", true},
      {"https://www.google.com/:99", true},

      // Try localized search pages.
      {"https://www.google.co.in/"
       "search?q=cats&oq=cq&aqs=foo&ie=UTF-8",
       true},
      {"https://www.google.co.in/", true},
      {"https://www.google.co.in/:99", true},

      // Try Google domain pages that are not web search related.
      {"https://www.not-google.com/", false},
      {"https://www.youtube.com/", false},
      {"https://domain.google.com/", false},
      {"https://images.google.com/", false},
  };

  for (const auto& test : tests) {
    GURL url(test.url);
    EXPECT_TRUE(url.is_valid());
    EXPECT_EQ(test.expect_is_google_url, hints_manager()->IsGoogleURL(url));
  }
}

TEST_F(OptimizationGuideHintsManagerTest,
       CanApplyOptimizationNoMatchingPageHint) {
  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so hint is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://somedomain.org/nomatch"));
  base::RunLoop run_loop;
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               run_loop.QuitClosure());
  run_loop.Run();

  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::NOSCRIPT});
  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(navigation_handle.get(),
                                            optimization_guide::proto::NOSCRIPT,
                                            /*optimization_metadata=*/nullptr);

  // Make sure decisions are logged correctly.
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kNotAllowedByHint,
            optimization_type_decision);
  // Make sure navigation data is populated correctly.
  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  EXPECT_EQ(nullptr, navigation_data->page_hint());
}

TEST_F(OptimizationGuideHintsManagerTest,
       CanApplyOptimizationNoHintForNavigationMetadataClearedAnyway) {
  InitializeWithDefaultConfig("1.0.0.0");

  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://nohint.com"));

  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::NOSCRIPT});
  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_metadata.set_previews_metadata(
      optimization_guide::proto::PreviewsMetadata());
  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(navigation_handle.get(),
                                            optimization_guide::proto::NOSCRIPT,
                                            &optimization_metadata);
  EXPECT_FALSE(optimization_metadata.previews_metadata().has_value());

  // Make sure decisions are logged correctly.
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kNoHintAvailable,
            optimization_type_decision);
  // Make sure navigation data is populated correctly.
  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  EXPECT_FALSE(navigation_data->has_page_hint_value());
}

TEST_F(OptimizationGuideHintsManagerTest,
       CanApplyOptimizationHasHintInCacheButNotLoaded) {
  InitializeWithDefaultConfig("1.0.0.0");

  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_with_hints());

  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::NOSCRIPT});
  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(navigation_handle.get(),
                                            optimization_guide::proto::NOSCRIPT,
                                            &optimization_metadata);

  // Make sure decisions are logged correctly.
  EXPECT_EQ(
      optimization_guide::OptimizationTypeDecision::kHadHintButNotLoadedInTime,
      optimization_type_decision);
  // Make sure navigation data is populated correctly.
  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  EXPECT_FALSE(navigation_data->has_page_hint_value());
}

TEST_F(OptimizationGuideHintsManagerTest,
       CanApplyOptimizationFilterTakesPrecedence) {
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://m.black.com/urlinfilterandhints"));

  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LITE_PAGE_REDIRECT});

  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("black.com");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  hint1->set_version("someversion");
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("https://m.black.com");
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(
      optimization_guide::proto::LITE_PAGE_REDIRECT);
  optimization_guide::BloomFilter blacklist_bloom_filter(
      kBlackBlacklistBloomFilterNumHashFunctions,
      kBlackBlacklistBloomFilterNumBits);
  PopulateBlackBlacklistBloomFilter(&blacklist_bloom_filter);
  AddBlacklistBloomFilterToConfig(optimization_guide::proto::LITE_PAGE_REDIRECT,
                                  blacklist_bloom_filter,
                                  kBlackBlacklistBloomFilterNumHashFunctions,
                                  kBlackBlacklistBloomFilterNumBits, &config);
  ProcessHints(config, "1.0.0.0");

  base::RunLoop run_loop;
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               run_loop.QuitClosure());
  run_loop.Run();

  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_handle.get(),
          optimization_guide::proto::LITE_PAGE_REDIRECT,
          /*optimization_metadata=*/nullptr);

  // Make sure decision points logged correctly.
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::
                kNotAllowedByOptimizationFilter,
            optimization_type_decision);
  // Make sure navigation data is populated correctly.
  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  EXPECT_FALSE(navigation_data->has_page_hint_value());
}

TEST_F(OptimizationGuideHintsManagerTest,
       CanApplyOptimizationFilterTakesPrecedenceMatchesFilter) {
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://notfiltered.com/whatever"));

  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LITE_PAGE_REDIRECT});

  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("notfiltered.com");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  hint1->set_version("someversion");
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("https://notfiltered.com");
  page_hint1->set_max_ect_trigger(
      optimization_guide::proto::EFFECTIVE_CONNECTION_TYPE_3G);
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(
      optimization_guide::proto::LITE_PAGE_REDIRECT);
  optimization_guide::BloomFilter blacklist_bloom_filter(
      kBlackBlacklistBloomFilterNumHashFunctions,
      kBlackBlacklistBloomFilterNumBits);
  PopulateBlackBlacklistBloomFilter(&blacklist_bloom_filter);
  AddBlacklistBloomFilterToConfig(optimization_guide::proto::LITE_PAGE_REDIRECT,
                                  blacklist_bloom_filter,
                                  kBlackBlacklistBloomFilterNumHashFunctions,
                                  kBlackBlacklistBloomFilterNumBits, &config);
  ProcessHints(config, "1.0.0.0");

  base::RunLoop run_loop;
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               run_loop.QuitClosure());
  run_loop.Run();

  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_handle.get(),
          optimization_guide::proto::LITE_PAGE_REDIRECT,
          /*optimization_metadata=*/nullptr);

  // Make sure decisions are logged correctly.
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::
                kAllowedByOptimizationFilter,
            optimization_type_decision);
  // Make sure navigation data is populated correctly.
  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  EXPECT_FALSE(navigation_data->has_page_hint_value());
}

class OptimizationGuideHintsManagerExperimentTest
    : public OptimizationGuideHintsManagerTest {
 public:
  OptimizationGuideHintsManagerExperimentTest() {
    scoped_list_.InitAndEnableFeatureWithParameters(
        optimization_guide::features::kOptimizationHintsExperiments,
        {{"experiment_name", "experiment"}});
  }

 private:
  base::test::ScopedFeatureList scoped_list_;
};

TEST_F(OptimizationGuideHintsManagerExperimentTest,
       CanApplyOptimizationAndPopulatesMetadataWithFirstOptThatMatchesWithExp) {
  InitializeWithDefaultConfig("1.0.0.0");

  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_with_hints());
  base::RunLoop run_loop;
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               run_loop.QuitClosure());
  run_loop.Run();

  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::NOSCRIPT});
  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(navigation_handle.get(),
                                            optimization_guide::proto::NOSCRIPT,
                                            &optimization_metadata);
  EXPECT_EQ(
      12345,
      optimization_metadata.previews_metadata().value().inflation_percent());

  // Make sure decisions are logged correctly.
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kAllowedByHint,
            optimization_type_decision);
  // Make sure navigation data is populated correctly.
  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  ASSERT_TRUE(navigation_data->page_hint());
}

class OptimizationGuideHintsManagerFetchingDisabledTest
    : public OptimizationGuideHintsManagerTest {
 public:
  OptimizationGuideHintsManagerFetchingDisabledTest() {
    scoped_list_.InitAndDisableFeature(
        optimization_guide::features::kRemoteOptimizationGuideFetching);
  }

 private:
  base::test::ScopedFeatureList scoped_list_;
};

TEST_F(OptimizationGuideHintsManagerFetchingDisabledTest,
       HintsFetchNotAllowedIfFeatureIsNotEnabled) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);

  std::unique_ptr<FakeTopHostProvider> top_host_provider =
      std::make_unique<FakeTopHostProvider>(
          std::vector<std::string>({"example1.com", "example2.com"}));

  CreateServiceAndHintsManager(/*optimization_types_at_initialization=*/{},
                               top_host_provider.get());
  InitializeWithDefaultConfig("1.0.0");

  // Force timer to expire and schedule a hints fetch.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  EXPECT_EQ(0, top_host_provider->get_num_top_hosts_called());
  // Hints fetcher should not even be created.
  EXPECT_FALSE(batch_update_hints_fetcher());
}

TEST_F(OptimizationGuideHintsManagerTest,
       CanApplyOptimizationAsyncReturnsRightAwayIfNotAllowedToFetch) {
  base::HistogramTester histogram_tester;

  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});
  InitializeWithDefaultConfig("1.0.0.0");

  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_without_hints());
  hints_manager()->CanApplyOptimizationAsync(
      url_without_hints(), optimization_guide::proto::COMPRESS_PUBLIC_IMAGES,
      base::BindOnce(
          [](optimization_guide::OptimizationGuideDecision decision,
             const optimization_guide::OptimizationMetadata& metadata) {
            EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kFalse,
                      decision);
          }));
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecisionAsync.CompressPublicImages",
      optimization_guide::OptimizationTypeDecision::kNoHintAvailable, 1);
}

TEST_F(
    OptimizationGuideHintsManagerTest,
    CanApplyOptimizationAsyncReturnsRightAwayIfNotAllowedToFetchAndNotWhitelistedByAvailableHint) {
  base::HistogramTester histogram_tester;

  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});
  InitializeWithDefaultConfig("1.0.0.0");

  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_with_hints());
  // Wait for hint to be loaded.
  base::RunLoop run_loop;
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               run_loop.QuitClosure());
  run_loop.Run();

  hints_manager()->CanApplyOptimizationAsync(
      url_with_hints(), optimization_guide::proto::COMPRESS_PUBLIC_IMAGES,
      base::BindOnce(
          [](optimization_guide::OptimizationGuideDecision decision,
             const optimization_guide::OptimizationMetadata& metadata) {
            EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kFalse,
                      decision);
          }));
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecisionAsync.CompressPublicImages",
      optimization_guide::OptimizationTypeDecision::kNotAllowedByHint, 1);
}

class OptimizationGuideHintsManagerFetchingTest
    : public OptimizationGuideHintsManagerTest {
 public:
  OptimizationGuideHintsManagerFetchingTest() {
    scoped_list_.InitAndEnableFeatureWithParameters(
        optimization_guide::features::kRemoteOptimizationGuideFetching,
        {{"max_concurrent_page_navigation_fetches", "2"},
         {"approved_external_app_packages",
          "org.example.whatever,com.foo.bar"}});
  }
 private:
  base::test::ScopedFeatureList scoped_list_;
};

TEST_F(OptimizationGuideHintsManagerFetchingTest,
       HintsFetchNotAllowedIfFeatureIsEnabledButUserNotAllowed) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  CreateServiceAndHintsManager({optimization_guide::proto::DEFER_ALL_SCRIPT},
                               /*top_host_provider=*/nullptr);
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithHostHints}));
  InitializeWithDefaultConfig("1.0.0");

  // Force timer to expire and schedule a hints fetch.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  // Hints fetcher should not even be created.
  EXPECT_FALSE(batch_update_hints_fetcher());
}

TEST_F(OptimizationGuideHintsManagerFetchingTest,
       HintsFetchNotAllowedIfFeatureIsEnabledButTopHostProviderIsNotProvided) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  CreateServiceAndHintsManager({optimization_guide::proto::DEFER_ALL_SCRIPT},
                               /*top_host_provider=*/nullptr);
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithHostHints}));
  InitializeWithDefaultConfig("1.0.0");

  // Force timer to expire and schedule a hints fetch.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  // Hints fetcher should not even be created.
  EXPECT_FALSE(batch_update_hints_fetcher());
}

TEST_F(OptimizationGuideHintsManagerFetchingTest,
       NoRegisteredOptimizationTypesAndHintsFetchNotAttempted) {
  std::unique_ptr<FakeTopHostProvider> top_host_provider =
      std::make_unique<FakeTopHostProvider>(
          std::vector<std::string>({"example1.com", "example2.com"}));

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  CreateServiceAndHintsManager(/*optimization_types_at_initialization=*/{},
                               top_host_provider.get());

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithHostHints}));
  InitializeWithDefaultConfig("1.0.0");

  // Force timer to expire and schedule a hints fetch but the fetch is not made.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  EXPECT_EQ(0, top_host_provider->get_num_top_hosts_called());
  // Hints fetcher should not be created.
  EXPECT_FALSE(batch_update_hints_fetcher());
}

TEST_F(OptimizationGuideHintsManagerFetchingTest,
       HintsFetcherEnabledNoHostsToFetch) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  std::unique_ptr<FakeTopHostProvider> top_host_provider =
      std::make_unique<FakeTopHostProvider>(std::vector<std::string>({}));
  CreateServiceAndHintsManager(/*optimization_types_at_initialization=*/{},
                               top_host_provider.get());

  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithHostHints}));
  InitializeWithDefaultConfig("1.0.0");

  // Force timer to expire and schedule a hints fetch.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  EXPECT_EQ(1, top_host_provider->get_num_top_hosts_called());
  // Hints fetcher should not be even created.
  EXPECT_FALSE(batch_update_hints_fetcher());
}

TEST_F(OptimizationGuideHintsManagerFetchingTest,
       HintsFetcherEnabledWithHostsNoHintsInResponse) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  std::unique_ptr<FakeTopHostProvider> top_host_provider =
      std::make_unique<FakeTopHostProvider>(
          std::vector<std::string>({"example1.com", "example2.com"}));
  CreateServiceAndHintsManager(/*optimization_types_at_initialization=*/{},
                               top_host_provider.get());
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithNoHints}));
  InitializeWithDefaultConfig("1.0.0");

  // Force timer to expire and schedule a hints fetch.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  EXPECT_EQ(1, top_host_provider->get_num_top_hosts_called());
  EXPECT_EQ(1, batch_update_hints_fetcher()->num_fetches_requested());

  // Check that hints should not be fetched again after the delay for a failed
  // hints fetch attempt.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  // This should be called exactly once, confirming that hints are not fetched
  // again after |kTestFetchRetryDelaySecs|.
  EXPECT_EQ(1, top_host_provider->get_num_top_hosts_called());
  EXPECT_EQ(1, batch_update_hints_fetcher()->num_fetches_requested());
}

TEST_F(OptimizationGuideHintsManagerFetchingTest, HintsFetcherTimerRetryDelay) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  std::unique_ptr<FakeTopHostProvider> top_host_provider =
      std::make_unique<FakeTopHostProvider>(
          std::vector<std::string>({"example1.com", "example2.com"}));

  CreateServiceAndHintsManager({optimization_guide::proto::DEFER_ALL_SCRIPT},
                               top_host_provider.get());
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchFailed,
           HintsFetcherEndState::kFetchSuccessWithHostHints}));
  InitializeWithDefaultConfig("1.0.0");

  // Force timer to expire and schedule a hints fetch - first time.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  EXPECT_EQ(1, top_host_provider->get_num_top_hosts_called());
  EXPECT_EQ(1, batch_update_hints_fetcher()->num_fetches_requested());

  // Force speculative timer to expire after fetch fails first time, update
  // hints fetcher so it succeeds this time.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  EXPECT_EQ(2, top_host_provider->get_num_top_hosts_called());
  EXPECT_EQ(2, batch_update_hints_fetcher()->num_fetches_requested());
}

TEST_F(OptimizationGuideHintsManagerFetchingTest,
       HintsFetcherTimerFetchSucceeds) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  std::unique_ptr<FakeTopHostProvider> top_host_provider =
      std::make_unique<FakeTopHostProvider>(
          std::vector<std::string>({"example1.com", "example2.com"}));

  // Force hints fetch scheduling.
  CreateServiceAndHintsManager(/*optimization_types_at_initialization=*/{},
                               top_host_provider.get());
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithHostHints}));
  InitializeWithDefaultConfig("1.0.0");

  // Force timer to expire and schedule a hints fetch that succeeds.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  EXPECT_EQ(1, batch_update_hints_fetcher()->num_fetches_requested());

  // TODO(mcrouse): Make sure timer is triggered by metadata entry,
  // |hint_cache| control needed.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  EXPECT_EQ(1, batch_update_hints_fetcher()->num_fetches_requested());

  MoveClockForwardBy(base::TimeDelta::FromSeconds(kUpdateFetchHintsTimeSecs));
  EXPECT_EQ(2, batch_update_hints_fetcher()->num_fetches_requested());
}

TEST_F(OptimizationGuideHintsManagerFetchingTest,
       HintsFetched_AtSRP_ECT_SLOW_2G) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so fetch is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  base::HistogramTester histogram_tester;
  std::vector<GURL> sorted_predicted_urls;
  sorted_predicted_urls.push_back(GURL("https://foo.com/"));
  NavigationPredictorKeyedService::Prediction prediction(
      nullptr, GURL("https://www.google.com/"),
      /*external_app_packages_name=*/{},
      NavigationPredictorKeyedService::PredictionSource::
          kAnchorElementsParsedFromWebPage,
      sorted_predicted_urls);

  hints_manager()->OnPredictionUpdated(prediction);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.UrlCount", 1, 1);
}

TEST_F(OptimizationGuideHintsManagerFetchingTest,
       HintsFetched_AtSRP_NoRegisteredOptimizationTypes) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so hint is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  base::HistogramTester histogram_tester;
  std::vector<GURL> sorted_predicted_urls;
  sorted_predicted_urls.push_back(GURL("https://foo.com/"));
  NavigationPredictorKeyedService::Prediction prediction(
      nullptr, GURL("https://www.google.com/"),
      /*external_app_packages_name=*/{},
      NavigationPredictorKeyedService::PredictionSource::
          kAnchorElementsParsedFromWebPage,
      sorted_predicted_urls);

  hints_manager()->OnPredictionUpdated(prediction);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.UrlCount", 0);
}

TEST_F(OptimizationGuideHintsManagerFetchingTest,
       HintsFetched_AtSRP_ECT_SLOW_2G_DuplicatesRemoved) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so fetch is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);

  std::vector<GURL> sorted_predicted_urls;
  sorted_predicted_urls.push_back(GURL("https://foo.com/page1.html"));
  sorted_predicted_urls.push_back(GURL("https://foo.com/page2.html"));
  sorted_predicted_urls.push_back(GURL("https://foo.com/page3.html"));
  sorted_predicted_urls.push_back(GURL("https://bar.com/"));

  NavigationPredictorKeyedService::Prediction prediction(
      nullptr, GURL("https://www.google.com/"),
      /*external_app_packages_name=*/{},
      NavigationPredictorKeyedService::PredictionSource::
          kAnchorElementsParsedFromWebPage,
      sorted_predicted_urls);

  {
    base::HistogramTester histogram_tester;

    hints_manager()->OnPredictionUpdated(prediction);
    // Ensure that we only include 2 hosts in the request. These would be
    // foo.com and bar.com.
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 2, 1);
    // Ensure that we include all URLs in the request.
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.HintsFetcher.GetHintsRequest.UrlCount", 4, 1);
    RunUntilIdle();
  }

  {
    base::HistogramTester histogram_tester;
    hints_manager()->OnPredictionUpdated(prediction);
    // Ensure that URLs are not re-fetched.
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.HintsFetcher.GetHintsRequest.UrlCount", 0);
  }
}

TEST_F(OptimizationGuideHintsManagerFetchingTest,
       HintsFetched_AtSRP_ECT_SLOW_2G_NonHTTPOrHTTPSHostsRemoved) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so fetch is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  base::HistogramTester histogram_tester;
  std::vector<GURL> sorted_predicted_urls;
  sorted_predicted_urls.push_back(GURL("https://foo.com/page1.html"));
  sorted_predicted_urls.push_back(GURL("file://non-web-bar.com/"));
  sorted_predicted_urls.push_back(GURL("http://httppage.com/"));

  NavigationPredictorKeyedService::Prediction prediction(
      nullptr, GURL("https://www.google.com/"),
      /*external_app_packages_name=*/{},
      NavigationPredictorKeyedService::PredictionSource::
          kAnchorElementsParsedFromWebPage,
      sorted_predicted_urls);

  hints_manager()->OnPredictionUpdated(prediction);
  // Ensure that we include both web hosts in the request. These would be
  // foo.com and httppage.com.
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 2, 1);
  // Ensure that we only include 2 URLs in the request.
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.UrlCount", 2, 1);
}

// Verify that optimization hints are not fetched if the prediction for the next
// likely navigations are provided by external Android app that is whitelisted.
TEST_F(
    OptimizationGuideHintsManagerFetchingTest,
    HintsFetched_ExternalAndroidApp_ECT_SLOW_2G_NonHTTPOrHTTPSHostsRemovedAppWhitelisted) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so fetch is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  base::HistogramTester histogram_tester;
  std::vector<GURL> sorted_predicted_urls;
  sorted_predicted_urls.push_back(GURL("https://foo.com/page1.html"));
  sorted_predicted_urls.push_back(GURL("file://non-web-bar.com/"));
  sorted_predicted_urls.push_back(GURL("http://httppage.com/"));

  std::vector<std::string> external_app_packages_name;
  external_app_packages_name.push_back("com.foo.bar");

  NavigationPredictorKeyedService::Prediction prediction_external_android_app(
      nullptr, base::nullopt, external_app_packages_name,
      NavigationPredictorKeyedService::PredictionSource::kExternalAndroidApp,
      sorted_predicted_urls);
  hints_manager()->OnPredictionUpdated(prediction_external_android_app);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.UrlCount", 2, 1);
}

// Verify that optimization hints are not fetched if the prediction for the next
// likely navigations are provided by external Android app that is not
// whitelisted, even though one of the apps was whitelisted.
TEST_F(
    OptimizationGuideHintsManagerFetchingTest,
    HintsFetched_ExternalAndroidApp_ECT_SLOW_2G_NonHTTPOrHTTPSHostsRemovedNotAllAppsWhitelisted) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so fetch is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  base::HistogramTester histogram_tester;
  std::vector<GURL> sorted_predicted_urls;
  sorted_predicted_urls.push_back(GURL("https://foo.com/page1.html"));
  sorted_predicted_urls.push_back(GURL("file://non-web-bar.com/"));
  sorted_predicted_urls.push_back(GURL("http://httppage.com/"));

  std::vector<std::string> external_app_packages_name;
  external_app_packages_name.push_back("com.foo.bar");
  external_app_packages_name.push_back("com.example.notwhitelisted");

  NavigationPredictorKeyedService::Prediction prediction_external_android_app(
      nullptr, base::nullopt, external_app_packages_name,
      NavigationPredictorKeyedService::PredictionSource::kExternalAndroidApp,
      sorted_predicted_urls);
  hints_manager()->OnPredictionUpdated(prediction_external_android_app);
  // Nothing should be fetched.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.UrlCount", 0);
}

// Verify that optimization hints are not fetched if the prediction for the next
// likely navigations are provided by external Android app that is not
// whitelisted.
TEST_F(
    OptimizationGuideHintsManagerFetchingTest,
    HintsFetched_ExternalAndroidApp_ECT_SLOW_2G_NonHTTPOrHTTPSHostsRemovedAppNotWhitelisted) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so fetch is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  base::HistogramTester histogram_tester;
  std::vector<GURL> sorted_predicted_urls;
  sorted_predicted_urls.push_back(GURL("https://foo.com/page1.html"));
  sorted_predicted_urls.push_back(GURL("file://non-web-bar.com/"));
  sorted_predicted_urls.push_back(GURL("http://httppage.com/"));

  std::vector<std::string> external_app_packages_name;
  external_app_packages_name.push_back("com.example.notwhitelisted");

  NavigationPredictorKeyedService::Prediction prediction_external_android_app(
      nullptr, base::nullopt, external_app_packages_name,
      NavigationPredictorKeyedService::PredictionSource::kExternalAndroidApp,
      sorted_predicted_urls);
  hints_manager()->OnPredictionUpdated(prediction_external_android_app);
  // Nothing should be fetched.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.UrlCount", 0);
}

TEST_F(OptimizationGuideHintsManagerFetchingTest, HintsFetched_AtSRP_ECT_4G) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so fetch is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_4G);
  base::HistogramTester histogram_tester;
  std::vector<GURL> sorted_predicted_urls;
  sorted_predicted_urls.push_back(GURL("https://foo.com/"));
  NavigationPredictorKeyedService::Prediction prediction(
      nullptr, GURL("https://www.google.com/"),
      /*external_app_packages_name=*/{},
      NavigationPredictorKeyedService::PredictionSource::
          kAnchorElementsParsedFromWebPage,
      sorted_predicted_urls);

  hints_manager()->OnPredictionUpdated(prediction);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.UrlCount", 1);
}

TEST_F(OptimizationGuideHintsManagerFetchingTest,
       HintsFetched_AtNonSRP_ECT_SLOW_2G) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so fetch is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  base::HistogramTester histogram_tester;
  std::vector<GURL> sorted_predicted_urls;
  sorted_predicted_urls.push_back(GURL("https://foo.com/"));
  NavigationPredictorKeyedService::Prediction prediction(
      nullptr, GURL("https://www.not-google.com/"),
      /*external_app_packages_name=*/{},
      NavigationPredictorKeyedService::PredictionSource::
          kAnchorElementsParsedFromWebPage,
      sorted_predicted_urls);

  hints_manager()->OnPredictionUpdated(prediction);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.UrlCount", 0);
}

TEST_F(OptimizationGuideHintsManagerFetchingTest,
       HintsFetchedAtNavigationTime_ECT_SLOW_2G) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so fetch is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_without_hints());
  base::HistogramTester histogram_tester;
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               base::DoNothing());
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.UrlCount", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
      optimization_guide::RaceNavigationFetchAttemptStatus::
          kRaceNavigationFetchHostAndURL,
      1);
}

TEST_F(OptimizationGuideHintsManagerFetchingTest,
       HintsFetchedAtNavigationTime_HasComponentHintButNotFetched) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  InitializeWithDefaultConfig("1.0.0.0");
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));

  // Set ECT estimate so fetch is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_with_hints());
  base::HistogramTester histogram_tester;
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               base::DoNothing());
  RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 0);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
      optimization_guide::RaceNavigationFetchAttemptStatus::
          kRaceNavigationFetchURL,
      1);
}

TEST_F(OptimizationGuideHintsManagerFetchingTest,
       URLHintsNotFetchedAtNavigationTime) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  InitializeWithDefaultConfig("1.0.0.0");
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));

  // Set ECT estimate so fetch is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);

  {
    base::HistogramTester histogram_tester;
    std::unique_ptr<content::MockNavigationHandle> navigation_handle =
        CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
            url_with_hints());

    hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                                 base::DoNothing());
    RunUntilIdle();

    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 0);

    // Make sure navigation data is populated correctly.
    OptimizationGuideNavigationData* navigation_data =
        OptimizationGuideNavigationData::GetFromNavigationHandle(
            navigation_handle.get());
    EXPECT_TRUE(navigation_data->hints_fetch_latency().has_value());
    EXPECT_EQ(navigation_data->hints_fetch_attempt_status(),
              optimization_guide::RaceNavigationFetchAttemptStatus::
                  kRaceNavigationFetchURL);

    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
        optimization_guide::RaceNavigationFetchAttemptStatus::
            kRaceNavigationFetchURL,
        1);
    RunUntilIdle();
  }

  {
    base::HistogramTester histogram_tester;
    std::unique_ptr<content::MockNavigationHandle> navigation_handle =
        CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
            url_with_hints());
    hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                                 base::DoNothing());
    RunUntilIdle();

    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
        optimization_guide::RaceNavigationFetchAttemptStatus::
            kRaceNavigationFetchNotAttempted,
        1);

    // Make sure navigation data is populated correctly.
    OptimizationGuideNavigationData* navigation_data =
        OptimizationGuideNavigationData::GetFromNavigationHandle(
            navigation_handle.get());
    EXPECT_FALSE(navigation_data->hints_fetch_latency().has_value());
    EXPECT_EQ(navigation_data->hints_fetch_attempt_status(),
              optimization_guide::RaceNavigationFetchAttemptStatus::
                  kRaceNavigationFetchNotAttempted);
  }
}

TEST_F(OptimizationGuideHintsManagerFetchingTest,
       URLWithNoHintsNotRefetchedAtNavigationTime) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  InitializeWithDefaultConfig("1.0.0.0");
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithHostHints}));

  // Set ECT estimate so fetch is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);

  base::HistogramTester histogram_tester;
  {
    std::unique_ptr<content::MockNavigationHandle> navigation_handle =
        CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
            url_without_hints());

    hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                                 base::DoNothing());
    RunUntilIdle();
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 0);

    // Make sure navigation data is populated correctly.
    OptimizationGuideNavigationData* navigation_data =
        OptimizationGuideNavigationData::GetFromNavigationHandle(
            navigation_handle.get());
    EXPECT_TRUE(navigation_data->hints_fetch_latency().has_value());
    EXPECT_EQ(navigation_data->hints_fetch_attempt_status(),
              optimization_guide::RaceNavigationFetchAttemptStatus::
                  kRaceNavigationFetchHostAndURL);

    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
        optimization_guide::RaceNavigationFetchAttemptStatus::
            kRaceNavigationFetchHostAndURL,
        1);
    RunUntilIdle();
  }

  {
    std::unique_ptr<content::MockNavigationHandle> navigation_handle =
        CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
            url_without_hints());
    base::RunLoop run_loop;
    navigation_handle =
        CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
            url_without_hints());
    hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                                 base::DoNothing());
    RunUntilIdle();

    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
        optimization_guide::RaceNavigationFetchAttemptStatus::
            kRaceNavigationFetchHost,
        1);
    OptimizationGuideNavigationData* navigation_data =
        OptimizationGuideNavigationData::GetFromNavigationHandle(
            navigation_handle.get());
    EXPECT_TRUE(navigation_data->hints_fetch_latency().has_value());
    EXPECT_EQ(navigation_data->hints_fetch_attempt_status(),
              optimization_guide::RaceNavigationFetchAttemptStatus::
                  kRaceNavigationFetchHost);
  }
}

TEST_F(OptimizationGuideHintsManagerFetchingTest,
       HintsNotFetchedAtNavigationTime_ECT_UNKNOWN) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so fetch is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_without_hints());
  base::HistogramTester histogram_tester;
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               base::DoNothing());
  RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 0);
  // Make sure navigation data is populated correctly.
  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  EXPECT_FALSE(navigation_data->hints_fetch_latency().has_value());
  EXPECT_FALSE(navigation_data->hints_fetch_attempt_status().has_value());

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus", 0);
}

TEST_F(OptimizationGuideHintsManagerFetchingTest,
       CanApplyOptimizationCalledMidFetch) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so hint will attempt to be fetched.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_2G);
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_without_hints());
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               base::DoNothing());
  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_handle.get(), optimization_guide::proto::DEFER_ALL_SCRIPT,
          /*optimization_metadata=*/nullptr);

  EXPECT_EQ(optimization_type_decision,
            optimization_guide::OptimizationTypeDecision::
                kHintFetchStartedButNotAvailableInTime);
}

TEST_F(OptimizationGuideHintsManagerFetchingTest,
       CanApplyOptimizationCalledPostFetchButNoHintsCameBack) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithNoHints}));

  // Set ECT estimate so fetch is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_without_hints());
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               base::DoNothing());
  RunUntilIdle();

  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_handle.get(), optimization_guide::proto::DEFER_ALL_SCRIPT,
          /*optimization_metadata=*/nullptr);

  EXPECT_EQ(optimization_type_decision,
            optimization_guide::OptimizationTypeDecision::kNoHintAvailable);
}

TEST_F(OptimizationGuideHintsManagerFetchingTest,
       CanApplyOptimizationCalledPostFetchButFetchFailed) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory({HintsFetcherEndState::kFetchFailed}));

  // Set ECT estimate so fetch is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_without_hints());
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               base::DoNothing());
  RunUntilIdle();

  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_handle.get(), optimization_guide::proto::DEFER_ALL_SCRIPT,
          /*optimization_metadata=*/nullptr);

  EXPECT_EQ(optimization_type_decision,
            optimization_guide::OptimizationTypeDecision::kNoHintAvailable);
}

TEST_F(OptimizationGuideHintsManagerFetchingTest,
       CanApplyOptimizationWithURLKeyedHintApplicableForOptimizationType) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});
  InitializeWithDefaultConfig("1.0.0");

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));

  // Set ECT estimate so fetch is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_with_url_keyed_hint());
  // Make sure URL-keyed hint is fetched and processed.
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               base::DoNothing());
  RunUntilIdle();

  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_handle.get(),
          optimization_guide::proto::COMPRESS_PUBLIC_IMAGES,
          &optimization_metadata);

  // Make sure decisions are logged correctly and metadata is populated off
  // a URL-keyed hint.
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kAllowedByHint,
            optimization_type_decision);
  EXPECT_TRUE(optimization_metadata.public_image_metadata().has_value());
}

TEST_F(OptimizationGuideHintsManagerFetchingTest,
       CanApplyOptimizationNotAllowedByURLButAllowedByHostKeyedHint) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::NOSCRIPT});

  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so fetch is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);

  // Make sure both URL-Keyed and host-keyed hints are processed and cached.
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_with_url_keyed_hint());
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               base::DoNothing());
  RunUntilIdle();

  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(navigation_handle.get(),
                                            optimization_guide::proto::NOSCRIPT,
                                            &optimization_metadata);

  // Make sure decisions are logged correctly.
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kAllowedByHint,
            optimization_type_decision);
}

TEST_F(OptimizationGuideHintsManagerFetchingTest,
       CanApplyOptimizationNotAllowedByURLOrHostKeyedHint) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::RESOURCE_LOADING});

  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so fetch is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);

  // Make sure both URL-Keyed and host-keyed hints are processed and cached.
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_with_url_keyed_hint());
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               base::DoNothing());
  RunUntilIdle();

  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_handle.get(), optimization_guide::proto::RESOURCE_LOADING,
          &optimization_metadata);

  // Make sure decisions are logged correctly.
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kNotAllowedByHint,
            optimization_type_decision);
}

TEST_F(OptimizationGuideHintsManagerFetchingTest,
       CanApplyOptimizationNoURLKeyedHintOrHostKeyedHint) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});

  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so fetch is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithNoHints}));
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_without_hints());

  // Attempt to fetch a hint but ensure nothing comes back.
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               base::DoNothing());
  RunUntilIdle();

  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_handle.get(),
          optimization_guide::proto::COMPRESS_PUBLIC_IMAGES,
          &optimization_metadata);

  // Make sure decisions are logged correctly.
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kNoHintAvailable,
            optimization_type_decision);
}

TEST_F(OptimizationGuideHintsManagerFetchingTest,
       CanApplyOptimizationCalledMidFetchForURLKeyedOptimization) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});

  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so fetch is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);

  // Attempt to fetch a hint but call CanApplyOptimization right away to
  // simulate being mid-fetch.
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_without_hints());
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithHostHints}));
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               base::DoNothing());
  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_handle.get(),
          optimization_guide::proto::COMPRESS_PUBLIC_IMAGES,
          &optimization_metadata);

  // Make sure decisions are logged correctly.
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::
                kHintFetchStartedButNotAvailableInTime,
            optimization_type_decision);
}

TEST_F(OptimizationGuideHintsManagerFetchingTest,
       OnNavigationStartOrRedirectWontInitiateFetchIfAlreadyStartedForTheURL) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::RESOURCE_LOADING});

  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so fetch is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);

  // Attempt to fetch a hint but initiate the next navigation right away to
  // simulate being mid-fetch.
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_without_hints());
  {
    base::HistogramTester histogram_tester;
    hints_manager()->SetHintsFetcherFactoryForTesting(
        BuildTestHintsFetcherFactory(
            {HintsFetcherEndState::kFetchSuccessWithHostHints}));
    hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                                 base::DoNothing());
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
        optimization_guide::RaceNavigationFetchAttemptStatus::
            kRaceNavigationFetchHostAndURL,
        1);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.HintsManager.ConcurrentPageNavigationFetches", 1, 1);
  }

  {
    base::HistogramTester histogram_tester;
    hints_manager()->SetHintsFetcherFactoryForTesting(
        BuildTestHintsFetcherFactory(
            {HintsFetcherEndState::kFetchSuccessWithHostHints}));
    hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                                 base::DoNothing());
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
        optimization_guide::RaceNavigationFetchAttemptStatus::
            kRaceNavigationFetchAlreadyInProgress,
        1);
    // Should not be recorded since we are not attempting a new fetch.
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.HintsManager.ConcurrentPageNavigationFetches", 0);
  }
}

TEST_F(OptimizationGuideHintsManagerFetchingTest,
       PageNavigationHintsFetcherGetsCleanedUpOnceHintsAreStored) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::RESOURCE_LOADING});

  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so fetch is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithNoHints}));

  // Attempt to fetch a hint but initiate the next navigation right away to
  // simulate being mid-fetch.
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_without_hints());
  {
    base::HistogramTester histogram_tester;
    hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                                 base::DoNothing());
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
        optimization_guide::RaceNavigationFetchAttemptStatus::
            kRaceNavigationFetchHostAndURL,
        1);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.HintsManager.ConcurrentPageNavigationFetches", 1, 1);

    // Make sure hints are stored (i.e. fetcher is cleaned up).
    RunUntilIdle();
  }

  {
    base::HistogramTester histogram_tester;
    hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                                 base::DoNothing());
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
        optimization_guide::RaceNavigationFetchAttemptStatus::
            kRaceNavigationFetchHost,
        1);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.HintsManager.ConcurrentPageNavigationFetches", 1, 1);
  }
}

TEST_F(OptimizationGuideHintsManagerFetchingTest,
       PageNavigationHintsFetcherCanFetchMultipleThingsConcurrently) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});

  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so fetch is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));

  std::unique_ptr<content::MockNavigationHandle> navigation_handle_with_hints =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_with_url_keyed_hint());
  std::unique_ptr<content::MockNavigationHandle>
      navigation_handle_without_hints =
          CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
              GURL("https://doesntmatter.com/"));
  std::unique_ptr<content::MockNavigationHandle>
      navigation_handle_without_hints2 =
          CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
              url_without_hints());

  // Attempt to fetch a hint but initiate the next navigations right away to
  // simulate being mid-fetch.
  base::HistogramTester histogram_tester;
  hints_manager()->OnNavigationStartOrRedirect(
      navigation_handle_with_hints.get(), base::DoNothing());
  hints_manager()->OnNavigationStartOrRedirect(
      navigation_handle_without_hints.get(), base::DoNothing());
  hints_manager()->OnNavigationStartOrRedirect(
      navigation_handle_without_hints2.get(), base::DoNothing());

  // The third one is over the max and should evict another one.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsManager.ConcurrentPageNavigationFetches", 3);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintsManager.ConcurrentPageNavigationFetches", 1, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintsManager.ConcurrentPageNavigationFetches", 2, 2);
}

TEST_F(OptimizationGuideHintsManagerFetchingTest,
       CanApplyOptimizationAsyncDecisionComesFromInFlightURLHint) {
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});
  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so fetch is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_with_url_keyed_hint());
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               base::DoNothing());
  hints_manager()->CanApplyOptimizationAsync(
      url_with_url_keyed_hint(),
      optimization_guide::proto::COMPRESS_PUBLIC_IMAGES,
      base::BindOnce(
          [](optimization_guide::OptimizationGuideDecision decision,
             const optimization_guide::OptimizationMetadata& metadata) {
            EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kTrue,
                      decision);
            EXPECT_TRUE(metadata.public_image_metadata().has_value());
          }));
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecisionAsync.CompressPublicImages",
      optimization_guide::OptimizationTypeDecision::kAllowedByHint, 1);
}

TEST_F(OptimizationGuideHintsManagerFetchingTest,
       CanApplyOptimizationAsyncMultipleCallbacksRegisteredForSameTypeAndURL) {
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});
  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so fetch is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_with_url_keyed_hint());
  hints_manager()->CanApplyOptimizationAsync(
      url_with_url_keyed_hint(),
      optimization_guide::proto::COMPRESS_PUBLIC_IMAGES,
      base::BindOnce(
          [](optimization_guide::OptimizationGuideDecision decision,
             const optimization_guide::OptimizationMetadata& metadata) {
            EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kTrue,
                      decision);
            EXPECT_TRUE(metadata.public_image_metadata().has_value());
          }));
  hints_manager()->CanApplyOptimizationAsync(
      url_with_url_keyed_hint(),
      optimization_guide::proto::COMPRESS_PUBLIC_IMAGES,
      base::BindOnce(
          [](optimization_guide::OptimizationGuideDecision decision,
             const optimization_guide::OptimizationMetadata& metadata) {
            EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kTrue,
                      decision);
            EXPECT_TRUE(metadata.public_image_metadata().has_value());
          }));
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               base::DoNothing());
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecisionAsync.CompressPublicImages",
      optimization_guide::OptimizationTypeDecision::kAllowedByHint, 2);
}

TEST_F(
    OptimizationGuideHintsManagerFetchingTest,
    CanApplyOptimizationAsyncDecisionComesFromInFlightURLHintNotWhitelisted) {
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::RESOURCE_LOADING});
  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so fetch is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_with_url_keyed_hint());
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               base::DoNothing());
  hints_manager()->CanApplyOptimizationAsync(
      url_with_url_keyed_hint(), optimization_guide::proto::RESOURCE_LOADING,
      base::BindOnce(
          [](optimization_guide::OptimizationGuideDecision decision,
             const optimization_guide::OptimizationMetadata& metadata) {
            EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kFalse,
                      decision);
          }));
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecisionAsync.ResourceLoading",
      optimization_guide::OptimizationTypeDecision::kNotAllowedByHint, 1);
}

TEST_F(OptimizationGuideHintsManagerFetchingTest,
       CanApplyOptimizationAsyncFetchFailsDoesNotStrandCallbacks) {
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});

  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so fetch is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory({HintsFetcherEndState::kFetchFailed}));
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_with_url_keyed_hint());
  hints_manager()->CanApplyOptimizationAsync(
      url_with_url_keyed_hint(),
      optimization_guide::proto::COMPRESS_PUBLIC_IMAGES,
      base::BindOnce(
          [](optimization_guide::OptimizationGuideDecision decision,
             const optimization_guide::OptimizationMetadata& metadata) {
            EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kFalse,
                      decision);
          }));
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               base::DoNothing());
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecisionAsync.CompressPublicImages",
      optimization_guide::OptimizationTypeDecision::kNotAllowedByHint, 1);
}

TEST_F(OptimizationGuideHintsManagerFetchingTest,
       CanApplyOptimizationAsyncInfoAlreadyInPriorToCall) {
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});

  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so fetch is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_with_url_keyed_hint());
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               base::DoNothing());
  RunUntilIdle();

  hints_manager()->CanApplyOptimizationAsync(
      url_with_url_keyed_hint(),
      optimization_guide::proto::COMPRESS_PUBLIC_IMAGES,
      base::BindOnce(
          [](optimization_guide::OptimizationGuideDecision decision,
             const optimization_guide::OptimizationMetadata& metadata) {
            EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kTrue,
                      decision);
            EXPECT_TRUE(metadata.public_image_metadata().has_value());
          }));
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecisionAsync.CompressPublicImages",
      optimization_guide::OptimizationTypeDecision::kAllowedByHint, 1);
}

TEST_F(OptimizationGuideHintsManagerFetchingTest,
       CanApplyOptimizationAsyncInfoAlreadyInPriorToCallAndNotWhitelisted) {
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::PERFORMANCE_HINTS});

  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so fetch is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_with_url_keyed_hint());
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               base::DoNothing());
  RunUntilIdle();

  hints_manager()->CanApplyOptimizationAsync(
      url_with_url_keyed_hint(), optimization_guide::proto::PERFORMANCE_HINTS,
      base::BindOnce(
          [](optimization_guide::OptimizationGuideDecision decision,
             const optimization_guide::OptimizationMetadata& metadata) {
            EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kFalse,
                      decision);
          }));
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecisionAsync.PerformanceHints",
      optimization_guide::OptimizationTypeDecision::kNotAllowedByHint, 1);
}

TEST_F(OptimizationGuideHintsManagerFetchingTest,
       CanApplyOptimizationAsyncHintComesInAndNotWhitelisted) {
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::PERFORMANCE_HINTS});

  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so fetch is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithNoHints}));
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_without_hints());
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               base::DoNothing());
  hints_manager()->CanApplyOptimizationAsync(
      url_without_hints(), optimization_guide::proto::PERFORMANCE_HINTS,
      base::BindOnce(
          [](optimization_guide::OptimizationGuideDecision decision,
             const optimization_guide::OptimizationMetadata& metadata) {
            EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kFalse,
                      decision);
          }));
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecisionAsync.PerformanceHints",
      optimization_guide::OptimizationTypeDecision::kNoHintAvailable, 1);
}

TEST_F(OptimizationGuideHintsManagerFetchingTest,
       CanApplyOptimizationAsyncDoesNotStrandCallbacksAtBeginningOfChain) {
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});

  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so fetch is NOT activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_4G);

  GURL url_that_redirected("https://urlthatredirected.com");
  std::unique_ptr<content::MockNavigationHandle> navigation_handle_redirect =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_that_redirected);
  hints_manager()->CanApplyOptimizationAsync(
      url_that_redirected, optimization_guide::proto::COMPRESS_PUBLIC_IMAGES,
      base::BindOnce(
          [](optimization_guide::OptimizationGuideDecision decision,
             const optimization_guide::OptimizationMetadata& metadata) {
            EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kFalse,
                      decision);
          }));
  hints_manager()->OnNavigationFinish(
      {url_that_redirected, GURL("https://otherurl.com/")});
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecisionAsync.CompressPublicImages",
      optimization_guide::OptimizationTypeDecision::kNoHintAvailable, 1);
}

TEST_F(OptimizationGuideHintsManagerFetchingTest,
       CanApplyOptimizationAsyncDoesNotStrandCallbacksIfFetchNotPending) {
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});

  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so fetch is NOT activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_4G);

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithNoHints}));
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_with_url_keyed_hint());
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               base::DoNothing());
  hints_manager()->CanApplyOptimizationAsync(
      url_with_url_keyed_hint(),
      optimization_guide::proto::COMPRESS_PUBLIC_IMAGES,
      base::BindOnce(
          [](optimization_guide::OptimizationGuideDecision decision,
             const optimization_guide::OptimizationMetadata& metadata) {
            EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kFalse,
                      decision);
          }));
  hints_manager()->OnNavigationFinish({url_with_url_keyed_hint()});
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecisionAsync.CompressPublicImages",
      optimization_guide::OptimizationTypeDecision::kNotAllowedByHint, 1);
}

TEST_F(
    OptimizationGuideHintsManagerFetchingTest,
    CanApplyOptimizationAsyncWithDecisionFromOptimizationFilterReturnsRightAway) {
  base::HistogramTester histogram_tester;

  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LITE_PAGE_REDIRECT});

  optimization_guide::proto::Configuration config;
  optimization_guide::BloomFilter blacklist_bloom_filter(
      kBlackBlacklistBloomFilterNumHashFunctions,
      kBlackBlacklistBloomFilterNumBits);
  PopulateBlackBlacklistBloomFilter(&blacklist_bloom_filter);
  AddBlacklistBloomFilterToConfig(optimization_guide::proto::LITE_PAGE_REDIRECT,
                                  blacklist_bloom_filter,
                                  kBlackBlacklistBloomFilterNumHashFunctions,
                                  kBlackBlacklistBloomFilterNumBits, &config);
  ProcessHints(config, "1.0.0.0");

  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://m.black.com/123"));
  hints_manager()->CanApplyOptimizationAsync(
      navigation_handle->GetURL(),
      optimization_guide::proto::LITE_PAGE_REDIRECT,
      base::BindOnce(
          [](optimization_guide::OptimizationGuideDecision decision,
             const optimization_guide::OptimizationMetadata& metadata) {
            EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kFalse,
                      decision);
          }));
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecisionAsync.LitePageRedirect",
      optimization_guide::OptimizationTypeDecision::
          kNotAllowedByOptimizationFilter,
      1);
}

TEST_F(OptimizationGuideHintsManagerFetchingTest,
       OnNavigationFinishDoesNotPrematurelyInvokeRegisteredCallbacks) {
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});

  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so fetch is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_with_url_keyed_hint());
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               base::DoNothing());
  hints_manager()->CanApplyOptimizationAsync(
      url_with_url_keyed_hint(),
      optimization_guide::proto::COMPRESS_PUBLIC_IMAGES,
      base::BindOnce(
          [](optimization_guide::OptimizationGuideDecision decision,
             const optimization_guide::OptimizationMetadata& metadata) {
            EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kTrue,
                      decision);
            EXPECT_TRUE(metadata.public_image_metadata().has_value());
          }));
  hints_manager()->OnNavigationFinish({url_with_url_keyed_hint()});
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecisionAsync.CompressPublicImages",
      optimization_guide::OptimizationTypeDecision::kAllowedByHint, 1);
}

TEST_F(OptimizationGuideHintsManagerFetchingTest,
       OnNavigationFinishDoesNotCrashWithoutAnyCallbacksRegistered) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});

  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->OnNavigationFinish({url_with_url_keyed_hint()});

  RunUntilIdle();
}

TEST_F(OptimizationGuideHintsManagerFetchingTest,
       NewOptTypeRegisteredClearsHintCache) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});

  InitializeWithDefaultConfig("1.0.0.0");

  GURL url("https://host.com/fetched_hint_host");

  // Set ECT estimate so fetch is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithHostHints}));
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(url);

  // Attempt to fetch a hint but ensure nothing comes back.
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               base::DoNothing());
  RunUntilIdle();

  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_handle.get(), optimization_guide::proto::DEFER_ALL_SCRIPT,
          &optimization_metadata);

  // Make sure decisions are logged correctly.
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kNotAllowedByHint,
            optimization_type_decision);

  // Register a new type that is unlaunched - this should clear the Fetched
  // hints.
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});

  RunUntilIdle();

  base::RunLoop run_loop;

  // Set ECT estimate to 4g so the fetch does not happen so
  // the cache state is known and empty.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_OFFLINE);

  navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(url);
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               run_loop.QuitClosure());

  run_loop.Run();

  optimization_type_decision = hints_manager()->CanApplyOptimization(
      navigation_handle.get(), optimization_guide::proto::DEFER_ALL_SCRIPT,
      &optimization_metadata);

  // The fetched hints should not be available after registering a new
  // optimization type.
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kNoHintAvailable,
            optimization_type_decision);
}

class OptimizationGuideHintsManagerFetchingNoBatchUpdateTest
    : public OptimizationGuideHintsManagerTest {
 public:
  OptimizationGuideHintsManagerFetchingNoBatchUpdateTest() {
    scoped_list_.InitAndEnableFeatureWithParameters(
        optimization_guide::features::kRemoteOptimizationGuideFetching,
        {{"batch_update_hints_for_top_hosts", "false"}});
  }

 private:
  base::test::ScopedFeatureList scoped_list_;
};

TEST_F(OptimizationGuideHintsManagerFetchingNoBatchUpdateTest,
       BatchUpdateHintsFetchNotScheduledIfNotAllowed) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  std::unique_ptr<FakeTopHostProvider> top_host_provider =
      std::make_unique<FakeTopHostProvider>(
          std::vector<std::string>({"example1.com", "example2.com"}));

  // Force hints fetch scheduling.
  CreateServiceAndHintsManager({optimization_guide::proto::DEFER_ALL_SCRIPT},
                               top_host_provider.get());
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithHostHints}));
  InitializeWithDefaultConfig("1.0.0");

  // Force timer to expire and schedule a hints fetch.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  // Hints fetcher should not even be created.
  EXPECT_FALSE(batch_update_hints_fetcher());
}
