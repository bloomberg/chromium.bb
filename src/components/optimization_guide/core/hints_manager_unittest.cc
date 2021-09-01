// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/hints_manager.h"

#include <string>
#include <utility>

#include "base/base64.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/optimization_guide/core/bloom_filter.h"
#include "components/optimization_guide/core/hint_cache.h"
#include "components/optimization_guide/core/hints_component_util.h"
#include "components/optimization_guide/core/hints_fetcher.h"
#include "components/optimization_guide/core/hints_fetcher_factory.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_navigation_data.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/optimization_guide_store.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/proto_database_provider_test_base.h"
#include "components/optimization_guide/core/tab_url_provider.h"
#include "components/optimization_guide/core/top_host_provider.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/unified_consent/unified_consent_service.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {
namespace {

// Allows for default hour to pass + random delay between 30 and 60 seconds.
constexpr int kUpdateFetchHintsTimeSecs = 61 * 60;  // 1 hours and 1 minutes.

const int kDefaultHostBloomFilterNumHashFunctions = 7;
const int kDefaultHostBloomFilterNumBits = 511;

void PopulateBloomFilterWithDefaultHost(
    optimization_guide::BloomFilter* bloom_filter) {
  bloom_filter->Add("host.com");
}

void AddBloomFilterToConfig(
    optimization_guide::proto::OptimizationType optimization_type,
    const optimization_guide::BloomFilter& bloom_filter,
    int num_hash_functions,
    int num_bits,
    bool is_allowlist,
    optimization_guide::proto::Configuration* config) {
  std::string bloom_filter_data(
      reinterpret_cast<const char*>(&bloom_filter.bytes()[0]),
      bloom_filter.bytes().size());
  optimization_guide::proto::OptimizationFilter* of_proto =
      is_allowlist ? config->add_optimization_allowlists()
                   : config->add_optimization_blocklists();
  of_proto->set_optimization_type(optimization_type);
  std::unique_ptr<optimization_guide::proto::BloomFilter> bloom_filter_proto =
      std::make_unique<optimization_guide::proto::BloomFilter>();
  bloom_filter_proto->set_num_hash_functions(num_hash_functions);
  bloom_filter_proto->set_num_bits(num_bits);
  bloom_filter_proto->set_data(bloom_filter_data);
  of_proto->set_allocated_bloom_filter(bloom_filter_proto.release());
}

std::unique_ptr<optimization_guide::proto::GetHintsResponse> BuildHintsResponse(
    const std::vector<std::string>& hosts,
    const std::vector<std::string>& urls) {
  std::unique_ptr<optimization_guide::proto::GetHintsResponse>
      get_hints_response =
          std::make_unique<optimization_guide::proto::GetHintsResponse>();

  for (const auto& host : hosts) {
    optimization_guide::proto::Hint* hint = get_hints_response->add_hints();
    hint->set_key_representation(optimization_guide::proto::HOST);
    hint->set_key(host);
    optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
    page_hint->set_page_pattern("page pattern");
    optimization_guide::proto::Optimization* opt =
        page_hint->add_allowlisted_optimizations();
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
        page_hint->add_allowlisted_optimizations();
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

// A mock class implementation of TabUrlProvider.
class FakeTabUrlProvider : public optimization_guide::TabUrlProvider {
 public:
  const std::vector<GURL> GetUrlsOfActiveTabs(
      const base::TimeDelta& duration_since_last_shown) override {
    num_urls_called_++;
    return urls_;
  }

  void SetUrls(const std::vector<GURL>& urls) { urls_ = urls; }

  int get_num_urls_called() const { return num_urls_called_; }

 private:
  std::vector<GURL> urls_;
  int num_urls_called_ = 0;
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
      network::NetworkConnectionTracker* network_connection_tracker,
      const std::vector<HintsFetcherEndState>& fetch_states)
      : HintsFetcher(url_loader_factory,
                     optimization_guide_service_url,
                     pref_service,
                     network_connection_tracker),
        fetch_states_(fetch_states) {
    DCHECK(!fetch_states_.empty());
  }

  bool FetchOptimizationGuideServiceHints(
      const std::vector<std::string>& hosts,
      const std::vector<GURL>& urls,
      const base::flat_set<optimization_guide::proto::OptimizationType>&
          optimization_types,
      optimization_guide::proto::RequestContext request_context,
      const std::string& locale,
      optimization_guide::HintsFetchedCallback hints_fetched_callback)
      override {
    HintsFetcherEndState fetch_state =
        num_fetches_requested_ < static_cast<int>(fetch_states_.size())
            ? fetch_states_[num_fetches_requested_]
            : fetch_states_.back();
    num_fetches_requested_++;
    locale_requested_ = locale;
    switch (fetch_state) {
      case HintsFetcherEndState::kFetchFailed:
        std::move(hints_fetched_callback).Run(absl::nullopt);
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

  int num_fetches_requested() const { return num_fetches_requested_; }

  std::string locale_requested() const { return locale_requested_; }

 private:
  std::vector<HintsFetcherEndState> fetch_states_;
  int num_fetches_requested_ = 0;
  std::string locale_requested_;
};

// A mock class of HintsFetcherFactory that returns instances of
// TestHintsFetchers with the provided fetch state.
class TestHintsFetcherFactory : public optimization_guide::HintsFetcherFactory {
 public:
  TestHintsFetcherFactory(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      GURL optimization_guide_service_url,
      PrefService* pref_service,
      const std::vector<HintsFetcherEndState>& fetch_states,
      network::NetworkConnectionTracker* network_connection_tracker)
      : HintsFetcherFactory(url_loader_factory,
                            optimization_guide_service_url,
                            pref_service,
                            network_connection_tracker),
        fetch_states_(fetch_states) {}

  std::unique_ptr<optimization_guide::HintsFetcher> BuildInstance() override {
    return std::make_unique<TestHintsFetcher>(
        url_loader_factory_, optimization_guide_service_url_, pref_service_,
        network_connection_tracker_, fetch_states_);
  }

 private:
  std::vector<HintsFetcherEndState> fetch_states_;
};

class HintsManagerTest
    : public optimization_guide::ProtoDatabaseProviderTestBase {
 public:
  HintsManagerTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        optimization_guide::features::kOptimizationHints,
        {{"max_host_keyed_hint_cache_size", "1"}});
  }
  ~HintsManagerTest() override = default;

  HintsManagerTest(const HintsManagerTest&) = delete;
  HintsManagerTest& operator=(const HintsManagerTest&) = delete;

  void SetUp() override {
    optimization_guide::ProtoDatabaseProviderTestBase::SetUp();
    CreateHintsManager(/*top_host_provider=*/nullptr);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        optimization_guide::switches::
            kDisableFetchHintsForActiveTabsOnDeferredStartup);
  }

  void TearDown() override {
    ResetHintsManager();
    optimization_guide::ProtoDatabaseProviderTestBase::TearDown();
  }

  void CreateHintsManager(
      optimization_guide::TopHostProvider* top_host_provider) {
    if (hints_manager_)
      ResetHintsManager();

    pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    optimization_guide::prefs::RegisterProfilePrefs(pref_service_->registry());
    pref_service_->registry()->RegisterBooleanPref(
        data_reduction_proxy::prefs::kDataSaverEnabled, false);
    unified_consent::UnifiedConsentService::RegisterPrefs(
        pref_service_->registry());

    url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    hint_store_ = std::make_unique<optimization_guide::OptimizationGuideStore>(
        db_provider_.get(), temp_dir(),
        task_environment_.GetMainThreadTaskRunner());

    tab_url_provider_ = std::make_unique<FakeTabUrlProvider>();

    hints_manager_ = std::make_unique<HintsManager>(
        /*is_off_the_record=*/false, /*application_locale=*/"en-US",
        pref_service(), hint_store_.get(), top_host_provider,
        tab_url_provider_.get(), url_loader_factory_,
        network::TestNetworkConnectionTracker::GetInstance(),
        /*push_notification_manager=*/nullptr);
    hints_manager_->SetClockForTesting(task_environment_.GetMockClock());

    // Run until hint cache is initialized and the HintsManager is ready to
    // process hints.
    RunUntilIdle();
  }

  void ResetHintsManager() {
    hints_manager_->Shutdown();
    hints_manager_.reset();
    tab_url_provider_.reset();
    hint_store_.reset();
    pref_service_.reset();
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

  void ProcessHintsComponentInfoWithBadConfig(const std::string& version) {
    optimization_guide::HintsComponentInfo info(
        base::Version(version),
        temp_dir().Append(FILE_PATH_LITERAL("badconfig.pb")));
    ASSERT_EQ(7, base::WriteFile(info.path, "garbage", 7));

    hints_manager_->OnHintsComponentAvailable(info);
    RunUntilIdle();
  }

  void ProcessHints(const optimization_guide::proto::Configuration& config,
                    const std::string& version,
                    bool should_wait = true) {
    optimization_guide::HintsComponentInfo info(
        base::Version(version),
        temp_dir().Append(FILE_PATH_LITERAL("somefile.pb")));
    ASSERT_NO_FATAL_FAILURE(WriteConfigToFile(config, info.path));

    base::RunLoop run_loop;
    if (should_wait)
      hints_manager_->ListenForNextUpdateForTesting(run_loop.QuitClosure());
    hints_manager_->OnHintsComponentAvailable(info);
    if (should_wait)
      run_loop.Run();
  }

  void InitializeWithDefaultConfig(const std::string& version,
                                   bool should_wait = true) {
    optimization_guide::proto::Configuration config;
    optimization_guide::proto::Hint* hint1 = config.add_hints();
    hint1->set_key("somedomain.org");
    hint1->set_key_representation(optimization_guide::proto::HOST);
    hint1->set_version("someversion");
    optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
    page_hint1->set_page_pattern("/news/");
    optimization_guide::proto::Optimization* default_opt =
        page_hint1->add_allowlisted_optimizations();
    default_opt->set_optimization_type(optimization_guide::proto::NOSCRIPT);
    // Add another hint so somedomain.org hint is not in-memory initially.
    optimization_guide::proto::Hint* hint2 = config.add_hints();
    hint2->set_key("somedomain2.org");
    hint2->set_key_representation(optimization_guide::proto::HOST);
    hint2->set_version("someversion");
    optimization_guide::proto::Optimization* opt =
        hint2->add_allowlisted_optimizations();
    opt->set_optimization_type(optimization_guide::proto::NOSCRIPT);

    ProcessHints(config, version, should_wait);
  }

  std::unique_ptr<optimization_guide::HintsFetcherFactory>
  BuildTestHintsFetcherFactory(
      const std::vector<HintsFetcherEndState>& fetch_states) {
    return std::make_unique<TestHintsFetcherFactory>(
        url_loader_factory_, GURL("https://hintsserver.com"), pref_service(),
        fetch_states, network::TestNetworkConnectionTracker::GetInstance());
  }

  void MoveClockForwardBy(base::TimeDelta time_delta) {
    task_environment_.FastForwardBy(time_delta);
    RunUntilIdle();
  }

  // Creates navigation data for a navigation to |url| with registered
  // |optimization_types|.
  std::unique_ptr<OptimizationGuideNavigationData> CreateTestNavigationData(
      const GURL& url,
      const std::vector<optimization_guide::proto::OptimizationType>&
          optimization_types) {
    auto navigation_data = std::make_unique<OptimizationGuideNavigationData>(
        /*navigation_id=*/1, /*navigation_start*/ base::TimeTicks::Now());
    navigation_data->set_navigation_url(url);
    navigation_data->set_registered_optimization_types(optimization_types);
    return navigation_data;
  }

  void CallOnNavigationStartOrRedirect(
      OptimizationGuideNavigationData* navigation_data,
      base::OnceClosure callback) {
    hints_manager()->OnNavigationStartOrRedirect(navigation_data,
                                                 std::move(callback));
  }

  void SetConnectionOffline() {
    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_NONE);
  }

  void SetConnectionOnline() {
    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_4G);
  }

