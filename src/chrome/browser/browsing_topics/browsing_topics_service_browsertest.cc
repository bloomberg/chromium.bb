// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_file_value_serializer.h"
#include "base/json/values_util.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browsing_topics/browsing_topics_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/optimization_guide/page_content_annotations_service_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/browsing_topics/browsing_topics_service.h"
#include "components/browsing_topics/browsing_topics_service_impl.h"
#include "components/browsing_topics/epoch_topics.h"
#include "components/browsing_topics/test_util.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/optimization_guide/content/browser/page_content_annotations_service.h"
#include "components/optimization_guide/content/browser/test_page_content_annotator.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/browsing_topics_site_data_manager.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browsing_topics_test_util.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"

namespace browsing_topics {

namespace {

constexpr browsing_topics::HmacKey kTestKey = {1};

constexpr base::Time kTime1 =
    base::Time::FromDeltaSinceWindowsEpoch(base::Days(1));
constexpr base::Time kTime2 =
    base::Time::FromDeltaSinceWindowsEpoch(base::Days(2));

constexpr size_t kTaxonomySize = 349;
constexpr int kTaxonomyVersion = 1;
constexpr int64_t kModelVersion = 2;
constexpr size_t kPaddedTopTopicsStartIndex = 5;
constexpr Topic kExpectedTopic1 = Topic(1);
constexpr Topic kExpectedTopic2 = Topic(10);
constexpr char kExpectedResultOrder1[] =
    "[{\"configVersion\":\"chrome.1\",\"modelVersion\":\"2\","
    "\"taxonomyVersion\":\"1\",\"topic\":1,\"version\":\"chrome.1:1:2\"};{"
    "\"configVersion\":\"chrome.1\",\"modelVersion\":\"2\","
    "\"taxonomyVersion\":\"1\",\"topic\":10,\"version\":\"chrome.1:1:2\"};]";

constexpr char kExpectedResultOrder2[] =
    "[{\"configVersion\":\"chrome.1\",\"modelVersion\":\"2\","
    "\"taxonomyVersion\":\"1\",\"topic\":10,\"version\":\"chrome.1:1:2\"};{"
    "\"configVersion\":\"chrome.1\",\"modelVersion\":\"2\","
    "\"taxonomyVersion\":\"1\",\"topic\":1,\"version\":\"chrome.1:1:2\"};]";

EpochTopics CreateTestEpochTopics(
    const std::vector<std::pair<Topic, std::set<HashedDomain>>>& topics,
    base::Time calculation_time) {
  DCHECK_EQ(topics.size(), 5u);

  std::vector<TopicAndDomains> top_topics_and_observing_domains;
  for (size_t i = 0; i < 5; ++i) {
    top_topics_and_observing_domains.emplace_back(topics[i].first,
                                                  topics[i].second);
  }

  return EpochTopics(std::move(top_topics_and_observing_domains),
                     kPaddedTopTopicsStartIndex, kTaxonomySize,
                     kTaxonomyVersion, kModelVersion, calculation_time);
}

}  // namespace

// A tester class that allows waiting for the first calculation to finish.
class TesterBrowsingTopicsService : public BrowsingTopicsServiceImpl {
 public:
  TesterBrowsingTopicsService(
      const base::FilePath& profile_path,
      privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
      history::HistoryService* history_service,
      content::BrowsingTopicsSiteDataManager* site_data_manager,
      optimization_guide::PageContentAnnotationsService* annotations_service,
      base::OnceClosure calculation_finish_callback)
      : BrowsingTopicsServiceImpl(profile_path,
                                  privacy_sandbox_settings,
                                  history_service,
                                  site_data_manager,
                                  annotations_service),
        calculation_finish_callback_(std::move(calculation_finish_callback)) {}

  ~TesterBrowsingTopicsService() override = default;

  TesterBrowsingTopicsService(const TesterBrowsingTopicsService&) = delete;
  TesterBrowsingTopicsService& operator=(const TesterBrowsingTopicsService&) =
      delete;
  TesterBrowsingTopicsService(TesterBrowsingTopicsService&&) = delete;
  TesterBrowsingTopicsService& operator=(TesterBrowsingTopicsService&&) =
      delete;

  const BrowsingTopicsState& browsing_topics_state() override {
    return BrowsingTopicsServiceImpl::browsing_topics_state();
  }