  HintsManager* hints_manager() const { return hints_manager_.get(); }

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

  PrefService* pref_service() const { return pref_service_.get(); }

  FakeTabUrlProvider* tab_url_provider() const {
    return tab_url_provider_.get();
  }

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

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<optimization_guide::OptimizationGuideStore> hint_store_;
  std::unique_ptr<FakeTabUrlProvider> tab_url_provider_;
  std::unique_ptr<HintsManager> hints_manager_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

TEST_F(HintsManagerTest, ProcessHintsWithValidCommandLineOverride) {
  base::HistogramTester histogram_tester;

  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint = config.add_hints();
  hint->set_key("somedomain.org");
  hint->set_key_representation(optimization_guide::proto::HOST);
  optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("noscript_default_2g");
  optimization_guide::proto::Optimization* optimization =
      page_hint->add_allowlisted_optimizations();
  optimization->set_optimization_type(optimization_guide::proto::NOSCRIPT);
  optimization_guide::BloomFilter bloom_filter(
      kDefaultHostBloomFilterNumHashFunctions, kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&bloom_filter);
  AddBloomFilterToConfig(optimization_guide::proto::LITE_PAGE_REDIRECT,
                         bloom_filter, kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/false, &config);
  AddBloomFilterToConfig(optimization_guide::proto::PERFORMANCE_HINTS,
                         bloom_filter, kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/true, &config);

  std::string encoded_config;
  config.SerializeToString(&encoded_config);
  base::Base64Encode(encoded_config, &encoded_config);

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      optimization_guide::switches::kHintsProtoOverride, encoded_config);
  CreateHintsManager(/*top_host_provider=*/nullptr);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LITE_PAGE_REDIRECT});

  // The below histogram should not be recorded since hints weren't coming
  // directly from the component.
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ProcessHintsResult",
      optimization_guide::ProcessHintsComponentResult::kSuccess, 1);
  // However, we still expect the local histogram for the hints being updated to
  // be recorded.
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.UpdateComponentHints.Result", true, 1);

  // Bloom filters passed via command line are processed on the background
  // thread so make sure everything has finished before checking if it has been
  // loaded.
  RunUntilIdle();

  EXPECT_TRUE(hints_manager()->HasLoadedOptimizationBlocklist(
      optimization_guide::proto::LITE_PAGE_REDIRECT));
  EXPECT_FALSE(hints_manager()->HasLoadedOptimizationAllowlist(
      optimization_guide::proto::PERFORMANCE_HINTS));

  // Now register a new type with an allowlist that has not yet been loaded.
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::PERFORMANCE_HINTS});
  RunUntilIdle();

  EXPECT_TRUE(hints_manager()->HasLoadedOptimizationAllowlist(
      optimization_guide::proto::PERFORMANCE_HINTS));
}

TEST_F(HintsManagerTest, ProcessHintsWithInvalidCommandLineOverride) {
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      optimization_guide::switches::kHintsProtoOverride, "this-is-not-a-proto");
  CreateHintsManager(/*top_host_provider=*/nullptr);

  // The below histogram should not be recorded since hints weren't coming
  // directly from the component.
  histogram_tester.ExpectTotalCount("OptimizationGuide.ProcessHintsResult", 0);
  // We also do not expect to update the component hints with bad hints either.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.UpdateComponentHints.Result", 0);
}

TEST_F(HintsManagerTest,
       ProcessHintsWithCommandLineOverrideShouldNotBeOverriddenByNewComponent) {
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint = config.add_hints();
  hint->set_key("somedomain.org");
  hint->set_key_representation(optimization_guide::proto::HOST);
  optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("noscript_default_2g");
  optimization_guide::proto::Optimization* optimization =
      page_hint->add_allowlisted_optimizations();
  optimization->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  std::string encoded_config;
  config.SerializeToString(&encoded_config);
  base::Base64Encode(encoded_config, &encoded_config);

  {
    base::HistogramTester histogram_tester;
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        optimization_guide::switches::kHintsProtoOverride, encoded_config);
    CreateHintsManager(/*top_host_provider=*/nullptr);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        optimization_guide::ProcessHintsComponentResult::kSuccess, 1);
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

TEST_F(HintsManagerTest, ParseTwoConfigVersions) {
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("somedomain.org");
  hint1->set_key_representation(optimization_guide::proto::HOST);
  hint1->set_version("someversion");
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("/news/");
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_allowlisted_optimizations();
  optimization1->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);

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

TEST_F(HintsManagerTest, ParseInvalidConfigVersions) {
  // Test the first time parsing the config.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("1.0.0.0");
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        optimization_guide::ProcessHintsComponentResult::kSuccess, 1);
  }

  {
    base::HistogramTester histogram_tester;
    ProcessHintsComponentInfoWithBadConfig("2.0.0.0");
    // If we have already parsed a version later than this version, we expect
    // for the hints to not be updated.
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        optimization_guide::ProcessHintsComponentResult::
            kFailedInvalidConfiguration,
        1);
  }
}

TEST_F(HintsManagerTest, ComponentProcessingWhileShutdown) {
  base::HistogramTester histogram_tester;
  InitializeWithDefaultConfig("10.0.0.0", /*should_wait=*/false);
  hints_manager()->Shutdown();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ProcessingComponentAtShutdown", true, 1);

  EXPECT_TRUE(
      pref_service()
          ->GetString(optimization_guide::prefs::kPendingHintsProcessingVersion)
          .empty());
}

TEST_F(HintsManagerTest, ParseOlderConfigVersions) {
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

TEST_F(HintsManagerTest, ParseDuplicateConfigVersions) {
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

TEST_F(HintsManagerTest, ComponentInfoDidNotContainConfig) {
  base::HistogramTester histogram_tester;
  ProcessInvalidHintsComponentInfo("1.0.0.0");
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ProcessHintsResult",
      optimization_guide::ProcessHintsComponentResult::kFailedReadingFile, 1);
}

TEST_F(HintsManagerTest, ProcessHintsWithExistingPref) {
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

TEST_F(HintsManagerTest,
       ProcessHintsWithExistingPrefDoesNotClearOrCountAsMidProcessing) {
  // Write hints processing pref for version 2.0.0.
  pref_service()->SetString(
      optimization_guide::prefs::kPendingHintsProcessingVersion, "2.0.0");

  // Verify component for same version counts as "failed".
  base::HistogramTester histogram_tester;
  InitializeWithDefaultConfig("2.0.0", /*should_wait=*/false);
  hints_manager()->Shutdown();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ProcessHintsResult",
      optimization_guide::ProcessHintsComponentResult::kFailedFinishProcessing,
      1);

  // Verify that pref still not cleared at shutdown and was not counted as
  // mid-processing.
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ProcessingComponentAtShutdown", false, 1);
  EXPECT_FALSE(
      pref_service()
          ->GetString(optimization_guide::prefs::kPendingHintsProcessingVersion)
          .empty());
}

TEST_F(HintsManagerTest, ProcessHintsWithInvalidPref) {
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
            kFailedPreviouslyAttemptedVersionInvalid,
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

TEST_F(HintsManagerTest, OnNavigationStartOrRedirectWithHint) {
  base::HistogramTester histogram_tester;
  InitializeWithDefaultConfig("3.0.0.0");

  auto navigation_data = CreateTestNavigationData(url_with_hints(), {});

  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  histogram_tester.ExpectUniqueSample("OptimizationGuide.LoadedHint.Result",
                                      true, 1);
}

TEST_F(HintsManagerTest, OnNavigationStartOrRedirectNoHint) {
  base::HistogramTester histogram_tester;
  InitializeWithDefaultConfig("3.0.0.0");

  auto navigation_data =
      CreateTestNavigationData(GURL("https://notinhints.com"), {});

  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  histogram_tester.ExpectUniqueSample("OptimizationGuide.LoadedHint.Result",
                                      false, 1);
}

TEST_F(HintsManagerTest, OnNavigationStartOrRedirectNoHost) {
  base::HistogramTester histogram_tester;
  InitializeWithDefaultConfig("3.0.0.0");

  auto navigation_data = CreateTestNavigationData(GURL("blargh"), {});

  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  histogram_tester.ExpectTotalCount("OptimizationGuide.LoadedHint.Result", 0);
}

TEST_F(HintsManagerTest, OptimizationFiltersAreOnlyLoadedIfTypeIsRegistered) {
  optimization_guide::proto::Configuration config;
  optimization_guide::BloomFilter bloom_filter(
      kDefaultHostBloomFilterNumHashFunctions, kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&bloom_filter);
  AddBloomFilterToConfig(optimization_guide::proto::LITE_PAGE_REDIRECT,
                         bloom_filter, kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/false, &config);
  AddBloomFilterToConfig(optimization_guide::proto::NOSCRIPT, bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/false, &config);
  AddBloomFilterToConfig(optimization_guide::proto::DEFER_ALL_SCRIPT,
                         bloom_filter, kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/true, &config);

  {
    base::HistogramTester histogram_tester;

    ProcessHints(config, "1.0.0.0");

    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect", 0);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.NoScript", 0);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.DeferAllScript", 0);
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
        optimization_guide::OptimizationFilterStatus::kFoundServerFilterConfig,
        1);
    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
        optimization_guide::OptimizationFilterStatus::kCreatedServerFilter, 1);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.NoScript", 0);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.DeferAllScript", 0);
    EXPECT_TRUE(hints_manager()->HasLoadedOptimizationBlocklist(
        optimization_guide::proto::LITE_PAGE_REDIRECT));
    EXPECT_FALSE(hints_manager()->HasLoadedOptimizationBlocklist(
        optimization_guide::proto::NOSCRIPT));
    EXPECT_FALSE(hints_manager()->HasLoadedOptimizationAllowlist(
        optimization_guide::proto::DEFER_ALL_SCRIPT));
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
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.DeferAllScript", 0);
  }

  // Registering a new optimization type without a filter does not trigger a
  // reload of the filter.
  {
    base::HistogramTester histogram_tester;

    base::RunLoop run_loop;
    hints_manager()->ListenForNextUpdateForTesting(run_loop.QuitClosure());
    hints_manager()->RegisterOptimizationTypes(
        {optimization_guide::proto::PERFORMANCE_HINTS});
    run_loop.Run();

    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect", 0);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.NoScript", 0);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.DeferAllScript", 0);
  }

  // Registering a new optimization types with filters does trigger a
  // reload of the filters.
  {
    base::HistogramTester histogram_tester;

    base::RunLoop run_loop;
    hints_manager()->ListenForNextUpdateForTesting(run_loop.QuitClosure());
    hints_manager()->RegisterOptimizationTypes(
        {optimization_guide::proto::NOSCRIPT,
         optimization_guide::proto::DEFER_ALL_SCRIPT});
    run_loop.Run();

    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
        optimization_guide::OptimizationFilterStatus::kFoundServerFilterConfig,
        1);
    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
        optimization_guide::OptimizationFilterStatus::kCreatedServerFilter, 1);
    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.NoScript",
        optimization_guide::OptimizationFilterStatus::kFoundServerFilterConfig,
        1);
    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.NoScript",
        optimization_guide::OptimizationFilterStatus::kCreatedServerFilter, 1);
    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.DeferAllScript",
        optimization_guide::OptimizationFilterStatus::kFoundServerFilterConfig,
        1);
    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.DeferAllScript",
        optimization_guide::OptimizationFilterStatus::kCreatedServerFilter, 1);
    EXPECT_TRUE(hints_manager()->HasLoadedOptimizationBlocklist(
        optimization_guide::proto::LITE_PAGE_REDIRECT));
    EXPECT_TRUE(hints_manager()->HasLoadedOptimizationBlocklist(
        optimization_guide::proto::NOSCRIPT));
    EXPECT_TRUE(hints_manager()->HasLoadedOptimizationAllowlist(
        optimization_guide::proto::DEFER_ALL_SCRIPT));
  }
}

TEST_F(HintsManagerTest, OptimizationFiltersOnlyLoadOncePerType) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LITE_PAGE_REDIRECT});

  base::HistogramTester histogram_tester;

  optimization_guide::proto::Configuration config;
  optimization_guide::BloomFilter blocklist_bloom_filter(
      kDefaultHostBloomFilterNumHashFunctions, kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&blocklist_bloom_filter);
  AddBloomFilterToConfig(
      optimization_guide::proto::LITE_PAGE_REDIRECT, blocklist_bloom_filter,
      kDefaultHostBloomFilterNumHashFunctions, kDefaultHostBloomFilterNumBits,
      /*is_allowlist=*/false, &config);
  AddBloomFilterToConfig(
      optimization_guide::proto::LITE_PAGE_REDIRECT, blocklist_bloom_filter,
      kDefaultHostBloomFilterNumHashFunctions, kDefaultHostBloomFilterNumBits,
      /*is_allowlist=*/false, &config);
  // Make sure it will only load one of an allowlist or a blocklist.
  AddBloomFilterToConfig(
      optimization_guide::proto::LITE_PAGE_REDIRECT, blocklist_bloom_filter,
      kDefaultHostBloomFilterNumHashFunctions, kDefaultHostBloomFilterNumBits,
      /*is_allowlist=*/true, &config);
  ProcessHints(config, "1.0.0.0");

  // We found 2 LPR blocklists: parsed one and duped the other.
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
      optimization_guide::OptimizationFilterStatus::kFoundServerFilterConfig,
      3);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
      optimization_guide::OptimizationFilterStatus::kCreatedServerFilter, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
      optimization_guide::OptimizationFilterStatus::
          kFailedServerFilterDuplicateConfig,
      2);
}

TEST_F(HintsManagerTest, InvalidOptimizationFilterNotLoaded) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LITE_PAGE_REDIRECT});

  base::HistogramTester histogram_tester;

  int too_many_bits =
      optimization_guide::features::MaxServerBloomFilterByteSize() * 8 + 1;

  optimization_guide::proto::Configuration config;
  optimization_guide::BloomFilter blocklist_bloom_filter(
      kDefaultHostBloomFilterNumHashFunctions, too_many_bits);
  PopulateBloomFilterWithDefaultHost(&blocklist_bloom_filter);
  AddBloomFilterToConfig(optimization_guide::proto::LITE_PAGE_REDIRECT,
                         blocklist_bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions, too_many_bits,
                         /*is_allowlist=*/false, &config);
  ProcessHints(config, "1.0.0.0");

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
      optimization_guide::OptimizationFilterStatus::kFoundServerFilterConfig,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
      optimization_guide::OptimizationFilterStatus::kFailedServerFilterTooBig,
      1);
  EXPECT_FALSE(hints_manager()->HasLoadedOptimizationBlocklist(
      optimization_guide::proto::LITE_PAGE_REDIRECT));
}

TEST_F(HintsManagerTest, CanApplyOptimizationUrlWithNoHost) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LITE_PAGE_REDIRECT});

  optimization_guide::proto::Configuration config;
  optimization_guide::BloomFilter blocklist_bloom_filter(
      kDefaultHostBloomFilterNumHashFunctions, kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&blocklist_bloom_filter);
  AddBloomFilterToConfig(
      optimization_guide::proto::LITE_PAGE_REDIRECT, blocklist_bloom_filter,
      kDefaultHostBloomFilterNumHashFunctions, kDefaultHostBloomFilterNumBits,
      /*is_allowlist=*/false, &config);
  ProcessHints(config, "1.0.0.0");

  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          GURL("urlwithnohost"), /*navigation_id=*/absl::nullopt,
          optimization_guide::proto::LITE_PAGE_REDIRECT,
          /*optimization_metadata=*/nullptr);

  // Make sure decisions are logged correctly.
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kNoHintAvailable,
            optimization_type_decision);
}

TEST_F(HintsManagerTest, CanApplyOptimizationHasFilterForTypeButNotLoadedYet) {
  optimization_guide::proto::Configuration config;
  optimization_guide::BloomFilter blocklist_bloom_filter(
      kDefaultHostBloomFilterNumHashFunctions, kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&blocklist_bloom_filter);
  AddBloomFilterToConfig(
      optimization_guide::proto::LITE_PAGE_REDIRECT, blocklist_bloom_filter,
      kDefaultHostBloomFilterNumHashFunctions, kDefaultHostBloomFilterNumBits,
      /*is_allowlist=*/false, &config);
  ProcessHints(config, "1.0.0.0");
  // Append the switch for processing hints to force the filter to not get
  // loaded.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kHintsProtoOverride);

  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LITE_PAGE_REDIRECT});
  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          GURL("https://whatever.com/123"), /*navigation_id=*/absl::nullopt,
          optimization_guide::proto::LITE_PAGE_REDIRECT,
          /*optimization_metadata=*/nullptr);

  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::
                kHadOptimizationFilterButNotLoadedInTime,
            optimization_type_decision);

  // Run until idle to ensure we don't crash because the test object has gone
  // away.
  RunUntilIdle();
}

TEST_F(HintsManagerTest,
       CanApplyOptimizationHasLoadedFilterForTypeUrlInAllowlist) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LITE_PAGE_REDIRECT});

  optimization_guide::proto::Configuration config;
  optimization_guide::BloomFilter allowlist_bloom_filter(
      kDefaultHostBloomFilterNumHashFunctions, kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&allowlist_bloom_filter);
  AddBloomFilterToConfig(
      optimization_guide::proto::LITE_PAGE_REDIRECT, allowlist_bloom_filter,
      kDefaultHostBloomFilterNumHashFunctions, kDefaultHostBloomFilterNumBits,
      /*is_allowlist=*/true, &config);
  ProcessHints(config, "1.0.0.0");

  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          GURL("https://m.host.com/123"), /*navigation_id=*/absl::nullopt,
          optimization_guide::proto::LITE_PAGE_REDIRECT,
          /*optimization_metadata=*/nullptr);

  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::
                kAllowedByOptimizationFilter,
            optimization_type_decision);
}

TEST_F(HintsManagerTest,
       CanApplyOptimizationHasLoadedFilterForTypeUrlInBlocklist) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LITE_PAGE_REDIRECT});

  optimization_guide::proto::Configuration config;
  optimization_guide::BloomFilter blocklist_bloom_filter(
      kDefaultHostBloomFilterNumHashFunctions, kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&blocklist_bloom_filter);
  AddBloomFilterToConfig(
      optimization_guide::proto::LITE_PAGE_REDIRECT, blocklist_bloom_filter,
      kDefaultHostBloomFilterNumHashFunctions, kDefaultHostBloomFilterNumBits,
      /*is_allowlist=*/false, &config);
  ProcessHints(config, "1.0.0.0");

  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          GURL("https://m.host.com/123"), /*navigation_id=*/absl::nullopt,
          optimization_guide::proto::LITE_PAGE_REDIRECT,
          /*optimization_metadata=*/nullptr);

  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::
                kNotAllowedByOptimizationFilter,
            optimization_type_decision);
}

TEST_F(HintsManagerTest,
       CanApplyOptimizationHasLoadedFilterForTypeUrlNotInAllowlistFilter) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LITE_PAGE_REDIRECT});

  optimization_guide::proto::Configuration config;
  optimization_guide::BloomFilter allowlist_bloom_filter(
      kDefaultHostBloomFilterNumHashFunctions, kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&allowlist_bloom_filter);
  AddBloomFilterToConfig(
      optimization_guide::proto::LITE_PAGE_REDIRECT, allowlist_bloom_filter,
      kDefaultHostBloomFilterNumHashFunctions, kDefaultHostBloomFilterNumBits,
      /*is_allowlist=*/true, &config);
  ProcessHints(config, "1.0.0.0");

  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          GURL("https://whatever.com/123"), /*navigation_id=*/absl::nullopt,
          optimization_guide::proto::LITE_PAGE_REDIRECT,
          /*optimization_metadata=*/nullptr);

  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::
                kNotAllowedByOptimizationFilter,
            optimization_type_decision);
}

TEST_F(HintsManagerTest,
       CanApplyOptimizationHasLoadedFilterForTypeUrlNotInBlocklistFilter) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LITE_PAGE_REDIRECT});

  optimization_guide::proto::Configuration config;
  optimization_guide::BloomFilter blocklist_bloom_filter(
      kDefaultHostBloomFilterNumHashFunctions, kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&blocklist_bloom_filter);
  AddBloomFilterToConfig(
      optimization_guide::proto::LITE_PAGE_REDIRECT, blocklist_bloom_filter,
      kDefaultHostBloomFilterNumHashFunctions, kDefaultHostBloomFilterNumBits,
      /*is_allowlist=*/false, &config);
  ProcessHints(config, "1.0.0.0");

  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          GURL("https://whatever.com/123"), /*navigation_id=*/absl::nullopt,
          optimization_guide::proto::LITE_PAGE_REDIRECT,
          /*optimization_metadata=*/nullptr);

  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::
                kAllowedByOptimizationFilter,
            optimization_type_decision);
}

TEST_F(HintsManagerTest,
       CanApplyOptimizationOptimizationTypeAllowlistedAtTopLevel) {
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("somedomain.org");
  hint1->set_key_representation(optimization_guide::proto::HOST);
  hint1->set_version("someversion");
  optimization_guide::proto::Optimization* opt1 =
      hint1->add_allowlisted_optimizations();
  opt1->set_optimization_type(optimization_guide::proto::RESOURCE_LOADING);
  ProcessHints(config, "1.0.0.0");

  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::RESOURCE_LOADING});

  auto navigation_data = CreateTestNavigationData(
      url_with_hints(), {optimization_guide::proto::RESOURCE_LOADING});
  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_data->navigation_url(), /*navigation_id=*/absl::nullopt,
          optimization_guide::proto::RESOURCE_LOADING, &optimization_metadata);
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kAllowedByHint,
            optimization_type_decision);
}

TEST_F(HintsManagerTest,
       CanApplyOptimizationOptimizationTypeHasTuningVersionShouldLogUKM) {
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("somedomain.org");
  hint1->set_key_representation(optimization_guide::proto::HOST);
  hint1->set_version("someversion");
  optimization_guide::proto::Optimization* opt1 =
      hint1->add_allowlisted_optimizations();
  opt1->set_optimization_type(optimization_guide::proto::RESOURCE_LOADING);
  opt1->set_tuning_version(123456);
  ProcessHints(config, "1.0.0.0");

  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::RESOURCE_LOADING});

  auto navigation_data = CreateTestNavigationData(
      url_with_hints(), {optimization_guide::proto::RESOURCE_LOADING});
  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_data->navigation_url(), navigation_data->navigation_id(),
          optimization_guide::proto::RESOURCE_LOADING, &optimization_metadata);
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kAllowedByHint,
            optimization_type_decision);

  // Make sure autotuning UKM is recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuideAutotuning::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuideAutotuning::kOptimizationTypeName,
      static_cast<int64_t>(optimization_guide::proto::RESOURCE_LOADING));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuideAutotuning::kTuningVersionName,
      123456);
}

TEST_F(
    HintsManagerTest,
    CanApplyOptimizationOptimizationTypeHostHasSentinelTuningVersionShouldLogUKM) {
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("somedomain.org");
  hint1->set_key_representation(optimization_guide::proto::HOST);
  hint1->set_version("someversion");
  optimization_guide::proto::Optimization* opt1 =
      hint1->add_allowlisted_optimizations();
  opt1->set_optimization_type(optimization_guide::proto::RESOURCE_LOADING);
  opt1->set_tuning_version(UINT64_MAX);
  ProcessHints(config, "1.0.0.0");

  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::RESOURCE_LOADING});

  auto navigation_data = CreateTestNavigationData(
      url_with_hints(), {optimization_guide::proto::RESOURCE_LOADING});
  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_data->navigation_url(), navigation_data->navigation_id(),
          optimization_guide::proto::RESOURCE_LOADING, &optimization_metadata);
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kNotAllowedByHint,
            optimization_type_decision);

  // Make sure autotuning UKM is recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuideAutotuning::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuideAutotuning::kOptimizationTypeName,
      static_cast<int64_t>(optimization_guide::proto::RESOURCE_LOADING));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuideAutotuning::kTuningVersionName,
      UINT64_MAX);
}