  void OnCalculateBrowsingTopicsCompleted(EpochTopics epoch_topics) override {
    BrowsingTopicsServiceImpl::OnCalculateBrowsingTopicsCompleted(
        std::move(epoch_topics));

    if (calculation_finish_callback_)
      std::move(calculation_finish_callback_).Run();
  }

 private:
  base::OnceClosure calculation_finish_callback_;
};

class BrowsingTopicsBrowserTestBase : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());

    content::SetupCrossSiteRedirector(&https_server_);
    ASSERT_TRUE(https_server_.Start());

    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  ~BrowsingTopicsBrowserTestBase() override = default;

  std::string InvokeTopicsAPI(const content::ToRenderFrameHost& adapter) {
    return EvalJs(adapter, R"(
      if (!(document.browsingTopics instanceof Function)) {
        'not a function';
      } else {
        document.browsingTopics()
        .then(topics => {
          let result = "[";
          for (const topic of topics) {
            result += JSON.stringify(topic, Object.keys(topic).sort()) + ";"
          }
          result += "]";
          return result;
        })
        .catch(error => error.message);
      }
    )")
        .ExtractString();
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  net::EmbeddedTestServer https_server_{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS};
};

class BrowsingTopicsDisabledBrowserTest : public BrowsingTopicsBrowserTestBase {
 public:
  BrowsingTopicsDisabledBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{blink::features::kBrowsingTopics});
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(BrowsingTopicsDisabledBrowserTest,
                       NoBrowsingTopicsService) {
  EXPECT_FALSE(
      BrowsingTopicsServiceFactory::GetForProfile(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsDisabledBrowserTest, NoTopicsAPI) {
  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/empty_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  EXPECT_EQ("not a function", InvokeTopicsAPI(web_contents()));
}

class BrowsingTopicsBrowserTest : public BrowsingTopicsBrowserTestBase {
 public:
  BrowsingTopicsBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {blink::features::kBrowsingTopics,
         blink::features::kBrowsingTopicsBypassIPIsPubliclyRoutableCheck,
         features::kPrivacySandboxAdsAPIsOverride},
        /*disabled_features=*/{});
  }

  ~BrowsingTopicsBrowserTest() override = default;

  void SetUpOnMainThread() override {
    BrowsingTopicsBrowserTestBase::SetUpOnMainThread();

    for (auto& profile_and_calculation_finish_waiter :
         calculation_finish_waiters_) {
      profile_and_calculation_finish_waiter.second->Run();
    }
  }

  // BrowserTestBase::SetUpInProcessBrowserTestFixture
  void SetUpInProcessBrowserTestFixture() override {
    subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &BrowsingTopicsBrowserTest::OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

 protected:
  void ExpectResultTopicsEqual(
      const std::vector<TopicAndDomains>& result,
      std::vector<std::pair<Topic, std::set<HashedDomain>>> expected) {
    DCHECK_EQ(expected.size(), 5u);
    EXPECT_EQ(result.size(), 5u);

    for (int i = 0; i < 5; ++i) {
      EXPECT_EQ(result[i].topic(), expected[i].first);
      EXPECT_EQ(result[i].hashed_domains(), expected[i].second);
    }
  }

  HashedDomain GetHashedDomain(const std::string& domain) {
    return HashContextDomainForStorage(kTestKey, domain);
  }

  void CreateBrowsingTopicsStateFile(
      const base::FilePath& profile_path,
      const std::vector<EpochTopics>& epochs,
      base::Time next_scheduled_calculation_time) {
    base::Value::List epochs_list;
    for (const EpochTopics& epoch : epochs) {
      epochs_list.Append(epoch.ToDictValue());
    }

    base::Value::Dict dict;
    dict.Set("epochs", std::move(epochs_list));
    dict.Set("next_scheduled_calculation_time",
             base::TimeToValue(next_scheduled_calculation_time));
    dict.Set("hex_encoded_hmac_key", base::HexEncode(kTestKey));
    dict.Set("config_version", 1);

    JSONFileValueSerializer(
        profile_path.Append(FILE_PATH_LITERAL("BrowsingTopicsState")))
        .Serialize(dict);
  }

  content::BrowsingTopicsSiteDataManager* browsing_topics_site_data_manager() {
    return browser()
        ->profile()
        ->GetDefaultStoragePartition()
        ->GetBrowsingTopicsSiteDataManager();
  }

  TesterBrowsingTopicsService* browsing_topics_service() {
    return static_cast<TesterBrowsingTopicsService*>(
        BrowsingTopicsServiceFactory::GetForProfile(browser()->profile()));
  }

  const BrowsingTopicsState& browsing_topics_state() {
    return browsing_topics_service()->browsing_topics_state();
  }

  std::vector<optimization_guide::WeightedIdentifier> TopicsAndWeight(
      const std::vector<int32_t>& topics,
      double weight) {
    std::vector<optimization_guide::WeightedIdentifier> result;
    for (int32_t topic : topics) {
      result.emplace_back(topic, weight);
    }

    return result;
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    PageContentAnnotationsServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(
            &BrowsingTopicsBrowserTest::CreatePageContentAnnotationsService,
            base::Unretained(this)));

    browsing_topics::BrowsingTopicsServiceFactory::GetInstance()
        ->SetTestingFactory(
            context,
            base::BindRepeating(
                &BrowsingTopicsBrowserTest::CreateBrowsingTopicsService,
                base::Unretained(this)));
  }

  std::unique_ptr<KeyedService> CreatePageContentAnnotationsService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);

    history::HistoryService* history_service =
        HistoryServiceFactory::GetForProfile(
            profile, ServiceAccessType::IMPLICIT_ACCESS);

    DCHECK(!base::Contains(optimization_guide_model_providers_, profile));
    optimization_guide_model_providers_.emplace(
        profile, std::make_unique<
                     optimization_guide::TestOptimizationGuideModelProvider>());

    auto page_content_annotations_service =
        std::make_unique<optimization_guide::PageContentAnnotationsService>(
            "en-US", optimization_guide_model_providers_.at(profile).get(),
            history_service, nullptr, base::FilePath(), nullptr);

    page_content_annotations_service->OverridePageContentAnnotatorForTesting(
        &test_page_content_annotator_);

    return page_content_annotations_service;
  }

  void InitializePreexistingState(
      history::HistoryService* history_service,
      content::BrowsingTopicsSiteDataManager* site_data_manager,
      const base::FilePath& profile_path) {
    // Configure the (mock) model.
    test_page_content_annotator_.UsePageTopics(
        *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build(),
        {{"foo6 com", TopicsAndWeight({1, 2, 3, 4, 5, 6}, 0.1)},
         {"foo5 com", TopicsAndWeight({2, 3, 4, 5, 6}, 0.1)},
         {"foo4 com", TopicsAndWeight({3, 4, 5, 6}, 0.1)},
         {"foo3 com", TopicsAndWeight({4, 5, 6}, 0.1)},
         {"foo2 com", TopicsAndWeight({5, 6}, 0.1)},
         {"foo1 com", TopicsAndWeight({6}, 0.1)}});

    // Add some initial history.
    history::HistoryAddPageArgs add_page_args;
    add_page_args.time = base::Time::Now();
    add_page_args.context_id = reinterpret_cast<history::ContextID>(1);
    add_page_args.nav_entry_id = 1;

    // Note: foo6.com isn't in the initial history.
    for (int i = 1; i <= 5; ++i) {
      add_page_args.url =
          GURL(base::StrCat({"https://foo", base::NumberToString(i), ".com"}));
      history_service->AddPage(add_page_args);
      history_service->SetBrowsingTopicsAllowed(add_page_args.context_id,
                                                add_page_args.nav_entry_id,
                                                add_page_args.url);
    }

    // Add some API usage contexts data.
    site_data_manager->OnBrowsingTopicsApiUsed(
        HashMainFrameHostForStorage("foo1.com"), {HashedDomain(1)},
        base::Time::Now());

    // Initialize the `BrowsingTopicsState`.
    std::vector<EpochTopics> preexisting_epochs;
    preexisting_epochs.push_back(
        CreateTestEpochTopics({{Topic(1), {GetHashedDomain("a.test")}},
                               {Topic(2), {GetHashedDomain("a.test")}},
                               {Topic(3), {GetHashedDomain("a.test")}},
                               {Topic(4), {GetHashedDomain("a.test")}},
                               {Topic(5), {GetHashedDomain("a.test")}}},
                              kTime1));
    preexisting_epochs.push_back(
        CreateTestEpochTopics({{Topic(6), {GetHashedDomain("a.test")}},
                               {Topic(7), {GetHashedDomain("a.test")}},
                               {Topic(8), {GetHashedDomain("a.test")}},
                               {Topic(9), {GetHashedDomain("a.test")}},
                               {Topic(10), {GetHashedDomain("a.test")}}},
                              kTime2));

    CreateBrowsingTopicsStateFile(
        profile_path, std::move(preexisting_epochs),
        /*next_scheduled_calculation_time=*/base::Time::Now() - base::Days(1));
  }

  std::unique_ptr<KeyedService> CreateBrowsingTopicsService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);

    privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings =
        PrivacySandboxSettingsFactory::GetForProfile(profile);

    history::HistoryService* history_service =
        HistoryServiceFactory::GetForProfile(
            profile, ServiceAccessType::IMPLICIT_ACCESS);

    content::BrowsingTopicsSiteDataManager* site_data_manager =
        context->GetDefaultStoragePartition()
            ->GetBrowsingTopicsSiteDataManager();

    optimization_guide::PageContentAnnotationsService* annotations_service =
        PageContentAnnotationsServiceFactory::GetForProfile(profile);

    InitializePreexistingState(history_service, site_data_manager,
                               profile->GetPath());

    DCHECK(!base::Contains(calculation_finish_waiters_, profile));
    calculation_finish_waiters_.emplace(profile,
                                        std::make_unique<base::RunLoop>());

    if (!ukm_recorder_)
      ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();

    return std::make_unique<TesterBrowsingTopicsService>(
        profile->GetPath(), privacy_sandbox_settings, history_service,
        site_data_manager, annotations_service,
        calculation_finish_waiters_.at(profile)->QuitClosure());
  }

  content::test::FencedFrameTestHelper fenced_frame_test_helper_;

  base::test::ScopedFeatureList scoped_feature_list_;

  std::map<
      Profile*,
      std::unique_ptr<optimization_guide::TestOptimizationGuideModelProvider>>
      optimization_guide_model_providers_;

  std::map<Profile*, std::unique_ptr<base::RunLoop>>
      calculation_finish_waiters_;

  optimization_guide::TestPageContentAnnotator test_page_content_annotator_;

  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;

  base::CallbackListSubscription subscription_;
};

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest, HasBrowsingTopicsService) {
  EXPECT_TRUE(browsing_topics_service());
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest, NoServiceInIncognitoMode) {
  CreateIncognitoBrowser(browser()->profile());

  EXPECT_TRUE(browser()->profile()->HasPrimaryOTRProfile());

  Profile* incognito_profile =
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/false);
  EXPECT_TRUE(incognito_profile);

  BrowsingTopicsService* incognito_browsing_topics_service =
      BrowsingTopicsServiceFactory::GetForProfile(incognito_profile);
  EXPECT_FALSE(incognito_browsing_topics_service);
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest, BrowsingTopicsStateOnStart) {
  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/empty_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  base::Time now = base::Time::Now();

  EXPECT_EQ(browsing_topics_state().epochs().size(), 3u);
  EXPECT_EQ(browsing_topics_state().epochs()[0].calculation_time(), kTime1);
  EXPECT_EQ(browsing_topics_state().epochs()[1].calculation_time(), kTime2);
  EXPECT_GT(browsing_topics_state().epochs()[2].calculation_time(),
            now - base::Minutes(1));
  EXPECT_LT(browsing_topics_state().epochs()[2].calculation_time(), now);

  ExpectResultTopicsEqual(
      browsing_topics_state().epochs()[0].top_topics_and_observing_domains(),
      {{Topic(1), {GetHashedDomain("a.test")}},
       {Topic(2), {GetHashedDomain("a.test")}},
       {Topic(3), {GetHashedDomain("a.test")}},
       {Topic(4), {GetHashedDomain("a.test")}},
       {Topic(5), {GetHashedDomain("a.test")}}});

  ExpectResultTopicsEqual(
      browsing_topics_state().epochs()[1].top_topics_and_observing_domains(),
      {{Topic(6), {GetHashedDomain("a.test")}},
       {Topic(7), {GetHashedDomain("a.test")}},
       {Topic(8), {GetHashedDomain("a.test")}},
       {Topic(9), {GetHashedDomain("a.test")}},
       {Topic(10), {GetHashedDomain("a.test")}}});

  ExpectResultTopicsEqual(
      browsing_topics_state().epochs()[2].top_topics_and_observing_domains(),
      {{Topic(6), {HashedDomain(1)}},
       {Topic(5), {}},
       {Topic(4), {}},
       {Topic(3), {}},
       {Topic(2), {}}});

  EXPECT_GT(browsing_topics_state().next_scheduled_calculation_time(),
            now + base::Days(7) - base::Minutes(1));
  EXPECT_LT(browsing_topics_state().next_scheduled_calculation_time(),
            now + base::Days(7));
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest, CalculationResultUkm) {
  auto entries = ukm_recorder_->GetEntriesByName(
      ukm::builders::BrowsingTopics_EpochTopicsCalculationResult::kEntryName);

  // The number of entries should equal the number of profiles, which could be
  // greater than 1 on some platform.
  EXPECT_EQ(optimization_guide_model_providers_.size(), entries.size());

  for (auto* entry : entries) {
    ukm_recorder_->ExpectEntryMetric(
        entry,
        ukm::builders::BrowsingTopics_EpochTopicsCalculationResult::
            kTopTopic0Name,
        6);
    ukm_recorder_->ExpectEntryMetric(
        entry,
        ukm::builders::BrowsingTopics_EpochTopicsCalculationResult::
            kTopTopic1Name,
        5);
    ukm_recorder_->ExpectEntryMetric(
        entry,
        ukm::builders::BrowsingTopics_EpochTopicsCalculationResult::
            kTopTopic2Name,
        4);
    ukm_recorder_->ExpectEntryMetric(
        entry,
        ukm::builders::BrowsingTopics_EpochTopicsCalculationResult::
            kTopTopic3Name,
        3);
    ukm_recorder_->ExpectEntryMetric(
        entry,
        ukm::builders::BrowsingTopics_EpochTopicsCalculationResult::
            kTopTopic4Name,
        2);
    ukm_recorder_->ExpectEntryMetric(
        entry,
        ukm::builders::BrowsingTopics_EpochTopicsCalculationResult::
            kTaxonomyVersionName,
        1);
    ukm_recorder_->ExpectEntryMetric(
        entry,
        ukm::builders::BrowsingTopics_EpochTopicsCalculationResult::
            kModelVersionName,
        1);
    ukm_recorder_->ExpectEntryMetric(
        entry,
        ukm::builders::BrowsingTopics_EpochTopicsCalculationResult::
            kPaddedTopicsStartIndexName,
        5);
  }
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest, ApiResultUkm) {
  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/one_iframe_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  InvokeTopicsAPI(web_contents());

  auto entries = ukm_recorder_->GetEntriesByName(
      ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult::
          kEntryName);
  EXPECT_EQ(1u, entries.size());

  ukm_recorder_->ExpectEntrySourceHasUrl(entries.back(), main_frame_url);

  const int64_t* topic0_metric = ukm_recorder_->GetEntryMetric(
      entries.back(),
      ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult::
          kReturnedTopic0Name);
  const int64_t* topic1_metric = ukm_recorder_->GetEntryMetric(
      entries.back(),
      ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult::
          kReturnedTopic1Name);

  EXPECT_TRUE(topic0_metric);
  EXPECT_TRUE(topic1_metric);

  EXPECT_TRUE((*topic0_metric == kExpectedTopic1.value() &&
               *topic1_metric == kExpectedTopic2.value()) ||
              (*topic0_metric == kExpectedTopic2.value() &&
               *topic1_metric == kExpectedTopic1.value()));

  ukm_recorder_->ExpectEntryMetric(
      entries.back(),
      ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult::
          kReturnedTopic0IsTrueTopTopicName,
      true);
  ukm_recorder_->ExpectEntryMetric(
      entries.back(),
      ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult::
          kReturnedTopic0ModelVersionName,
      2);
  ukm_recorder_->ExpectEntryMetric(
      entries.back(),
      ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult::
          kReturnedTopic0TaxonomyVersionName,
      1);

  ukm_recorder_->ExpectEntryMetric(
      entries.back(),
      ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult::
          kReturnedTopic1IsTrueTopTopicName,
      true);
  ukm_recorder_->ExpectEntryMetric(
      entries.back(),
      ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult::
          kReturnedTopic1ModelVersionName,
      2);
  ukm_recorder_->ExpectEntryMetric(
      entries.back(),
      ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult::
          kReturnedTopic1TaxonomyVersionName,
      1);

  EXPECT_FALSE(ukm_recorder_->GetEntryMetric(
      entries.back(),
      ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult::
          kReturnedTopic2Name));
  EXPECT_FALSE(ukm_recorder_->GetEntryMetric(
      entries.back(),
      ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult::
          kEmptyReasonName));
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest, PageLoadUkm) {
  // The test assumes pages gets deleted after navigation, triggering metrics
  // recording. Disable back/forward cache to ensure that pages don't get
  // preserved in the cache.
  content::DisableBackForwardCacheForTesting(
      web_contents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/one_iframe_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  InvokeTopicsAPI(web_contents());

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  auto entries = ukm_recorder_->GetEntriesByName(
      ukm::builders::BrowsingTopics_PageLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());

  ukm_recorder_->ExpectEntrySourceHasUrl(entries.back(), main_frame_url);

  ukm_recorder_->ExpectEntryMetric(entries.back(),
                                   ukm::builders::BrowsingTopics_PageLoad::
                                       kTopicsRequestingContextDomainsCountName,
                                   1);
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest, GetTopicsForSiteForDisplay) {
  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/empty_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  std::vector<privacy_sandbox::CanonicalTopic> result =
      browsing_topics_service()->GetTopicsForSiteForDisplay(
          web_contents()->GetMainFrame()->GetLastCommittedOrigin());

  // Epoch switch time has not arrived. So expect one topic from each of the
  // first two epochs.
  EXPECT_EQ(result.size(), 2u);
  EXPECT_EQ(result[0].topic_id(), Topic(1));
  EXPECT_EQ(result[0].taxonomy_version(), 1);
  EXPECT_EQ(result[1].topic_id(), Topic(10));
  EXPECT_EQ(result[1].taxonomy_version(), 1);
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest, GetTopTopicsForDisplay) {
  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/empty_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  std::vector<privacy_sandbox::CanonicalTopic> result =
      browsing_topics_service()->GetTopTopicsForDisplay();

  EXPECT_EQ(result.size(), 15u);
  EXPECT_EQ(result[0].topic_id(), Topic(1));
  EXPECT_EQ(result[1].topic_id(), Topic(2));
  EXPECT_EQ(result[2].topic_id(), Topic(3));
  EXPECT_EQ(result[3].topic_id(), Topic(4));
  EXPECT_EQ(result[4].topic_id(), Topic(5));
  EXPECT_EQ(result[5].topic_id(), Topic(6));
  EXPECT_EQ(result[6].topic_id(), Topic(7));
  EXPECT_EQ(result[7].topic_id(), Topic(8));
  EXPECT_EQ(result[8].topic_id(), Topic(9));
  EXPECT_EQ(result[9].topic_id(), Topic(10));
  EXPECT_EQ(result[10].topic_id(), Topic(6));
  EXPECT_EQ(result[11].topic_id(), Topic(5));
  EXPECT_EQ(result[12].topic_id(), Topic(4));
  EXPECT_EQ(result[13].topic_id(), Topic(3));
  EXPECT_EQ(result[14].topic_id(), Topic(2));
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       TopicsAPI_ContextDomainNotFiltered_FromMainFrame) {
  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/one_iframe_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  std::string result = InvokeTopicsAPI(web_contents());

  EXPECT_TRUE(result == kExpectedResultOrder1 ||
              result == kExpectedResultOrder2);

  // Ensure access has been reported to the Page Specific Content Settings.
  auto* pscs = content_settings::PageSpecificContentSettings::GetForPage(
      web_contents()->GetPrimaryPage());
  EXPECT_TRUE(pscs->HasAccessedTopics());
  auto topics = pscs->GetAccessedTopics();
  ASSERT_EQ(2u, topics.size());

  // No ordering is enforced by the PSCS.
  ASSERT_NE(topics[0].topic_id(), topics[1].topic_id());
  ASSERT_TRUE(topics[0].topic_id() == kExpectedTopic1 ||
              topics[0].topic_id() == kExpectedTopic2);
  ASSERT_TRUE(topics[1].topic_id() == kExpectedTopic1 ||
              topics[1].topic_id() == kExpectedTopic2);
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       TopicsAPI_ContextDomainNotFiltered_FromSubframe) {
  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/one_iframe_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  GURL subframe_url =
      https_server_.GetURL("a.test", "/browsing_topics/empty_page.html");

  ASSERT_TRUE(content::NavigateIframeToURL(web_contents(),
                                           /*iframe_id=*/"frame",
                                           subframe_url));

  std::string result =
      InvokeTopicsAPI(content::ChildFrameAt(web_contents()->GetMainFrame(), 0));

  EXPECT_TRUE(result == kExpectedResultOrder1 ||
              result == kExpectedResultOrder2);
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       TopicsAPI_ContextDomainFiltered) {
  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/one_iframe_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  GURL subframe_url =
      https_server_.GetURL("b.test", "/browsing_topics/empty_page.html");

  ASSERT_TRUE(content::NavigateIframeToURL(web_contents(),
                                           /*iframe_id=*/"frame",
                                           subframe_url));

  // b.test has yet to call the API so it shouldn't receive a topic.
  EXPECT_EQ("[]", InvokeTopicsAPI(content::ChildFrameAt(
                      web_contents()->GetMainFrame(), 0)));
  auto* pscs = content_settings::PageSpecificContentSettings::GetForPage(
      web_contents()->GetPrimaryPage());
  EXPECT_FALSE(pscs->HasAccessedTopics());
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       TopicsAPI_ContextDomainTracked) {
  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/one_iframe_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  GURL subframe_url =
      https_server_.GetURL("b.test", "/browsing_topics/empty_page.html");

  ASSERT_TRUE(content::NavigateIframeToURL(web_contents(),
                                           /*iframe_id=*/"frame",
                                           subframe_url));

  // The usage is not tracked before the API call. The returned entry was from
  // the pre-existing storage.
  std::vector<ApiUsageContext> api_usage_contexts =
      content::GetBrowsingTopicsApiUsage(browsing_topics_site_data_manager());
  EXPECT_EQ(api_usage_contexts.size(), 1u);

  EXPECT_EQ("[]", InvokeTopicsAPI(content::ChildFrameAt(
                      web_contents()->GetMainFrame(), 0)));

  api_usage_contexts =
      content::GetBrowsingTopicsApiUsage(browsing_topics_site_data_manager());

  // The usage is tracked after the API call.
  EXPECT_EQ(api_usage_contexts.size(), 2u);
  EXPECT_EQ(api_usage_contexts[0].hashed_main_frame_host,
            HashMainFrameHostForStorage("foo1.com"));
  EXPECT_EQ(api_usage_contexts[0].hashed_context_domain, HashedDomain(1));

  EXPECT_EQ(
      api_usage_contexts[1].hashed_main_frame_host,
      HashMainFrameHostForStorage(https_server_.GetURL("a.test", "/").host()));
  EXPECT_EQ(api_usage_contexts[1].hashed_context_domain,
            GetHashedDomain("b.test"));
}

IN_PROC_BROWSER_TEST_F(
    BrowsingTopicsBrowserTest,
    EmptyPage_PermissionsPolicyBrowsingTopicsNone_TopicsAPI) {
  GURL main_frame_url = https_server_.GetURL(
      "a.test", "/browsing_topics/empty_page_browsing_topics_none.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  EXPECT_EQ(
      "The \"browsing-topics\" Permissions Policy denied the use of "
      "document.browsingTopics().",
      InvokeTopicsAPI(web_contents()));
}

IN_PROC_BROWSER_TEST_F(
    BrowsingTopicsBrowserTest,
    EmptyPage_PermissionsPolicyInterestCohortNone_TopicsAPI) {
  GURL main_frame_url = https_server_.GetURL(
      "a.test", "/browsing_topics/empty_page_interest_cohort_none.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  EXPECT_EQ(
      "The \"interest-cohort\" Permissions Policy denied the use of "
      "document.browsingTopics().",
      InvokeTopicsAPI(web_contents()));
}

IN_PROC_BROWSER_TEST_F(
    BrowsingTopicsBrowserTest,
    OneIframePage_SubframePermissionsPolicyBrowsingTopicsNone_TopicsAPI) {
  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/one_iframe_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  GURL subframe_url = https_server_.GetURL(
      "a.test", "/browsing_topics/empty_page_browsing_topics_none.html");

  ASSERT_TRUE(content::NavigateIframeToURL(web_contents(),
                                           /*iframe_id=*/"frame",
                                           subframe_url));

  std::string result = InvokeTopicsAPI(web_contents());
  EXPECT_TRUE(result == kExpectedResultOrder1 ||
              result == kExpectedResultOrder2);

  EXPECT_EQ(
      "The \"browsing-topics\" Permissions Policy denied the use of "
      "document.browsingTopics().",
      InvokeTopicsAPI(
          content::ChildFrameAt(web_contents()->GetMainFrame(), 0)));
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       PermissionsPolicyAllowCertainOrigin_TopicsAPI) {
  base::StringPairs port_replacement;
  port_replacement.push_back(
      std::make_pair("{{PORT}}", base::NumberToString(https_server_.port())));

  GURL main_frame_url = https_server_.GetURL(
      "a.test", net::test_server::GetFilePathWithReplacements(
                    "/browsing_topics/"
                    "one_iframe_page_browsing_topics_allow_certain_origin.html",
                    port_replacement));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  std::string result = InvokeTopicsAPI(web_contents());
  EXPECT_TRUE(result == kExpectedResultOrder1 ||
              result == kExpectedResultOrder2);

  GURL subframe_url =
      https_server_.GetURL("c.test", "/browsing_topics/empty_page.html");

  ASSERT_TRUE(content::NavigateIframeToURL(web_contents(),
                                           /*iframe_id=*/"frame",
                                           subframe_url));
  EXPECT_EQ("[]", InvokeTopicsAPI(content::ChildFrameAt(
                      web_contents()->GetMainFrame(), 0)));

  subframe_url =
      https_server_.GetURL("b.test", "/browsing_topics/empty_page.html");

  ASSERT_TRUE(content::NavigateIframeToURL(web_contents(),
                                           /*iframe_id=*/"frame",
                                           subframe_url));

  EXPECT_EQ(
      "The \"browsing-topics\" Permissions Policy denied the use of "
      "document.browsingTopics().",
      InvokeTopicsAPI(
          content::ChildFrameAt(web_contents()->GetMainFrame(), 0)));
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       TopicsAPINotAllowedInInsecureContext) {
  GURL main_frame_url = embedded_test_server()->GetURL(
      "a.test", "/browsing_topics/one_iframe_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  // Navigate the iframe to a https site.
  GURL subframe_url = https_server_.GetURL("b.test", "/empty_page.html");
  content::NavigateIframeToURL(web_contents(),
                               /*iframe_id=*/"frame", subframe_url);

  // Both the main frame and the subframe are insecure context because the main
  // frame is loaded over HTTP. Expect that the API isn't available in either
  // frame.
  EXPECT_EQ("not a function", InvokeTopicsAPI(web_contents()));
  EXPECT_EQ("not a function", InvokeTopicsAPI(content::ChildFrameAt(
                                  web_contents()->GetMainFrame(), 0)));
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       TopicsAPINotAllowedInDetachedDocument) {
  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/one_iframe_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  EXPECT_EQ(
      "Failed to execute 'browsingTopics' on 'Document': A browsing "
      "context is required when calling document.browsingTopics().",
      EvalJs(web_contents(), R"(
      const iframe = document.getElementById('frame');
      const childDocument = iframe.contentWindow.document;
      iframe.remove();

      childDocument.browsingTopics()
        .then(topics => "success")
        .catch(error => error.message);
    )"));
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       TopicsAPINotAllowedInOpaqueOriginDocument) {
  GURL main_frame_url = https_server_.GetURL(
      "a.test", "/browsing_topics/one_sandboxed_iframe_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  EXPECT_EQ(
      "document.browsingTopics() is not allowed in an opaque origin context.",
      InvokeTopicsAPI(
          content::ChildFrameAt(web_contents()->GetMainFrame(), 0)));
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       TopicsAPINotAllowedInFencedFrame) {
  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/one_iframe_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  GURL fenced_frame_url =
      https_server_.GetURL("b.test", "/fenced_frames/title1.html");

  content::RenderFrameHostWrapper fenced_frame_rfh_wrapper(
      fenced_frame_test_helper_.CreateFencedFrame(
          web_contents()->GetMainFrame(), fenced_frame_url));

  EXPECT_EQ(
      "document.browsingTopics() is only allowed in the primary main frame or "
      "in its child iframes.",
      InvokeTopicsAPI(fenced_frame_rfh_wrapper.get()));
}

}  // namespace browsing_topics