TEST_F(
    HintsManagerTest,
    CanApplyOptimizationOptimizationTypePatternHasSentinelTuningVersionShouldLogUKM) {
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("somedomain.org");
  hint1->set_key_representation(optimization_guide::proto::HOST);
  hint1->set_version("someversion");
  optimization_guide::proto::PageHint* ph1 = hint1->add_page_hints();
  ph1->set_page_pattern("*");
  optimization_guide::proto::Optimization* opt1 =
      ph1->add_allowlisted_optimizations();
  opt1->set_optimization_type(optimization_guide::proto::RESOURCE_LOADING);
  opt1->set_tuning_version(UINT64_MAX);
  ProcessHints(config, "1.0.0.0");

  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::RESOURCE_LOADING});

  auto navigation_data = CreateTestNavigationData(
      url_with_hints(), {optimization_guide::proto::RESOURCE_LOADING});
  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_data->navigation_url(), navigation_data->navigation_id(),
          optimization_guide::proto::RESOURCE_LOADING, &optimization_metadata);
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kNotAllowedByHint,
            optimization_type_decision);

  // Make sure autotuning UKM is recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuideAutotuning::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuideAutotuning::kOptimizationTypeName,
      static_cast<int64_t>(optimization_guide::proto::RESOURCE_LOADING));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuideAutotuning::kTuningVersionName,
      UINT64_MAX);
}

TEST_F(
    HintsManagerTest,
    CanApplyOptimizationURLKeyedOptimizationTypeHasSentinelTuningVersionShouldLogUKM) {
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key(url_with_hints().spec());
  hint1->set_key_representation(optimization_guide::proto::FULL_URL);
  hint1->set_version("someversion");
  optimization_guide::proto::PageHint* ph1 = hint1->add_page_hints();
  ph1->set_page_pattern(url_with_hints().spec());
  optimization_guide::proto::Optimization* opt1 =
      ph1->add_allowlisted_optimizations();
  opt1->set_optimization_type(optimization_guide::proto::RESOURCE_LOADING);
  opt1->set_tuning_version(UINT64_MAX);
  ProcessHints(config, "1.0.0.0");

  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::RESOURCE_LOADING});

  auto navigation_data = CreateTestNavigationData(
      url_with_hints(), {optimization_guide::proto::RESOURCE_LOADING});
  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_data->navigation_url(), navigation_data->navigation_id(),
          optimization_guide::proto::RESOURCE_LOADING, &optimization_metadata);
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kNotAllowedByHint,
            optimization_type_decision);

  // Make sure autotuning UKM is recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuideAutotuning::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuideAutotuning::kOptimizationTypeName,
      static_cast<int64_t>(optimization_guide::proto::RESOURCE_LOADING));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuideAutotuning::kTuningVersionName,
      UINT64_MAX);
}

TEST_F(HintsManagerTest,
       CanApplyOptimizationOptimizationTypeHasTuningVersionButNoNavigation) {
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("somedomain.org");
  hint1->set_key_representation(optimization_guide::proto::HOST);
  hint1->set_version("someversion");
  optimization_guide::proto::Optimization* opt1 =
      hint1->add_allowlisted_optimizations();
  opt1->set_optimization_type(optimization_guide::proto::RESOURCE_LOADING);
  opt1->set_tuning_version(123456);
  ProcessHints(config, "1.0.0.0");

  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::RESOURCE_LOADING});

  auto navigation_data = CreateTestNavigationData(
      url_with_hints(), {optimization_guide::proto::RESOURCE_LOADING});
  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_data->navigation_url(), /*navigation_id=*/absl::nullopt,
          optimization_guide::proto::RESOURCE_LOADING, &optimization_metadata);
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kAllowedByHint,
            optimization_type_decision);

  // Make sure autotuning UKM is not recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuideAutotuning::kEntryName);
  EXPECT_EQ(0u, entries.size());
}

TEST_F(HintsManagerTest,
       CanApplyOptimizationOptimizationTypeHasNavigationButNoTuningVersion) {
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("somedomain.org");
  hint1->set_key_representation(optimization_guide::proto::HOST);
  hint1->set_version("someversion");
  optimization_guide::proto::Optimization* opt1 =
      hint1->add_allowlisted_optimizations();
  opt1->set_optimization_type(optimization_guide::proto::RESOURCE_LOADING);
  ProcessHints(config, "1.0.0.0");

  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::RESOURCE_LOADING});

  auto navigation_data = CreateTestNavigationData(
      url_with_hints(), {optimization_guide::proto::RESOURCE_LOADING});
  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_data->navigation_url(), navigation_data->navigation_id(),
          optimization_guide::proto::RESOURCE_LOADING, &optimization_metadata);
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kAllowedByHint,
            optimization_type_decision);

  // Make sure autotuning UKM is not recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuideAutotuning::kEntryName);
  EXPECT_EQ(0u, entries.size());
}

TEST_F(HintsManagerTest, CanApplyOptimizationHasPageHintButNoMatchingOptType) {
  InitializeWithDefaultConfig("1.0.0.0");
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});

  auto navigation_data = CreateTestNavigationData(
      url_with_hints(), {optimization_guide::proto::DEFER_ALL_SCRIPT});
  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_data->navigation_url(), /*navigation_id=*/absl::nullopt,
          optimization_guide::proto::DEFER_ALL_SCRIPT,
          /*optimization_metadata=*/nullptr);

  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kNotAllowedByHint,
            optimization_type_decision);
}

TEST_F(HintsManagerTest,
       CanApplyOptimizationAndPopulatesPerformanceHintsMetadata) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::PERFORMANCE_HINTS});
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint = config.add_hints();
  hint->set_key("somedomain.org");
  hint->set_key_representation(optimization_guide::proto::HOST);
  hint->set_version("someversion");
  optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("/news/");
  optimization_guide::proto::Optimization* opt =
      page_hint->add_allowlisted_optimizations();
  opt->set_optimization_type(optimization_guide::proto::PERFORMANCE_HINTS);
  optimization_guide::proto::PerformanceHint* performance_hint =
      opt->mutable_performance_hints_metadata()->add_performance_hints();
  performance_hint->set_wildcard_pattern("somedomain.org");
  performance_hint->set_performance_class(
      optimization_guide::proto::PERFORMANCE_SLOW);

  ProcessHints(config, "1.0.0.0");

  base::RunLoop run_loop;
  auto navigation_data = CreateTestNavigationData(
      url_with_hints(), {optimization_guide::proto::PERFORMANCE_HINTS});
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_data->navigation_url(), /*navigation_id=*/absl::nullopt,
          optimization_guide::proto::PERFORMANCE_HINTS, &optimization_metadata);
  // Make sure performance hints metadata is populated.
  EXPECT_TRUE(optimization_metadata.performance_hints_metadata().has_value());
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kAllowedByHint,
            optimization_type_decision);
}

TEST_F(HintsManagerTest, CanApplyOptimizationAndPopulatesPublicImageMetadata) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint = config.add_hints();
  hint->set_key("somedomain.org");
  hint->set_key_representation(optimization_guide::proto::HOST);
  hint->set_version("someversion");
  optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("/news/");
  optimization_guide::proto::Optimization* opt =
      page_hint->add_allowlisted_optimizations();
  opt->set_optimization_type(optimization_guide::proto::COMPRESS_PUBLIC_IMAGES);
  opt->mutable_public_image_metadata()->add_url("someimage");

  ProcessHints(config, "1.0.0.0");

  auto navigation_data = CreateTestNavigationData(
      url_with_hints(), {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});
  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_data->navigation_url(), /*navigation_id=*/absl::nullopt,
          optimization_guide::proto::COMPRESS_PUBLIC_IMAGES,
          &optimization_metadata);
  // Make sure public images metadata is populated.
  EXPECT_TRUE(optimization_metadata.public_image_metadata().has_value());
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kAllowedByHint,
            optimization_type_decision);
}

TEST_F(HintsManagerTest,
       CanApplyOptimizationAndPopulatesLoadingPredictorMetadata) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LOADING_PREDICTOR});
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint = config.add_hints();
  hint->set_key("somedomain.org");
  hint->set_key_representation(optimization_guide::proto::HOST);
  hint->set_version("someversion");
  optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("/news/");
  optimization_guide::proto::Optimization* opt =
      page_hint->add_allowlisted_optimizations();
  opt->set_optimization_type(optimization_guide::proto::LOADING_PREDICTOR);
  opt->mutable_loading_predictor_metadata()->add_subresources()->set_url(
      "https://resource.com/");

  ProcessHints(config, "1.0.0.0");

  auto navigation_data = CreateTestNavigationData(
      url_with_hints(), {optimization_guide::proto::LOADING_PREDICTOR});
  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_data->navigation_url(), /*navigation_id=*/absl::nullopt,
          optimization_guide::proto::LOADING_PREDICTOR, &optimization_metadata);
  // Make sure loading predictor metadata is populated.
  EXPECT_TRUE(optimization_metadata.loading_predictor_metadata().has_value());
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kAllowedByHint,
            optimization_type_decision);
}

TEST_F(HintsManagerTest, CanApplyOptimizationAndPopulatesAnyMetadata) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LOADING_PREDICTOR});
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint = config.add_hints();
  hint->set_key("somedomain.org");
  hint->set_key_representation(optimization_guide::proto::HOST);
  hint->set_version("someversion");
  optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("/news/");
  optimization_guide::proto::Optimization* opt =
      page_hint->add_allowlisted_optimizations();
  opt->set_optimization_type(optimization_guide::proto::LOADING_PREDICTOR);
  optimization_guide::proto::LoadingPredictorMetadata lp_metadata;
  lp_metadata.add_subresources()->set_url("https://resource.com/");
  lp_metadata.SerializeToString(opt->mutable_any_metadata()->mutable_value());
  opt->mutable_any_metadata()->set_type_url(
      "type.googleapis.com/com.foo.LoadingPredictorMetadata");

  ProcessHints(config, "1.0.0.0");

  auto navigation_data = CreateTestNavigationData(
      url_with_hints(), {optimization_guide::proto::LOADING_PREDICTOR});
  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_data->navigation_url(), navigation_data->navigation_id(),
          optimization_guide::proto::LOADING_PREDICTOR, &optimization_metadata);
  // Make sure loading predictor metadata is populated.
  EXPECT_TRUE(
      optimization_metadata
          .ParsedMetadata<optimization_guide::proto::LoadingPredictorMetadata>()
          .has_value());
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kAllowedByHint,
            optimization_type_decision);
}

TEST_F(HintsManagerTest, CanApplyOptimizationNoMatchingPageHint) {
  InitializeWithDefaultConfig("1.0.0.0");

  // Set to online so fetch is activated.
  SetConnectionOnline();
  auto navigation_data =
      CreateTestNavigationData(GURL("https://somedomain.org/nomatch"), {});
  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::NOSCRIPT});
  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(navigation_data->navigation_url(),
                                            /*navigation_id=*/absl::nullopt,
                                            optimization_guide::proto::NOSCRIPT,
                                            /*optimization_metadata=*/nullptr);

  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kNotAllowedByHint,
            optimization_type_decision);
}

TEST_F(HintsManagerTest,
       CanApplyOptimizationNoHintForNavigationMetadataClearedAnyway) {
  InitializeWithDefaultConfig("1.0.0.0");

  auto navigation_data = CreateTestNavigationData(
      GURL("https://nohint.com"),
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});

  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::NOSCRIPT});
  optimization_guide::OptimizationMetadata optimization_metadata;

  optimization_guide::proto::PerformanceHintsMetadata hints_metadata;
  auto* hint = hints_metadata.add_performance_hints();
  hint->set_wildcard_pattern("test.com");
  hint->set_performance_class(optimization_guide::proto::PERFORMANCE_SLOW);
  optimization_guide::OptimizationMetadata metadata;
  optimization_metadata.set_performance_hints_metadata(hints_metadata);

  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_data->navigation_url(), /*navigation_id=*/absl::nullopt,
          optimization_guide::proto::NOSCRIPT, &optimization_metadata);

  EXPECT_FALSE(optimization_metadata.performance_hints_metadata().has_value());
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kNoHintAvailable,
            optimization_type_decision);
}

TEST_F(HintsManagerTest, CanApplyOptimizationHasHintInCacheButNotLoaded) {
  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::NOSCRIPT});
  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          url_with_hints(), /*navigation_id=*/absl::nullopt,
          optimization_guide::proto::NOSCRIPT, &optimization_metadata);

  EXPECT_EQ(
      optimization_guide::OptimizationTypeDecision::kHadHintButNotLoadedInTime,
      optimization_type_decision);
}

TEST_F(HintsManagerTest, CanApplyOptimizationFilterTakesPrecedence) {
  auto navigation_data = CreateTestNavigationData(
      GURL("https://m.host.com/urlinfilterandhints"), {});

  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LITE_PAGE_REDIRECT});

  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("host.com");
  hint1->set_key_representation(optimization_guide::proto::HOST);
  hint1->set_version("someversion");
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("https://m.host.com");
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_allowlisted_optimizations();
  optimization1->set_optimization_type(
      optimization_guide::proto::LITE_PAGE_REDIRECT);
  optimization_guide::BloomFilter blocklist_bloom_filter(
      kDefaultHostBloomFilterNumHashFunctions, kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&blocklist_bloom_filter);
  AddBloomFilterToConfig(
      optimization_guide::proto::LITE_PAGE_REDIRECT, blocklist_bloom_filter,
      kDefaultHostBloomFilterNumHashFunctions, kDefaultHostBloomFilterNumBits,
      /*is_allowlist=*/false, &config);
  ProcessHints(config, "1.0.0.0");

  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_data->navigation_url(), /*navigation_id=*/absl::nullopt,
          optimization_guide::proto::LITE_PAGE_REDIRECT,
          /*optimization_metadata=*/nullptr);

  // Make sure decision points logged correctly.
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::
                kNotAllowedByOptimizationFilter,
            optimization_type_decision);
}

TEST_F(HintsManagerTest,
       CanApplyOptimizationFilterTakesPrecedenceMatchesFilter) {
  auto navigation_data = CreateTestNavigationData(
      GURL("https://notfiltered.com/whatever"),
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});

  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LITE_PAGE_REDIRECT});

  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("notfiltered.com");
  hint1->set_key_representation(optimization_guide::proto::HOST);
  hint1->set_version("someversion");
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("https://notfiltered.com");
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_allowlisted_optimizations();
  optimization1->set_optimization_type(
      optimization_guide::proto::LITE_PAGE_REDIRECT);
  optimization_guide::BloomFilter blocklist_bloom_filter(
      kDefaultHostBloomFilterNumHashFunctions, kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&blocklist_bloom_filter);
  AddBloomFilterToConfig(
      optimization_guide::proto::LITE_PAGE_REDIRECT, blocklist_bloom_filter,
      kDefaultHostBloomFilterNumHashFunctions, kDefaultHostBloomFilterNumBits,
      /*is_allowlist=*/false, &config);
  ProcessHints(config, "1.0.0.0");

  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_data->navigation_url(), /*navigation_id=*/absl::nullopt,
          optimization_guide::proto::LITE_PAGE_REDIRECT,
          /*optimization_metadata=*/nullptr);

  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::
                kAllowedByOptimizationFilter,
            optimization_type_decision);
}

class HintsManagerFetchingDisabledTest : public HintsManagerTest {
 public:
  HintsManagerFetchingDisabledTest() {
    scoped_list_.InitAndDisableFeature(
        optimization_guide::features::kRemoteOptimizationGuideFetching);
  }

 private:
  base::test::ScopedFeatureList scoped_list_;
};

TEST_F(HintsManagerFetchingDisabledTest,
       HintsFetchNotAllowedIfFeatureIsNotEnabled) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);

  std::unique_ptr<FakeTopHostProvider> top_host_provider =
      std::make_unique<FakeTopHostProvider>(
          std::vector<std::string>({"example1.com", "example2.com"}));

  CreateHintsManager(top_host_provider.get());
  InitializeWithDefaultConfig("1.0.0");

  // Force timer to expire and schedule a hints fetch.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kUpdateFetchHintsTimeSecs));
  EXPECT_EQ(0, top_host_provider->get_num_top_hosts_called());
  // Hints fetcher should not even be created.
  EXPECT_FALSE(batch_update_hints_fetcher());
}

TEST_F(HintsManagerTest,
       CanApplyOptimizationAsyncReturnsRightAwayIfNotAllowedToFetch) {
  base::HistogramTester histogram_tester;

  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});
  InitializeWithDefaultConfig("1.0.0.0");

  auto navigation_data = CreateTestNavigationData(
      url_without_hints(), {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});
  hints_manager()->CanApplyOptimizationAsync(
      url_without_hints(), navigation_data->navigation_id(),
      optimization_guide::proto::COMPRESS_PUBLIC_IMAGES,
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
    HintsManagerTest,
    CanApplyOptimizationAsyncReturnsRightAwayIfNotAllowedToFetchAndNotAllowlistedByAvailableHint) {
  base::HistogramTester histogram_tester;

  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});
  InitializeWithDefaultConfig("1.0.0.0");

  auto navigation_data = CreateTestNavigationData(
      url_with_hints(), {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});
  // Wait for hint to be loaded.
  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  hints_manager()->CanApplyOptimizationAsync(
      url_with_hints(), navigation_data->navigation_id(),
      optimization_guide::proto::COMPRESS_PUBLIC_IMAGES,
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

TEST_F(HintsManagerTest, RemoveFetchedEntriesByHintKeys_Host) {
  int cache_duration_in_secs = 60;
  GURL url("https://host.com/r/cats");

  std::unique_ptr<optimization_guide::proto::GetHintsResponse>
      get_hints_response =
          std::make_unique<optimization_guide::proto::GetHintsResponse>();

  optimization_guide::proto::Hint* hint = get_hints_response->add_hints();
  hint->set_key(url.spec());
  hint->set_key_representation(optimization_guide::proto::FULL_URL);
  hint->mutable_max_cache_duration()->set_seconds(cache_duration_in_secs);
  optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->add_allowlisted_optimizations()->set_optimization_type(
      optimization_guide::proto::PERFORMANCE_HINTS);
  page_hint->set_page_pattern("whatever/*");

  hint = get_hints_response->add_hints();
  hint->set_key_representation(optimization_guide::proto::HOST);
  hint->set_key(url.host());
  page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("anything/*");

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  hints_manager()->hint_cache()->UpdateFetchedHints(
      std::move(get_hints_response), base::Time().Now(), {url.host()}, {url},
      run_loop->QuitClosure());
  EXPECT_TRUE(hints_manager()->hint_cache()->HasHint(url.host()));
  EXPECT_TRUE(hints_manager()->hint_cache()->HasURLKeyedEntryForURL(url));

  run_loop = std::make_unique<base::RunLoop>();
  hints_manager()->RemoveFetchedEntriesByHintKeys(
      run_loop->QuitClosure(),
      optimization_guide::proto::KeyRepresentation::HOST, {url.host()});
  run_loop->Run();

  EXPECT_FALSE(hints_manager()->hint_cache()->HasHint(url.host()));
  EXPECT_TRUE(hints_manager()->hint_cache()->HasURLKeyedEntryForURL(url));
}

TEST_F(HintsManagerTest, RemoveFetchedEntriesByHintKeys_URL) {
  int cache_duration_in_secs = 60;
  GURL url("https://host.com/r/cats");

  std::unique_ptr<optimization_guide::proto::GetHintsResponse>
      get_hints_response =
          std::make_unique<optimization_guide::proto::GetHintsResponse>();

  optimization_guide::proto::Hint* hint = get_hints_response->add_hints();
  hint->set_key(url.spec());
  hint->set_key_representation(optimization_guide::proto::FULL_URL);
  hint->mutable_max_cache_duration()->set_seconds(cache_duration_in_secs);
  optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->add_allowlisted_optimizations()->set_optimization_type(
      optimization_guide::proto::PERFORMANCE_HINTS);
  page_hint->set_page_pattern("whatever/*");

  hint = get_hints_response->add_hints();
  hint->set_key_representation(optimization_guide::proto::HOST);
  hint->set_key(url.host());
  page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("anything/*");

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  hints_manager()->hint_cache()->UpdateFetchedHints(
      std::move(get_hints_response), base::Time().Now(), {url.host()}, {url},
      run_loop->QuitClosure());
  EXPECT_TRUE(hints_manager()->hint_cache()->HasHint(url.host()));
  EXPECT_TRUE(hints_manager()->hint_cache()->HasURLKeyedEntryForURL(url));

  run_loop = std::make_unique<base::RunLoop>();
  hints_manager()->RemoveFetchedEntriesByHintKeys(
      run_loop->QuitClosure(),
      optimization_guide::proto::KeyRepresentation::FULL_URL, {url.spec()});
  run_loop->Run();

  // Both the host and url entries should have been removed to support upgrading
  // hint keys from HOST to FULL_URL.
  EXPECT_FALSE(hints_manager()->hint_cache()->HasHint(url.host()));
  EXPECT_FALSE(hints_manager()->hint_cache()->HasURLKeyedEntryForURL(url));
}

TEST_F(HintsManagerTest, PurgeFetchedEntries) {
  int cache_duration_in_secs = 60;
  GURL url("https://host.com/r/cats");

  std::unique_ptr<optimization_guide::proto::GetHintsResponse>
      get_hints_response =
          std::make_unique<optimization_guide::proto::GetHintsResponse>();

  optimization_guide::proto::Hint* hint = get_hints_response->add_hints();
  hint->set_key(url.spec());
  hint->set_key_representation(optimization_guide::proto::FULL_URL);
  hint->mutable_max_cache_duration()->set_seconds(cache_duration_in_secs);
  optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->add_allowlisted_optimizations()->set_optimization_type(
      optimization_guide::proto::PERFORMANCE_HINTS);
  page_hint->set_page_pattern("whatever/*");

  hint = get_hints_response->add_hints();
  hint->set_key_representation(optimization_guide::proto::HOST);
  hint->set_key(url.host());
  page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("anything/*");

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  hints_manager()->hint_cache()->UpdateFetchedHints(
      std::move(get_hints_response), base::Time().Now(), {url.host()}, {url},
      run_loop->QuitClosure());
  EXPECT_TRUE(hints_manager()->hint_cache()->HasHint(url.host()));
  EXPECT_TRUE(hints_manager()->hint_cache()->HasURLKeyedEntryForURL(url));

  run_loop = std::make_unique<base::RunLoop>();
  hints_manager()->PurgeFetchedEntries(run_loop->QuitClosure());
  run_loop->Run();

  EXPECT_FALSE(hints_manager()->hint_cache()->HasHint(url.host()));
  EXPECT_FALSE(hints_manager()->hint_cache()->HasURLKeyedEntryForURL(url));
}

TEST_F(HintsManagerTest, HintFetcherPrefUpdated_URL) {
  base::Time expiry = base::Time::Now() + base::TimeDelta::FromHours(1);
  optimization_guide::HintsFetcher::AddFetchedHostForTesting(
      pref_service(), "host-key.com", expiry);
  optimization_guide::HintsFetcher::AddFetchedHostForTesting(
      pref_service(), "url-key.com", expiry);

  ASSERT_TRUE(optimization_guide::HintsFetcher::WasHostCoveredByFetch(
      pref_service(), "host-key.com"));
  ASSERT_TRUE(optimization_guide::HintsFetcher::WasHostCoveredByFetch(
      pref_service(), "url-key.com"));

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  hints_manager()->RemoveFetchedEntriesByHintKeys(
      run_loop->QuitClosure(),
      optimization_guide::proto::KeyRepresentation::FULL_URL,
      {
          GURL("https://host-key.com/page").spec(),
          GURL("https://url-key.com/page").spec(),
      });
  run_loop->Run();

  EXPECT_FALSE(optimization_guide::HintsFetcher::WasHostCoveredByFetch(
      pref_service(), "host-key.com"));
  EXPECT_FALSE(optimization_guide::HintsFetcher::WasHostCoveredByFetch(
      pref_service(), "url-key.com"));
}

TEST_F(HintsManagerTest, HintFetcherPrefUpdated_Hosts) {
  base::Time expiry = base::Time::Now() + base::TimeDelta::FromHours(1);
  optimization_guide::HintsFetcher::AddFetchedHostForTesting(
      pref_service(), "host-key.com", expiry);
  optimization_guide::HintsFetcher::AddFetchedHostForTesting(
      pref_service(), "url-key.com", expiry);

  ASSERT_TRUE(optimization_guide::HintsFetcher::WasHostCoveredByFetch(
      pref_service(), "host-key.com"));
  ASSERT_TRUE(optimization_guide::HintsFetcher::WasHostCoveredByFetch(
      pref_service(), "url-key.com"));

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  hints_manager()->RemoveFetchedEntriesByHintKeys(
      run_loop->QuitClosure(),
      optimization_guide::proto::KeyRepresentation::HOST,
      {
          "host-key.com",
          "url-key.com",
      });
  run_loop->Run();

  EXPECT_FALSE(optimization_guide::HintsFetcher::WasHostCoveredByFetch(
      pref_service(), "host-key.com"));
  EXPECT_FALSE(optimization_guide::HintsFetcher::WasHostCoveredByFetch(
      pref_service(), "url-key.com"));
}

class HintsManagerFetchingTest : public HintsManagerTest {
 public:
  HintsManagerFetchingTest() {
    scoped_list_.InitWithFeaturesAndParameters(
        {
            {
                optimization_guide::features::kRemoteOptimizationGuideFetching,
                {{"max_concurrent_page_navigation_fetches", "2"}},
            },
        },
        {optimization_guide::features::
             kRemoteOptimizationGuideFetchingAnonymousDataConsent});
  }

 private:
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  base::test::ScopedFeatureList scoped_list_;
};

TEST_F(HintsManagerFetchingTest,
       HintsFetchNotAllowedIfFeatureIsEnabledButUserNotAllowed) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  CreateHintsManager(/*top_host_provider=*/nullptr);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithHostHints}));
  InitializeWithDefaultConfig("1.0.0");

  // Force timer to expire and schedule a hints fetch.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kUpdateFetchHintsTimeSecs));
  // Hints fetcher should not even be created.
  EXPECT_FALSE(batch_update_hints_fetcher());
}

TEST_F(HintsManagerFetchingTest,
       NoRegisteredOptimizationTypesAndHintsFetchNotAttempted) {
  std::unique_ptr<FakeTopHostProvider> top_host_provider =
      std::make_unique<FakeTopHostProvider>(
          std::vector<std::string>({"example1.com", "example2.com"}));

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  CreateHintsManager(top_host_provider.get());

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithHostHints}));
  InitializeWithDefaultConfig("1.0.0");

  // Force timer to expire and schedule a hints fetch but the fetch is not made.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kUpdateFetchHintsTimeSecs));
  EXPECT_EQ(0, top_host_provider->get_num_top_hosts_called());
  // Hints fetcher should not be created.
  EXPECT_FALSE(batch_update_hints_fetcher());
}

TEST_F(HintsManagerFetchingTest,
       OnlyFilterTypesRegisteredHintsFetchNotAttempted) {
  optimization_guide::proto::Configuration config;
  optimization_guide::BloomFilter allowlist_bloom_filter(
      kDefaultHostBloomFilterNumHashFunctions, kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&allowlist_bloom_filter);
  AddBloomFilterToConfig(
      optimization_guide::proto::LITE_PAGE_REDIRECT, allowlist_bloom_filter,
      kDefaultHostBloomFilterNumHashFunctions, kDefaultHostBloomFilterNumBits,
      /*is_allowlist=*/true, &config);

  std::unique_ptr<FakeTopHostProvider> top_host_provider =
      std::make_unique<FakeTopHostProvider>(
          std::vector<std::string>({"example1.com", "example2.com"}));

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  CreateHintsManager(top_host_provider.get());
  ProcessHints(config, "1.0.0.0");

  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LITE_PAGE_REDIRECT});
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithHostHints}));

  // Force timer to expire after random delay and schedule a hints fetch.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(60 * 2));
  EXPECT_EQ(0, top_host_provider->get_num_top_hosts_called());
  // Hints fetcher should not be created.
  EXPECT_FALSE(batch_update_hints_fetcher());
}

TEST_F(HintsManagerFetchingTest, HintsFetcherEnabledNoHostsOrUrlsToFetch) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  std::unique_ptr<FakeTopHostProvider> top_host_provider =
      std::make_unique<FakeTopHostProvider>(std::vector<std::string>({}));
  CreateHintsManager(top_host_provider.get());

  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithHostHints}));
  InitializeWithDefaultConfig("1.0.0");

  // Force timer to expire after random delay and schedule a hints fetch.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(60 * 2));
  EXPECT_EQ(1, top_host_provider->get_num_top_hosts_called());
  EXPECT_EQ(1, tab_url_provider()->get_num_urls_called());
  // Hints fetcher should not be even created.
  EXPECT_FALSE(batch_update_hints_fetcher());

  // Move it forward again to make sure timer is scheduled.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kUpdateFetchHintsTimeSecs));
  EXPECT_EQ(2, top_host_provider->get_num_top_hosts_called());
  EXPECT_EQ(2, tab_url_provider()->get_num_urls_called());
  // Still no hosts or URLs, so hints fetcher should still not be even created.
  EXPECT_FALSE(batch_update_hints_fetcher());
}

TEST_F(HintsManagerFetchingTest, HintsFetcherEnabledNoHostsButHasUrlsToFetch) {
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  std::unique_ptr<FakeTopHostProvider> top_host_provider =
      std::make_unique<FakeTopHostProvider>(std::vector<std::string>({}));
  CreateHintsManager(top_host_provider.get());

  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithHostHints}));
  InitializeWithDefaultConfig("1.0.0");

  tab_url_provider()->SetUrls(
      {GURL("https://a.com"), GURL("https://b.com"), GURL("chrome://new-tab")});

  // Force timer to expire after random delay and schedule a hints fetch that
  // succeeds.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(60 * 2));
  EXPECT_EQ(1, top_host_provider->get_num_top_hosts_called());
  EXPECT_EQ(1, tab_url_provider()->get_num_urls_called());
  EXPECT_EQ(1, batch_update_hints_fetcher()->num_fetches_requested());
  EXPECT_EQ("en-US", batch_update_hints_fetcher()->locale_requested());
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintsManager.ActiveTabUrlsToFetchFor", 2, 1);

  // Move it forward again to make sure timer is scheduled.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kUpdateFetchHintsTimeSecs));
  EXPECT_EQ(2, top_host_provider->get_num_top_hosts_called());
  EXPECT_EQ(2, tab_url_provider()->get_num_urls_called());
  // Urls didn't change and we have all URLs cached in store.
  EXPECT_EQ(1, batch_update_hints_fetcher()->num_fetches_requested());
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintsManager.ActiveTabUrlsToFetchFor", 0, 1);
}

TEST_F(HintsManagerFetchingTest, HintsFetcherTimerFetch) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  std::unique_ptr<FakeTopHostProvider> top_host_provider =
      std::make_unique<FakeTopHostProvider>(
          std::vector<std::string>({"example1.com", "example2.com"}));

  CreateHintsManager(top_host_provider.get());
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithHostHints}));
  InitializeWithDefaultConfig("1.0.0");

  // Force timer to expire after random delay and schedule a hints fetch that
  // succeeds.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(60 * 2));
  EXPECT_EQ(1, batch_update_hints_fetcher()->num_fetches_requested());

  // Move it forward again to make sure timer is scheduled.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kUpdateFetchHintsTimeSecs));
  EXPECT_EQ(2, batch_update_hints_fetcher()->num_fetches_requested());
}

TEST_F(HintsManagerFetchingTest,
       HintsFetched_RegisteredOptimizationTypes_AllWithOptFilter) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LITE_PAGE_REDIRECT});

  optimization_guide::proto::Configuration config;
  optimization_guide::BloomFilter allowlist_bloom_filter(
      kDefaultHostBloomFilterNumHashFunctions, kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&allowlist_bloom_filter);
  AddBloomFilterToConfig(
      optimization_guide::proto::LITE_PAGE_REDIRECT, allowlist_bloom_filter,
      kDefaultHostBloomFilterNumHashFunctions, kDefaultHostBloomFilterNumBits,
      /*is_allowlist=*/true, &config);
  ProcessHints(config, "1.0.0.0");

  // Set to online so fetch is activated.
  SetConnectionOnline();
  auto navigation_data = CreateTestNavigationData(
      url_without_hints(), {optimization_guide::proto::LITE_PAGE_REDIRECT});
  base::HistogramTester histogram_tester;
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.UrlCount", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus", 0);
}

TEST_F(HintsManagerFetchingTest, HintsFetchedAtNavigationTime) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  InitializeWithDefaultConfig("1.0.0.0");

  // Set to online so fetch is activated.
  SetConnectionOnline();
  auto navigation_data = CreateTestNavigationData(
      url_without_hints(), {optimization_guide::proto::DEFER_ALL_SCRIPT});
  base::HistogramTester histogram_tester;
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
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

TEST_F(HintsManagerFetchingTest,
       HintsFetchedAtNavigationTime_FetchNotAttempted) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  InitializeWithDefaultConfig("1.0.0.0");

  // Set to online so fetch is activated.
  SetConnectionOnline();
  auto navigation_data = CreateTestNavigationData(
      url_without_hints(), {optimization_guide::proto::DEFER_ALL_SCRIPT});
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory({HintsFetcherEndState::kFetchFailed}));
  base::HistogramTester histogram_tester;
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.UrlCount", 0);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
      optimization_guide::RaceNavigationFetchAttemptStatus::
          kRaceNavigationFetchNotAttempted,
      1);
}

TEST_F(HintsManagerFetchingTest,
       HintsFetchedAtNavigationTime_HasComponentHintButNotFetched) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  InitializeWithDefaultConfig("1.0.0.0");
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));

  // Set to online so fetch is activated.
  SetConnectionOnline();
  auto navigation_data = CreateTestNavigationData(
      url_with_hints(), {optimization_guide::proto::DEFER_ALL_SCRIPT});
  base::HistogramTester histogram_tester;
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 0);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
      optimization_guide::RaceNavigationFetchAttemptStatus::
          kRaceNavigationFetchURL,
      1);
}

TEST_F(HintsManagerFetchingTest, URLHintsNotFetchedAtNavigationTime) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  InitializeWithDefaultConfig("1.0.0.0");
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));

  // Set to online so fetch is activated.
  SetConnectionOnline();

  {
    base::HistogramTester histogram_tester;
    auto navigation_data = CreateTestNavigationData(
        url_with_hints(), {optimization_guide::proto::DEFER_ALL_SCRIPT});

    CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
    RunUntilIdle();

    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 0);

    // Make sure navigation data is populated correctly.
    EXPECT_TRUE(navigation_data->hints_fetch_latency().has_value());
    EXPECT_EQ(navigation_data->hints_fetch_attempt_status(),
              optimization_guide::RaceNavigationFetchAttemptStatus::
                  kRaceNavigationFetchURL);

    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
        optimization_guide::RaceNavigationFetchAttemptStatus::
            kRaceNavigationFetchURL,
        1);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.HintsManager."
        "PageNavigationHintsReturnedBeforeDataFlushed",
        true, 1);
    RunUntilIdle();
  }

  {
    base::HistogramTester histogram_tester;
    auto navigation_data = CreateTestNavigationData(
        url_with_hints(), {optimization_guide::proto::DEFER_ALL_SCRIPT});
    CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
    RunUntilIdle();

    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
        optimization_guide::RaceNavigationFetchAttemptStatus::
            kRaceNavigationFetchNotAttempted,
        1);

    // Make sure navigation data is populated correctly.
    EXPECT_FALSE(navigation_data->hints_fetch_latency().has_value());
    EXPECT_EQ(navigation_data->hints_fetch_attempt_status(),
              optimization_guide::RaceNavigationFetchAttemptStatus::
                  kRaceNavigationFetchNotAttempted);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.HintsManager."
        "PageNavigationHintsReturnedBeforeDataFlushed",
        0);
  }
}

TEST_F(HintsManagerFetchingTest, URLWithNoHintsNotRefetchedAtNavigationTime) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  InitializeWithDefaultConfig("1.0.0.0");
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithHostHints}));

  // Set to online so fetch is activated.
  SetConnectionOnline();

  base::HistogramTester histogram_tester;
  {
    auto navigation_data = CreateTestNavigationData(
        url_without_hints(), {optimization_guide::proto::DEFER_ALL_SCRIPT});

    CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
    RunUntilIdle();
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 0);

    // Make sure navigation data is populated correctly.
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
    auto navigation_data = CreateTestNavigationData(
        url_without_hints(), {optimization_guide::proto::DEFER_ALL_SCRIPT});
    base::RunLoop run_loop;
    navigation_data = CreateTestNavigationData(
        url_without_hints(), {optimization_guide::proto::DEFER_ALL_SCRIPT});
    CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
    RunUntilIdle();

    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
        optimization_guide::RaceNavigationFetchAttemptStatus::
            kRaceNavigationFetchHost,
        1);
    EXPECT_TRUE(navigation_data->hints_fetch_latency().has_value());
    EXPECT_EQ(navigation_data->hints_fetch_attempt_status(),
              optimization_guide::RaceNavigationFetchAttemptStatus::
                  kRaceNavigationFetchHost);
  }
}

TEST_F(HintsManagerFetchingTest, CanApplyOptimizationCalledMidFetch) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  InitializeWithDefaultConfig("1.0.0.0");

  // Set to online so fetch is activated.
  SetConnectionOnline();
  auto navigation_data = CreateTestNavigationData(
      url_without_hints(), {optimization_guide::proto::DEFER_ALL_SCRIPT});
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_data->navigation_url(), /*navigation_id=*/absl::nullopt,
          optimization_guide::proto::DEFER_ALL_SCRIPT,
          /*optimization_metadata=*/nullptr);

  EXPECT_EQ(optimization_type_decision,
            optimization_guide::OptimizationTypeDecision::
                kHintFetchStartedButNotAvailableInTime);
}

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationCalledPostFetchButNoHintsCameBack) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithNoHints}));

  // Set to online so fetch is activated.
  SetConnectionOnline();
  auto navigation_data = CreateTestNavigationData(
      url_without_hints(), {optimization_guide::proto::DEFER_ALL_SCRIPT});
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  RunUntilIdle();

  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_data->navigation_url(), /*navigation_id=*/absl::nullopt,
          optimization_guide::proto::DEFER_ALL_SCRIPT,
          /*optimization_metadata=*/nullptr);

  EXPECT_EQ(optimization_type_decision,
            optimization_guide::OptimizationTypeDecision::kNoHintAvailable);
}

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationCalledPostFetchButFetchFailed) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory({HintsFetcherEndState::kFetchFailed}));

  // Set to online so fetch is activated.
  SetConnectionOnline();
  auto navigation_data = CreateTestNavigationData(
      url_without_hints(), {optimization_guide::proto::DEFER_ALL_SCRIPT});
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  RunUntilIdle();

  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_data->navigation_url(), /*navigation_id=*/absl::nullopt,
          optimization_guide::proto::DEFER_ALL_SCRIPT,
          /*optimization_metadata=*/nullptr);

  EXPECT_EQ(optimization_type_decision,
            optimization_guide::OptimizationTypeDecision::kNoHintAvailable);
}

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationWithURLKeyedHintApplicableForOptimizationType) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});
  InitializeWithDefaultConfig("1.0.0");

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));

  // Set to online so fetch is activated.
  SetConnectionOnline();
  auto navigation_data = CreateTestNavigationData(
      url_with_url_keyed_hint(), {optimization_guide::proto::DEFER_ALL_SCRIPT});
  // Make sure URL-keyed hint is fetched and processed.
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  RunUntilIdle();

  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_data->navigation_url(), /*navigation_id=*/absl::nullopt,
          optimization_guide::proto::COMPRESS_PUBLIC_IMAGES,
          &optimization_metadata);

  // Make sure decisions are logged correctly and metadata is populated off
  // a URL-keyed hint.
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kAllowedByHint,
            optimization_type_decision);
  EXPECT_TRUE(optimization_metadata.public_image_metadata().has_value());
}

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationNotAllowedByURLButAllowedByHostKeyedHint) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::NOSCRIPT});

  InitializeWithDefaultConfig("1.0.0.0");

  // Set to online so fetch is activated.
  SetConnectionOnline();

  // Make sure both URL-Keyed and host-keyed hints are processed and cached.
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));
  auto navigation_data = CreateTestNavigationData(
      url_with_url_keyed_hint(), {optimization_guide::proto::NOSCRIPT});
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  RunUntilIdle();

  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_data->navigation_url(), /*navigation_id=*/absl::nullopt,
          optimization_guide::proto::NOSCRIPT, &optimization_metadata);

  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kAllowedByHint,
            optimization_type_decision);
}

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationNotAllowedByURLOrHostKeyedHint) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::RESOURCE_LOADING});

  InitializeWithDefaultConfig("1.0.0.0");

  // Set to online so fetch is activated.
  SetConnectionOnline();

  // Make sure both URL-Keyed and host-keyed hints are processed and cached.
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));
  auto navigation_data = CreateTestNavigationData(
      url_with_url_keyed_hint(), {optimization_guide::proto::RESOURCE_LOADING});
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  RunUntilIdle();

  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_data->navigation_url(), /*navigation_id=*/absl::nullopt,
          optimization_guide::proto::RESOURCE_LOADING, &optimization_metadata);

  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kNotAllowedByHint,
            optimization_type_decision);
}

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationNoURLKeyedHintOrHostKeyedHint) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});

  InitializeWithDefaultConfig("1.0.0.0");

  // Set to online so fetch is activated.
  SetConnectionOnline();

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithNoHints}));
  auto navigation_data = CreateTestNavigationData(
      url_without_hints(), {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});

  // Attempt to fetch a hint but ensure nothing comes back.
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  RunUntilIdle();

  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_data->navigation_url(), /*navigation_id=*/absl::nullopt,
          optimization_guide::proto::COMPRESS_PUBLIC_IMAGES,
          &optimization_metadata);

  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kNoHintAvailable,
            optimization_type_decision);
}

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationCalledMidFetchForURLKeyedOptimization) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});

  InitializeWithDefaultConfig("1.0.0.0");

  // Set to online so fetch is activated.
  SetConnectionOnline();

  // Attempt to fetch a hint but call CanApplyOptimization right away to
  // simulate being mid-fetch.
  auto navigation_data = CreateTestNavigationData(
      url_without_hints(), {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithHostHints}));
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_data->navigation_url(), /*navigation_id=*/absl::nullopt,
          optimization_guide::proto::COMPRESS_PUBLIC_IMAGES,
          &optimization_metadata);

  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::
                kHintFetchStartedButNotAvailableInTime,
            optimization_type_decision);
}

TEST_F(HintsManagerFetchingTest,
       OnNavigationStartOrRedirectWontInitiateFetchIfAlreadyStartedForTheURL) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::RESOURCE_LOADING});

  InitializeWithDefaultConfig("1.0.0.0");

  // Set to online so fetch is activated.
  SetConnectionOnline();

  // Attempt to fetch a hint but initiate the next navigation right away to
  // simulate being mid-fetch.
  auto navigation_data = CreateTestNavigationData(
      url_without_hints(), {optimization_guide::proto::RESOURCE_LOADING});
  {
    base::HistogramTester histogram_tester;
    hints_manager()->SetHintsFetcherFactoryForTesting(
        BuildTestHintsFetcherFactory(
            {HintsFetcherEndState::kFetchSuccessWithHostHints}));
    CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
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
    CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
        optimization_guide::RaceNavigationFetchAttemptStatus::
            kRaceNavigationFetchAlreadyInProgress,
        1);
    // Should not be recorded since we are not attempting a new fetch.
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.HintsManager.ConcurrentPageNavigationFetches", 0);

    // Set hints fetch end.so we can figure out if hints fetch start was set.
    navigation_data->set_hints_fetch_end(base::TimeTicks::Now());
    EXPECT_TRUE(navigation_data->hints_fetch_latency().has_value());
    EXPECT_EQ(navigation_data->hints_fetch_attempt_status(),
              optimization_guide::RaceNavigationFetchAttemptStatus::
                  kRaceNavigationFetchAlreadyInProgress);
  }
}

TEST_F(HintsManagerFetchingTest,
       PageNavigationHintsFetcherGetsCleanedUpOnceHintsAreStored) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::RESOURCE_LOADING});

  InitializeWithDefaultConfig("1.0.0.0");

  // Set to online so fetch is activated.
  SetConnectionOnline();
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithNoHints}));

  // Attempt to fetch a hint but initiate the next navigation right away to
  // simulate being mid-fetch.
  auto navigation_data = CreateTestNavigationData(
      url_without_hints(), {optimization_guide::proto::RESOURCE_LOADING});
  {
    base::HistogramTester histogram_tester;
    CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
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
    CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
        optimization_guide::RaceNavigationFetchAttemptStatus::
            kRaceNavigationFetchHost,
        1);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.HintsManager.ConcurrentPageNavigationFetches", 1, 1);
  }
}

TEST_F(HintsManagerFetchingTest,
       PageNavigationHintsFetcherCanFetchMultipleThingsConcurrently) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});

  InitializeWithDefaultConfig("1.0.0.0");

  // Set to online so fetch is activated.
  SetConnectionOnline();
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));

  auto navigation_data_with_hints = CreateTestNavigationData(
      url_with_url_keyed_hint(),
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});
  auto navigation_data_without_hints = CreateTestNavigationData(
      GURL("https://doesntmatter.com/"),
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});
  auto navigation_data_without_hints2 = CreateTestNavigationData(
      url_without_hints(), {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});

  // Attempt to fetch a hint but initiate the next navigations right away to
  // simulate being mid-fetch.
  base::HistogramTester histogram_tester;
  CallOnNavigationStartOrRedirect(navigation_data_with_hints.get(),
                                  base::DoNothing());
  CallOnNavigationStartOrRedirect(navigation_data_without_hints.get(),
                                  base::DoNothing());
  CallOnNavigationStartOrRedirect(navigation_data_without_hints2.get(),
                                  base::DoNothing());

  // The third one is over the max and should evict another one.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsManager.ConcurrentPageNavigationFetches", 3);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintsManager.ConcurrentPageNavigationFetches", 1, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintsManager.ConcurrentPageNavigationFetches", 2, 2);
}

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationAsyncDecisionComesFromInFlightURLHint) {
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});
  InitializeWithDefaultConfig("1.0.0.0");

  // Set to online so fetch is activated.
  SetConnectionOnline();

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));
  auto navigation_data = CreateTestNavigationData(
      url_with_url_keyed_hint(),
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  hints_manager()->CanApplyOptimizationAsync(
      url_with_url_keyed_hint(), navigation_data->navigation_id(),
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

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationAsyncMultipleCallbacksRegisteredForSameTypeAndURL) {
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});
  InitializeWithDefaultConfig("1.0.0.0");

  // Set to online so fetch is activated.
  SetConnectionOnline();

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));
  auto navigation_data = CreateTestNavigationData(
      url_with_url_keyed_hint(),
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});
  hints_manager()->CanApplyOptimizationAsync(
      url_with_url_keyed_hint(), navigation_data->navigation_id(),
      optimization_guide::proto::COMPRESS_PUBLIC_IMAGES,
      base::BindOnce(
          [](optimization_guide::OptimizationGuideDecision decision,
             const optimization_guide::OptimizationMetadata& metadata) {
            EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kTrue,
                      decision);
            EXPECT_TRUE(metadata.public_image_metadata().has_value());
          }));
  hints_manager()->CanApplyOptimizationAsync(
      url_with_url_keyed_hint(), navigation_data->navigation_id(),
      optimization_guide::proto::COMPRESS_PUBLIC_IMAGES,
      base::BindOnce(
          [](optimization_guide::OptimizationGuideDecision decision,
             const optimization_guide::OptimizationMetadata& metadata) {
            EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kTrue,
                      decision);
            EXPECT_TRUE(metadata.public_image_metadata().has_value());
          }));
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecisionAsync.CompressPublicImages",
      optimization_guide::OptimizationTypeDecision::kAllowedByHint, 2);
}

TEST_F(
    HintsManagerFetchingTest,
    CanApplyOptimizationAsyncDecisionComesFromInFlightURLHintNotAllowlisted) {
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::RESOURCE_LOADING});
  InitializeWithDefaultConfig("1.0.0.0");

  // Set to online so fetch is activated.
  SetConnectionOnline();

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));
  auto navigation_data = CreateTestNavigationData(
      url_with_url_keyed_hint(), {optimization_guide::proto::RESOURCE_LOADING});
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  hints_manager()->CanApplyOptimizationAsync(
      url_with_url_keyed_hint(), navigation_data->navigation_id(),
      optimization_guide::proto::RESOURCE_LOADING,
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

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationAsyncFetchFailsDoesNotStrandCallbacks) {
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});

  InitializeWithDefaultConfig("1.0.0.0");

  // Set to online so fetch is activated.
  SetConnectionOnline();

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory({HintsFetcherEndState::kFetchFailed}));
  auto navigation_data = CreateTestNavigationData(
      url_with_url_keyed_hint(),
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});
  hints_manager()->CanApplyOptimizationAsync(
      url_with_url_keyed_hint(), navigation_data->navigation_id(),
      optimization_guide::proto::COMPRESS_PUBLIC_IMAGES,
      base::BindOnce(
          [](optimization_guide::OptimizationGuideDecision decision,
             const optimization_guide::OptimizationMetadata& metadata) {
            EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kFalse,
                      decision);
          }));
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecisionAsync.CompressPublicImages",
      optimization_guide::OptimizationTypeDecision::kNotAllowedByHint, 1);
}

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationAsyncInfoAlreadyInPriorToCall) {
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});

  InitializeWithDefaultConfig("1.0.0.0");

  // Set to online so fetch is activated.
  SetConnectionOnline();

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));
  auto navigation_data = CreateTestNavigationData(
      url_with_url_keyed_hint(),
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  RunUntilIdle();

  hints_manager()->CanApplyOptimizationAsync(
      url_with_url_keyed_hint(), navigation_data->navigation_id(),
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

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationAsyncInfoAlreadyInPriorToCallAndNotAllowlisted) {
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::PERFORMANCE_HINTS});

  InitializeWithDefaultConfig("1.0.0.0");

  // Set to online so fetch is activated.
  SetConnectionOnline();

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));
  auto navigation_data =
      CreateTestNavigationData(url_with_url_keyed_hint(),
                               {optimization_guide::proto::PERFORMANCE_HINTS});
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  RunUntilIdle();

  hints_manager()->CanApplyOptimizationAsync(
      url_with_url_keyed_hint(), navigation_data->navigation_id(),
      optimization_guide::proto::PERFORMANCE_HINTS,
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

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationAsyncHintComesInAndNotAllowlisted) {
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::PERFORMANCE_HINTS});

  InitializeWithDefaultConfig("1.0.0.0");

  // Set to online so fetch is activated.
  SetConnectionOnline();

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithNoHints}));
  auto navigation_data = CreateTestNavigationData(
      url_without_hints(), {optimization_guide::proto::PERFORMANCE_HINTS});
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  hints_manager()->CanApplyOptimizationAsync(
      url_without_hints(), navigation_data->navigation_id(),
      optimization_guide::proto::PERFORMANCE_HINTS,
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

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationAsyncDoesNotStrandCallbacksAtBeginningOfChain) {
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});

  InitializeWithDefaultConfig("1.0.0.0");

  // Set to offline so fetch is NOT activated.
  SetConnectionOffline();

  GURL url_that_redirected("https://urlthatredirected.com");
  auto navigation_data_redirect = CreateTestNavigationData(
      url_that_redirected, {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});
  hints_manager()->CanApplyOptimizationAsync(
      url_that_redirected, navigation_data_redirect->navigation_id(),
      optimization_guide::proto::COMPRESS_PUBLIC_IMAGES,
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

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationAsyncDoesNotStrandCallbacksIfFetchNotPending) {
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});

  InitializeWithDefaultConfig("1.0.0.0");

  // Set to offline so fetch is NOT activated.
  SetConnectionOffline();

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithNoHints}));
  auto navigation_data = CreateTestNavigationData(
      url_with_url_keyed_hint(),
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  hints_manager()->CanApplyOptimizationAsync(
      url_with_url_keyed_hint(), navigation_data->navigation_id(),
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

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationAsyncWithDecisionFromAllowlistReturnsRightAway) {
  base::HistogramTester histogram_tester;

  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LITE_PAGE_REDIRECT});

  optimization_guide::proto::Configuration config;
  optimization_guide::BloomFilter allowlist_bloom_filter(
      kDefaultHostBloomFilterNumHashFunctions, kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&allowlist_bloom_filter);
  AddBloomFilterToConfig(
      optimization_guide::proto::LITE_PAGE_REDIRECT, allowlist_bloom_filter,
      kDefaultHostBloomFilterNumHashFunctions, kDefaultHostBloomFilterNumBits,
      /*is_allowlist=*/true, &config);
  ProcessHints(config, "1.0.0.0");

  auto navigation_data =
      CreateTestNavigationData(GURL("https://notallowed.com/123"),
                               {optimization_guide::proto::LITE_PAGE_REDIRECT});
  hints_manager()->CanApplyOptimizationAsync(
      navigation_data->navigation_url(), navigation_data->navigation_id(),
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

TEST_F(HintsManagerFetchingTest,
       CanApplyOptimizationAsyncWithDecisionFromBlocklistReturnsRightAway) {
  base::HistogramTester histogram_tester;

  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LITE_PAGE_REDIRECT});

  optimization_guide::proto::Configuration config;
  optimization_guide::BloomFilter blocklist_bloom_filter(
      kDefaultHostBloomFilterNumHashFunctions, kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&blocklist_bloom_filter);
  AddBloomFilterToConfig(
      optimization_guide::proto::LITE_PAGE_REDIRECT, blocklist_bloom_filter,
      kDefaultHostBloomFilterNumHashFunctions, kDefaultHostBloomFilterNumBits,
      /*is_allowlist=*/false, &config);
  ProcessHints(config, "1.0.0.0");

  auto navigation_data =
      CreateTestNavigationData(GURL("https://m.host.com/123"),
                               {optimization_guide::proto::LITE_PAGE_REDIRECT});
  hints_manager()->CanApplyOptimizationAsync(
      navigation_data->navigation_url(), navigation_data->navigation_id(),
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

TEST_F(HintsManagerFetchingTest,
       OnNavigationFinishDoesNotPrematurelyInvokeRegisteredCallbacks) {
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});

  InitializeWithDefaultConfig("1.0.0.0");

  // Set to online so fetch is activated.
  SetConnectionOnline();

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithURLHints}));
  auto navigation_data =
      CreateTestNavigationData(url_with_url_keyed_hint(),
                               {optimization_guide::proto::LITE_PAGE_REDIRECT});
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  hints_manager()->CanApplyOptimizationAsync(
      url_with_url_keyed_hint(), navigation_data->navigation_id(),
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

TEST_F(HintsManagerFetchingTest,
       OnNavigationFinishDoesNotCrashWithoutAnyCallbacksRegistered) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});

  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->OnNavigationFinish({url_with_url_keyed_hint()});

  RunUntilIdle();
}

TEST_F(HintsManagerFetchingTest, NewOptTypeRegisteredClearsHintCache) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});

  InitializeWithDefaultConfig("1.0.0.0");

  GURL url("https://host.com/fetched_hint_host");

  // Set to online so fetch is activated.
  SetConnectionOnline();

  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithHostHints}));
  auto navigation_data = CreateTestNavigationData(
      url, {optimization_guide::proto::DEFER_ALL_SCRIPT});

  // Attempt to fetch a hint but ensure nothing comes back.
  CallOnNavigationStartOrRedirect(navigation_data.get(), base::DoNothing());
  RunUntilIdle();

  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(
          navigation_data->navigation_url(), /*navigation_id=*/absl::nullopt,
          optimization_guide::proto::DEFER_ALL_SCRIPT, &optimization_metadata);

  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kNotAllowedByHint,
            optimization_type_decision);

  // Register a new type that is unlaunched - this should clear the Fetched
  // hints.
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});

  RunUntilIdle();

  base::RunLoop run_loop;

  // Set to offline so fetch is NOT activated, so the cache state is known and
  // empty.
  SetConnectionOffline();

  base::HistogramTester histogram_tester;

  navigation_data = CreateTestNavigationData(
      url, {optimization_guide::proto::DEFER_ALL_SCRIPT});
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());

  run_loop.Run();

  optimization_type_decision = hints_manager()->CanApplyOptimization(
      navigation_data->navigation_url(), /*navigation_id=*/absl::nullopt,
      optimization_guide::proto::DEFER_ALL_SCRIPT, &optimization_metadata);

  // The previously fetched hints for the host should not be available after
  // registering a new optimization type.
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
      optimization_guide::RaceNavigationFetchAttemptStatus::
          kRaceNavigationFetchHost,
      1);
}

class HintsManagerFetchingNoBatchUpdateTest : public HintsManagerTest {
 public:
  HintsManagerFetchingNoBatchUpdateTest() {
    scoped_list_.InitAndEnableFeatureWithParameters(
        optimization_guide::features::kRemoteOptimizationGuideFetching,
        {{"batch_update_hints_for_top_hosts", "false"}});
  }

 private:
  base::test::ScopedFeatureList scoped_list_;
};

TEST_F(HintsManagerFetchingNoBatchUpdateTest,
       BatchUpdateHintsFetchNotScheduledIfNotAllowed) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  std::unique_ptr<FakeTopHostProvider> top_host_provider =
      std::make_unique<FakeTopHostProvider>(
          std::vector<std::string>({"example1.com", "example2.com"}));

  // Force hints fetch scheduling.
  CreateHintsManager(top_host_provider.get());
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  hints_manager()->SetHintsFetcherFactoryForTesting(
      BuildTestHintsFetcherFactory(
          {HintsFetcherEndState::kFetchSuccessWithHostHints}));
  InitializeWithDefaultConfig("1.0.0");

  // Force timer to expire and schedule a hints fetch.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kUpdateFetchHintsTimeSecs));
  // Hints fetcher should not even be created.
  EXPECT_FALSE(batch_update_hints_fetcher());
}

}  // namespace optimization_guide
