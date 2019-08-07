// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/previews_ukm_observer.h"

#include <memory>
#include <unordered_map>

#include "base/macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/optional.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/page_load_metrics/metrics_web_contents_observer.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/page_load_tracker.h"
#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_features.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/common/previews_state.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/ip_endpoint.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"

namespace content {
class NavigationHandle;
}

namespace previews {

namespace {

const char kDefaultTestUrl[] = "https://www.google.com/";

class TestPreviewsUKMObserver : public PreviewsUKMObserver {
 public:
  TestPreviewsUKMObserver(
      PreviewsType committed_preview,
      content::PreviewsState allowed_state,
      bool lite_page_received,
      bool lite_page_redirect_received,
      bool noscript_on,
      bool resource_loading_hints_on,
      bool origin_opt_out_received,
      bool save_data_enabled,
      bool is_offline_preview,
      CoinFlipHoldbackResult coin_flip_result,
      std::unordered_map<PreviewsType, PreviewsEligibilityReason>
          eligibility_reasons,
      base::Optional<base::TimeDelta> navigation_restart_penalty,
      base::Optional<std::string> hint_version_string)
      : committed_preview_(committed_preview),
        allowed_state_(allowed_state),
        lite_page_received_(lite_page_received),
        lite_page_redirect_received_(lite_page_redirect_received),
        noscript_on_(noscript_on),
        resource_loading_hints_on_(resource_loading_hints_on),
        origin_opt_out_received_(origin_opt_out_received),
        save_data_enabled_(save_data_enabled),
        is_offline_preview_(is_offline_preview),
        coin_flip_result_(coin_flip_result),
        eligibility_reasons_(eligibility_reasons),
        navigation_restart_penalty_(navigation_restart_penalty),
        hint_version_string_(hint_version_string) {}

  ~TestPreviewsUKMObserver() override {}

  // page_load_metrics::PageLoadMetricsObserver implementation:
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle,
                         ukm::SourceId source_id) override {
    PreviewsUITabHelper* ui_tab_helper = PreviewsUITabHelper::FromWebContents(
        navigation_handle->GetWebContents());
    previews::PreviewsUserData* user_data =
        ui_tab_helper->CreatePreviewsUserDataForNavigationHandle(
            navigation_handle, 1u);

    user_data->SetCommittedPreviewsTypeForTesting(committed_preview_);
    user_data->set_allowed_previews_state(allowed_state_);
    user_data->set_coin_flip_holdback_result(coin_flip_result_);

    if (noscript_on_) {
      content::PreviewsState previews_state =
          user_data->committed_previews_state();
      user_data->set_committed_previews_state(previews_state |=
                                              content::NOSCRIPT_ON);
    }

    if (resource_loading_hints_on_) {
      content::PreviewsState previews_state =
          user_data->committed_previews_state();
      user_data->set_committed_previews_state(
          previews_state |= content::RESOURCE_LOADING_HINTS_ON);
    }

    if (lite_page_received_) {
      content::PreviewsState previews_state =
          user_data->committed_previews_state();
      user_data->set_committed_previews_state(previews_state |=
                                              content::SERVER_LITE_PAGE_ON);
    }

    if (lite_page_redirect_received_) {
      content::PreviewsState previews_state =
          user_data->committed_previews_state();
      user_data->set_committed_previews_state(previews_state |=
                                              content::LITE_PAGE_REDIRECT_ON);
    }

    if (is_offline_preview_) {
      content::PreviewsState previews_state =
          user_data->committed_previews_state();
      user_data->set_committed_previews_state(previews_state |=
                                              content::OFFLINE_PAGE_ON);
    }

    if (navigation_restart_penalty_.has_value()) {
      user_data->set_server_lite_page_info(
          std::make_unique<previews::PreviewsUserData::ServerLitePageInfo>());
      user_data->server_lite_page_info()->original_navigation_start =
          navigation_handle->NavigationStart() -
          navigation_restart_penalty_.value();
    }

    if (origin_opt_out_received_) {
      user_data->set_cache_control_no_transform_directive();
    }

    for (auto iter = eligibility_reasons_.begin();
         iter != eligibility_reasons_.end(); iter++) {
      user_data->SetEligibilityReasonForPreview(iter->first, iter->second);
    }

    if (hint_version_string_.has_value()) {
      user_data->set_serialized_hint_version_string(
          hint_version_string_.value());
    }

    return PreviewsUKMObserver::OnCommit(navigation_handle, source_id);
  }

 private:
  bool IsDataSaverEnabled(
      content::NavigationHandle* navigation_handle) const override {
    return save_data_enabled_;
  }

  bool IsOfflinePreview(content::WebContents* web_contents) const override {
    return is_offline_preview_;
  }

  PreviewsType committed_preview_;
  content::PreviewsState allowed_state_;
  bool lite_page_received_;
  bool lite_page_redirect_received_;
  bool noscript_on_;
  bool resource_loading_hints_on_;
  bool origin_opt_out_received_;
  const bool save_data_enabled_;
  const bool is_offline_preview_;
  CoinFlipHoldbackResult coin_flip_result_;
  std::unordered_map<PreviewsType, PreviewsEligibilityReason>
      eligibility_reasons_;
  base::Optional<base::TimeDelta> navigation_restart_penalty_;
  base::Optional<std::string> hint_version_string_;

  DISALLOW_COPY_AND_ASSIGN(TestPreviewsUKMObserver);
};

class PreviewsUKMObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  PreviewsUKMObserverTest() {}
  ~PreviewsUKMObserverTest() override {}

  void RunTest(content::PreviewsState allowed_state,
               PreviewsType committed_preview,
               bool lite_page_received,
               bool lite_page_redirect_received,
               bool noscript_on,
               bool resource_loading_hints_on,
               bool origin_opt_out,
               bool save_data_enabled,
               bool is_offline_preview,
               CoinFlipHoldbackResult coin_flip_result,
               std::unordered_map<PreviewsType, PreviewsEligibilityReason>
                   eligibility_reasons,
               base::Optional<base::TimeDelta> navigation_restart_penalty,
               base::Optional<std::string> hint_version_string) {
    committed_preview_ = committed_preview;
    allowed_state_ = allowed_state;
    lite_page_received_ = lite_page_received;
    lite_page_redirect_received_ = lite_page_redirect_received;
    noscript_on_ = noscript_on;
    resource_loading_hints_on_ = resource_loading_hints_on;
    origin_opt_out_ = origin_opt_out;
    save_data_enabled_ = save_data_enabled;
    is_offline_preview_ = is_offline_preview;
    coin_flip_result_ = coin_flip_result;
    eligibility_reasons_ = eligibility_reasons;
    navigation_restart_penalty_ = navigation_restart_penalty;
    hint_version_string_ = hint_version_string;
    auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
        GURL(kDefaultTestUrl), web_contents());
    if (is_offline_preview_)
      navigation->SetContentsMimeType("multipart/related");

    navigation->Commit();
  }

  void ValidateUKM(bool server_lofi_expected,
                   bool client_lofi_expected,
                   bool lite_page_expected,
                   bool lite_page_redirect_expected,
                   bool noscript_expected,
                   bool resource_loading_hints_expected,
                   int opt_out_value,
                   bool origin_opt_out_expected,
                   bool save_data_enabled_expected,
                   bool offline_preview_expected,
                   bool previews_likely_expected,
                   CoinFlipHoldbackResult coin_flip_result_expected,
                   std::unordered_map<PreviewsType, PreviewsEligibilityReason>
                       eligibility_reasons,
                   base::Optional<base::TimeDelta> navigation_restart_penalty,
                   base::Optional<int64_t> hint_generation_timestamp,
                   base::Optional<int> hint_source) {
    ValidatePreviewsUKM(server_lofi_expected, client_lofi_expected,
                        lite_page_expected, lite_page_redirect_expected,
                        noscript_expected, resource_loading_hints_expected,
                        opt_out_value, origin_opt_out_expected,
                        save_data_enabled_expected, offline_preview_expected,
                        previews_likely_expected, coin_flip_result_expected,
                        eligibility_reasons, navigation_restart_penalty);
    ValidateOptimizationGuideUKM(hint_generation_timestamp, hint_source);
  }

  void SetUp() override {
    page_load_metrics::PageLoadMetricsObserverTestHarness::SetUp();
    PreviewsUITabHelper::CreateForWebContents(web_contents());
  }

 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(std::make_unique<TestPreviewsUKMObserver>(
        committed_preview_, allowed_state_, lite_page_received_,
        lite_page_redirect_received_, noscript_on_, resource_loading_hints_on_,
        origin_opt_out_, save_data_enabled_, is_offline_preview_,
        coin_flip_result_, eligibility_reasons_, navigation_restart_penalty_,
        hint_version_string_));
    // Data is only added to the first navigation after RunTest().
    committed_preview_ = PreviewsType::NONE;
    allowed_state_ = content::PREVIEWS_OFF;
    lite_page_received_ = false;
    lite_page_redirect_received_ = false;
    noscript_on_ = false;
    resource_loading_hints_on_ = false;
    origin_opt_out_ = false;
    coin_flip_result_ = CoinFlipHoldbackResult::kNotSet;
    eligibility_reasons_.clear();
    navigation_restart_penalty_ = base::nullopt;
    hint_version_string_ = base::nullopt;
  }

 private:
  void ValidatePreviewsUKM(
      bool server_lofi_expected,
      bool client_lofi_expected,
      bool lite_page_expected,
      bool lite_page_redirect_expected,
      bool noscript_expected,
      bool resource_loading_hints_expected,
      int opt_out_value,
      bool origin_opt_out_expected,
      bool save_data_enabled_expected,
      bool offline_preview_expected,
      bool previews_likely_expected,
      CoinFlipHoldbackResult coin_flip_result_expected,
      std::unordered_map<PreviewsType, PreviewsEligibilityReason>
          eligibility_reasons,
      base::Optional<base::TimeDelta> navigation_restart_penalty) {
    using UkmEntry = ukm::builders::Previews;
    auto entries = test_ukm_recorder().GetEntriesByName(UkmEntry::kEntryName);
    if (!server_lofi_expected && !client_lofi_expected && !lite_page_expected &&
        !lite_page_redirect_expected && !noscript_expected &&
        !resource_loading_hints_expected && opt_out_value == 0 &&
        !origin_opt_out_expected && !save_data_enabled_expected &&
        !offline_preview_expected && !previews_likely_expected &&
        coin_flip_result_expected == CoinFlipHoldbackResult::kNotSet &&
        !navigation_restart_penalty.has_value()) {
      EXPECT_EQ(0u, entries.size());
      return;
    }
    EXPECT_EQ(1u, entries.size());
    for (const auto* const entry : entries) {
      test_ukm_recorder().ExpectEntrySourceHasUrl(entry, GURL(kDefaultTestUrl));
      EXPECT_EQ(server_lofi_expected, test_ukm_recorder().EntryHasMetric(
                                          entry, UkmEntry::kserver_lofiName));
      EXPECT_EQ(client_lofi_expected, test_ukm_recorder().EntryHasMetric(
                                          entry, UkmEntry::kclient_lofiName));
      EXPECT_EQ(lite_page_expected, test_ukm_recorder().EntryHasMetric(
                                        entry, UkmEntry::klite_pageName));
      EXPECT_EQ(lite_page_redirect_expected,
                test_ukm_recorder().EntryHasMetric(
                    entry, UkmEntry::klite_page_redirectName));
      EXPECT_EQ(noscript_expected, test_ukm_recorder().EntryHasMetric(
                                       entry, UkmEntry::knoscriptName));
      EXPECT_EQ(resource_loading_hints_expected,
                test_ukm_recorder().EntryHasMetric(
                    entry, UkmEntry::kresource_loading_hintsName));
      EXPECT_EQ(offline_preview_expected,
                test_ukm_recorder().EntryHasMetric(
                    entry, UkmEntry::koffline_previewName));
      EXPECT_EQ(opt_out_value != 0, test_ukm_recorder().EntryHasMetric(
                                        entry, UkmEntry::kopt_outName));
      if (opt_out_value != 0) {
        test_ukm_recorder().ExpectEntryMetric(entry, UkmEntry::kopt_outName,
                                              opt_out_value);
      }
      EXPECT_EQ(origin_opt_out_expected,
                test_ukm_recorder().EntryHasMetric(
                    entry, UkmEntry::korigin_opt_outName));
      EXPECT_EQ(save_data_enabled_expected,
                test_ukm_recorder().EntryHasMetric(
                    entry, UkmEntry::ksave_data_enabledName));
      EXPECT_EQ(previews_likely_expected,
                test_ukm_recorder().EntryHasMetric(
                    entry, UkmEntry::kpreviews_likelyName));
      EXPECT_EQ(static_cast<int>(coin_flip_result_expected),
                *test_ukm_recorder().GetEntryMetric(
                    entry, UkmEntry::kcoin_flip_resultName));
      if (navigation_restart_penalty.has_value()) {
        test_ukm_recorder().ExpectEntryMetric(
            entry, UkmEntry::knavigation_restart_penaltyName,
            navigation_restart_penalty.value().InMilliseconds());
      }

      int want_lite_page_eligibility_reason =
          static_cast<int>(eligibility_reasons[PreviewsType::LITE_PAGE]);
      if (want_lite_page_eligibility_reason) {
        test_ukm_recorder().ExpectEntryMetric(
            entry, UkmEntry::kproxy_lite_page_eligibility_reasonName,
            want_lite_page_eligibility_reason);
      } else {
        EXPECT_FALSE(test_ukm_recorder().EntryHasMetric(
            entry, UkmEntry::kproxy_lite_page_eligibility_reasonName));
      }

      int want_lite_page_redirect_eligibility_reason = static_cast<int>(
          eligibility_reasons[PreviewsType::LITE_PAGE_REDIRECT]);
      if (want_lite_page_redirect_eligibility_reason) {
        test_ukm_recorder().ExpectEntryMetric(
            entry, UkmEntry::klite_page_redirect_eligibility_reasonName,
            want_lite_page_redirect_eligibility_reason);
      } else {
        EXPECT_FALSE(test_ukm_recorder().EntryHasMetric(
            entry, UkmEntry::klite_page_redirect_eligibility_reasonName));
      }

      int want_noscript_eligibility_reason =
          static_cast<int>(eligibility_reasons[PreviewsType::NOSCRIPT]);
      if (want_noscript_eligibility_reason) {
        test_ukm_recorder().ExpectEntryMetric(
            entry, UkmEntry::knoscript_eligibility_reasonName,
            want_noscript_eligibility_reason);
      } else {
        EXPECT_FALSE(test_ukm_recorder().EntryHasMetric(
            entry, UkmEntry::knoscript_eligibility_reasonName));
      }

      int want_resource_loading_hints_eligibility_reason = static_cast<int>(
          eligibility_reasons[PreviewsType::RESOURCE_LOADING_HINTS]);
      if (want_resource_loading_hints_eligibility_reason) {
        test_ukm_recorder().ExpectEntryMetric(
            entry, UkmEntry::kresource_loading_hints_eligibility_reasonName,
            want_resource_loading_hints_eligibility_reason);
      } else {
        EXPECT_FALSE(test_ukm_recorder().EntryHasMetric(
            entry, UkmEntry::kresource_loading_hints_eligibility_reasonName));
      }

      int want_offline_eligibility_reason =
          static_cast<int>(eligibility_reasons[PreviewsType::OFFLINE]);
      if (want_offline_eligibility_reason) {
        test_ukm_recorder().ExpectEntryMetric(
            entry, UkmEntry::koffline_eligibility_reasonName,
            want_offline_eligibility_reason);
      } else {
        EXPECT_FALSE(test_ukm_recorder().EntryHasMetric(
            entry, UkmEntry::koffline_eligibility_reasonName));
      }
    }
  }

  void ValidateOptimizationGuideUKM(
      base::Optional<int64_t> hint_generation_timestamp,
      base::Optional<int> hint_source) {
    using UkmEntry = ukm::builders::OptimizationGuide;
    auto entries = test_ukm_recorder().GetEntriesByName(UkmEntry::kEntryName);
    if (!hint_generation_timestamp.has_value() && !hint_source.has_value()) {
      EXPECT_EQ(0u, entries.size());
      return;
    }

    EXPECT_EQ(1u, entries.size());
    for (const auto* const entry : entries) {
      test_ukm_recorder().ExpectEntrySourceHasUrl(entry, GURL(kDefaultTestUrl));
      if (hint_generation_timestamp.has_value()) {
        test_ukm_recorder().ExpectEntryMetric(
            entry, UkmEntry::kHintGenerationTimestampName,
            hint_generation_timestamp.value());
      } else {
        EXPECT_FALSE(test_ukm_recorder().EntryHasMetric(
            entry, UkmEntry::kHintGenerationTimestampName));
      }
      if (hint_source.has_value()) {
        test_ukm_recorder().ExpectEntryMetric(entry, UkmEntry::kHintSourceName,
                                              hint_source.value());
      } else {
        EXPECT_FALSE(test_ukm_recorder().EntryHasMetric(
            entry, UkmEntry::kHintSourceName));
      }
    }
  }

  PreviewsType committed_preview_ = PreviewsType::NONE;
  content::PreviewsState allowed_state_ = content::PREVIEWS_OFF;
  bool lite_page_received_ = false;
  bool lite_page_redirect_received_ = false;
  bool noscript_on_ = false;
  bool resource_loading_hints_on_ = false;
  bool origin_opt_out_ = false;
  bool save_data_enabled_ = false;
  bool is_offline_preview_ = false;
  std::unordered_map<PreviewsType, PreviewsEligibilityReason>
      eligibility_reasons_ = {};
  CoinFlipHoldbackResult coin_flip_result_ = CoinFlipHoldbackResult::kNotSet;
  base::Optional<base::TimeDelta> navigation_restart_penalty_ = base::nullopt;
  base::Optional<std::string> hint_version_string_ = base::nullopt;

  DISALLOW_COPY_AND_ASSIGN(PreviewsUKMObserverTest);
};

TEST_F(PreviewsUKMObserverTest, NoPreviewSeen) {
  RunTest(content::PREVIEWS_UNSPECIFIED /* allowed_state */, PreviewsType::NONE,
          false /* lite_page_received */,
          false /* lite_page_redirect_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          false /* save_data_enabled */, false /* is_offline_preview */,
          CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
          base::nullopt /* navigation_restart_penalty */,
          base::nullopt /* hint_version_string */);
  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, false /* lite_page_expected */,
              false /* lite_page_redirect_expected */,
              false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              0 /* opt_out_value */, false /* origin_opt_out_expected */,
              false /* save_data_enabled_expected */,
              false /* offline_preview_expected */, false /* previews_likely */,
              CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
              base::nullopt /* navigation_restart_penalty */,
              base::nullopt /* hint_generation_timestamp */,
              base::nullopt /* hint_source */);
}

TEST_F(PreviewsUKMObserverTest, UntrackedPreviewTypeOptOut) {
  RunTest(content::PREVIEWS_UNSPECIFIED /* allowed_state */, PreviewsType::NONE,
          false /* lite_page_received */,
          false /* lite_page_redirect_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          false /* save_data_enabled */, false /* is_offline_preview */,
          CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
          base::nullopt /* navigation_restart_penalty */,
          base::nullopt /* hint_version_string */);
  observer()->BroadcastEventToObservers(PreviewsUITabHelper::OptOutEventKey());
  NavigateToUntrackedUrl();

  // Opt out should not be added since we don't track this type.
  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, false /* lite_page_expected */,
              false /* lite_page_redirect_expected */,
              false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              0 /* opt_out_value */, false /* origin_opt_out_expected */,
              false /* save_data_enabled_expected */,
              false /* offline_preview_expected */, false /* previews_likely */,
              CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
              base::nullopt /* navigation_restart_penalty */,
              base::nullopt /* hint_generation_timestamp */,
              base::nullopt /* hint_source */);
}

TEST_F(PreviewsUKMObserverTest, LitePageSeen) {
  RunTest(content::PREVIEWS_UNSPECIFIED /* allowed_state */, PreviewsType::NONE,
          true /* lite_page_received */,
          false /* lite_page_redirect_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          false /* save_data_enabled */, false /* is_offline_preview */,
          CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
          base::nullopt /* navigation_restart_penalty */,
          base::nullopt /* hint_version_string */);

  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, true /* lite_page_expected */,
              false /* lite_page_redirect_expected */,
              false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              0 /* opt_out_value */, false /* origin_opt_out_expected */,
              false /* save_data_enabled_expected */,
              false /* offline_preview_expected */, false /* previews_likely */,
              CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
              base::nullopt /* navigation_restart_penalty */,
              base::nullopt /* hint_generation_timestamp */,
              base::nullopt /* hint_source */);
}

TEST_F(PreviewsUKMObserverTest, LitePageOptOutChip) {
  RunTest(content::PREVIEWS_UNSPECIFIED /* allowed_state */,
          PreviewsType::LITE_PAGE, true /* lite_page_received */,
          false /* lite_page_redirect_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          false /* save_data_enabled */, false /* is_offline_preview */,
          CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
          base::nullopt /* navigation_restart_penalty */,
          base::nullopt /* hint_version_string */);

  observer()->BroadcastEventToObservers(PreviewsUITabHelper::OptOutEventKey());
  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, true /* lite_page_expected */,
              false /* lite_page_redirect_expected */,
              false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              2 /* opt_out_value */, false /* origin_opt_out_expected */,
              false /* save_data_enabled_expected */,
              false /* offline_preview_expected */, false /* previews_likely */,
              CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
              base::nullopt /* navigation_restart_penalty */,
              base::nullopt /* hint_generation_timestamp */,
              base::nullopt /* hint_source */);
}

TEST_F(PreviewsUKMObserverTest, LitePageRedirectSeen) {
  RunTest(content::PREVIEWS_UNSPECIFIED /* allowed_state */, PreviewsType::NONE,
          false /* lite_page_received */,
          true /* lite_page_redirect_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          false /* save_data_enabled */, false /* is_offline_preview */,
          CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
          base::nullopt /* navigation_restart_penalty */,
          base::nullopt /* hint_version_string */);

  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, false /* lite_page_expected */,
              true /* lite_page_redirect_expected */,
              false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              0 /* opt_out_value */, false /* origin_opt_out_expected */,
              false /* save_data_enabled_expected */,
              false /* offline_preview_expected */, false /* previews_likely */,
              CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
              base::nullopt /* navigation_restart_penalty */,
              base::nullopt /* hint_generation_timestamp */,
              base::nullopt /* hint_source */);
}

TEST_F(PreviewsUKMObserverTest, LitePageRedirectOptOutChip) {
  RunTest(content::PREVIEWS_UNSPECIFIED /* allowed_state */,
          PreviewsType::LITE_PAGE_REDIRECT, false /* lite_page_received */,
          true /* lite_page_redirect_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          false /* save_data_enabled */, false /* is_offline_preview */,
          CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
          base::nullopt /* navigation_restart_penalty */,
          base::nullopt /* hint_version_string */);

  observer()->BroadcastEventToObservers(PreviewsUITabHelper::OptOutEventKey());
  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, false /* lite_page_expected */,
              true /* lite_page_redirect_expected */,
              false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              2 /* opt_out_value */, false /* origin_opt_out_expected */,
              false /* save_data_enabled_expected */,
              false /* offline_preview_expected */, false /* previews_likely */,
              CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
              base::nullopt /* navigation_restart_penalty */,
              base::nullopt /* hint_generation_timestamp */,
              base::nullopt /* hint_source */);
}

TEST_F(PreviewsUKMObserverTest, NoScriptSeenWithBadVersionString) {
  RunTest(content::PREVIEWS_UNSPECIFIED /* allowed_state */,
          PreviewsType::NOSCRIPT, false /* lite_page_received */,
          false /* lite_page_redirect_received */, true /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          false /* save_data_enabled */, false /* is_offline_preview */,
          CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
          base::nullopt /* navigation_restart_penalty */, "badversion");

  NavigateToUntrackedUrl();

  ValidateUKM(
      false /* server_lofi_expected */, false /* client_lofi_expected */,
      false /* lite_page_expected */, false /* lite_page_redirect_expected */,
      true /* noscript_expected */, false /* resource_loading_hints_expected */,
      0 /* opt_out_value */, false /* origin_opt_out_expected */,
      false /* save_data_enabled_expected */,
      false /* offline_preview_expected */, false /* previews_likely */,
      CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
      base::nullopt /* navigation_restart_penalty */,
      base::nullopt /* hint_generation_timestamp */,
      base::nullopt /* hint_source */);
}

TEST_F(PreviewsUKMObserverTest, NoScriptOptOutChip) {
  RunTest(content::PREVIEWS_UNSPECIFIED /* allowed_state */,
          PreviewsType::NOSCRIPT, false /* lite_page_received */,
          false /* lite_page_redirect_received */, true /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          false /* save_data_enabled */, false /* is_offline_preview */,
          CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
          base::nullopt /* navigation_restart_penalty */,
          base::nullopt /* hint_version_string */);

  observer()->BroadcastEventToObservers(PreviewsUITabHelper::OptOutEventKey());
  NavigateToUntrackedUrl();

  ValidateUKM(
      false /* server_lofi_expected */, false /* client_lofi_expected */,
      false /* lite_page_expected */, false /* lite_page_redirect_expected */,
      true /* noscript_expected */, false /* resource_loading_hints_expected */,
      2 /* opt_out_value */, false /* origin_opt_out_expected */,
      false /* save_data_enabled_expected */,
      false /* offline_preview_expected */, false /* previews_likely */,
      CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
      base::nullopt /* navigation_restart_penalty */,
      base::nullopt /* hint_generation_timestamp */,
      base::nullopt /* hint_source */);
}

TEST_F(PreviewsUKMObserverTest, OfflinePreviewsSeen) {
  RunTest(content::PREVIEWS_UNSPECIFIED /* allowed_state */,
          PreviewsType::OFFLINE, false /* lite_page_received */,
          false /* lite_page_redirect_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          false /* save_data_enabled */, true /* is_offline_preview */,
          CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
          base::nullopt /* navigation_restart_penalty */,
          base::nullopt /* hint_version_string */);

  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, false /* lite_page_expected */,
              false /* lite_page_redirect_expected */,
              false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              0 /* opt_out_value */, false /* origin_opt_out_expected */,
              false /* save_data_enabled_expected */,
              true /* offline_preview_expected */, false /* previews_likely */,
              CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
              base::nullopt /* navigation_restart_penalty */,
              base::nullopt /* hint_generation_timestamp */,
              base::nullopt /* hint_source */);
}

TEST_F(PreviewsUKMObserverTest, ResourceLoadingHintsSeen) {
  RunTest(content::PREVIEWS_UNSPECIFIED /* allowed_state */,
          PreviewsType::RESOURCE_LOADING_HINTS, false /* lite_page_received */,
          false /* lite_page_redirect_received */, false /* noscript_on */,
          true /* resource_loading_hints_on */, false /* origin_opt_out */,
          false /* save_data_enabled */, false /* is_offline_preview */,
          CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
          base::nullopt /* navigation_restart_penalty */,
          base::nullopt /* hint_version_string */);

  NavigateToUntrackedUrl();

  ValidateUKM(
      false /* server_lofi_expected */, false /* client_lofi_expected */,
      false /* lite_page_expected */, false /* lite_page_redirect_expected */,
      false /* noscript_expected */, true /* resource_loading_hints_expected */,
      0 /* opt_out_value */, false /* origin_opt_out_expected */,
      false /* save_data_enabled_expected */,
      false /* offline_preview_expected */, false /* previews_likely */,
      CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
      base::nullopt /* navigation_restart_penalty */,
      base::nullopt /* hint_generation_timestamp */,
      base::nullopt /* hint_source */);
}

TEST_F(PreviewsUKMObserverTest, ResourceLoadingHintsOptOutChip) {
  RunTest(content::PREVIEWS_UNSPECIFIED /* allowed_state */,
          PreviewsType::RESOURCE_LOADING_HINTS, false /* lite_page_received */,
          false /* lite_page_redirect_received */, false /* noscript_on */,
          true /* resource_loading_hints_on */, false /* origin_opt_out */,
          false /* save_data_enabled */, false /* is_offline_preview */,
          CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
          base::nullopt /* navigation_restart_penalty */,
          base::nullopt /* hint_version_string */);

  observer()->BroadcastEventToObservers(PreviewsUITabHelper::OptOutEventKey());
  NavigateToUntrackedUrl();

  ValidateUKM(
      false /* server_lofi_expected */, false /* client_lofi_expected */,
      false /* lite_page_expected */, false /* lite_page_redirect_expected */,
      false /* noscript_expected */, true /* resource_loading_hints_expected */,
      2 /* opt_out_value */, false /* origin_opt_out_expected */,
      false /* save_data_enabled_expected */,
      false /* offline_preview_expected */, false /* previews_likely */,
      CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
      base::nullopt /* navigation_restart_penalty */,
      base::nullopt /* hint_generation_timestamp */,
      base::nullopt /* hint_source */);
}

TEST_F(PreviewsUKMObserverTest, ClientLoFiSeen) {
  RunTest(content::PREVIEWS_UNSPECIFIED /* allowed_state */, PreviewsType::LOFI,
          false /* lite_page_received */,
          false /* lite_page_redirect_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          false /* save_data_enabled */, false /* is_offline_preview */,
          CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
          base::nullopt /* navigation_restart_penalty */,
          base::nullopt /* hint_version_string */);

  std::unique_ptr<data_reduction_proxy::DataReductionProxyData> data =
      std::make_unique<data_reduction_proxy::DataReductionProxyData>();
  data->set_client_lofi_requested(true);

  // Prepare 3 resources of varying size and configurations, 2 of which have
  // client LoFi set.
  page_load_metrics::ExtraRequestCompleteInfo resources[] = {
      {GURL(kDefaultTestUrl), net::IPEndPoint(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 * 5 /* original_network_content_length */, std::move(data),
       content::ResourceType::kImage, 0, nullptr /* load_timing_info */},
      // Uncached non-proxied request.
      {GURL(kDefaultTestUrl), net::IPEndPoint(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 /* original_network_content_length */,
       nullptr /* data_reduction_proxy_data */, content::ResourceType::kImage,
       0, nullptr /* load_timing_info */},
  };

  for (const auto& request : resources)
    SimulateLoadedResource(request);

  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */, true /* client_lofi_expected */,
              false /* lite_page_expected */,
              false /* lite_page_redirect_expected */,
              false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              0 /* opt_out_value */, false /* origin_opt_out_expected */,
              false /* save_data_enabled_expected */,
              false /* offline_preview_expected */, false /* previews_likely */,
              CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
              base::nullopt /* navigation_restart_penalty */,
              base::nullopt /* hint_generation_timestamp */,
              base::nullopt /* hint_source */);
}

TEST_F(PreviewsUKMObserverTest, ClientLoFiOptOutChip) {
  RunTest(content::PREVIEWS_UNSPECIFIED /* allowed_state */, PreviewsType::LOFI,
          false /* lite_page_received */,
          false /* lite_page_redirect_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          false /* save_data_enabled */, false /* is_offline_preview */,
          CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
          base::nullopt /* navigation_restart_penalty */,
          base::nullopt /* hint_version_string */);

  std::unique_ptr<data_reduction_proxy::DataReductionProxyData> data =
      std::make_unique<data_reduction_proxy::DataReductionProxyData>();
  data->set_client_lofi_requested(true);

  // Prepare 3 resources of varying size and configurations, 2 of which have
  // client LoFi set.
  page_load_metrics::ExtraRequestCompleteInfo resources[] = {
      {GURL(kDefaultTestUrl), net::IPEndPoint(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 * 5 /* original_network_content_length */, std::move(data),
       content::ResourceType::kImage, 0, nullptr /* load_timing_info */},
      // Uncached non-proxied request.
      {GURL(kDefaultTestUrl), net::IPEndPoint(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 /* original_network_content_length */,
       nullptr /* data_reduction_proxy_data */, content::ResourceType::kImage,
       0, nullptr /* load_timing_info */},
  };

  for (const auto& request : resources)
    SimulateLoadedResource(request);
  observer()->BroadcastEventToObservers(PreviewsUITabHelper::OptOutEventKey());
  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */, true /* client_lofi_expected */,
              false /* lite_page_expected */,
              false /* lite_page_redirect_expected */,
              false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              2 /* opt_out_value */, false /* origin_opt_out_expected */,
              false /* save_data_enabled_expected */,
              false /* offline_preview_expected */, false /* previews_likely */,
              CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
              base::nullopt /* navigation_restart_penalty */,
              base::nullopt /* hint_generation_timestamp */,
              base::nullopt /* hint_source */);
}

TEST_F(PreviewsUKMObserverTest, ServerLoFiSeen) {
  RunTest(content::PREVIEWS_UNSPECIFIED /* allowed_state */, PreviewsType::LOFI,
          false /* lite_page_received */,
          false /* lite_page_redirect_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          false /* save_data_enabled */, false /* is_offline_preview */,
          CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
          base::nullopt /* navigation_restart_penalty */,
          base::nullopt /* hint_version_string */);

  std::unique_ptr<data_reduction_proxy::DataReductionProxyData> data =
      std::make_unique<data_reduction_proxy::DataReductionProxyData>();
  data->set_used_data_reduction_proxy(true);
  data->set_lofi_received(true);

  // Prepare 3 resources of varying size and configurations, 2 of which have
  // client LoFi set.
  page_load_metrics::ExtraRequestCompleteInfo resources[] = {
      {GURL(kDefaultTestUrl), net::IPEndPoint(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 * 5 /* original_network_content_length */, std::move(data),
       content::ResourceType::kImage, 0, nullptr /* load_timing_info */},
      {GURL(kDefaultTestUrl), net::IPEndPoint(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 /* original_network_content_length */,
       nullptr /* data_reduction_proxy_data */, content::ResourceType::kImage,
       0, nullptr /* load_timing_info */},
  };

  for (const auto& request : resources)
    SimulateLoadedResource(request);

  NavigateToUntrackedUrl();

  ValidateUKM(true /* server_lofi_expected */, false /* client_lofi_expected */,
              false /* lite_page_expected */,
              false /* lite_page_redirect_expected */,
              false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              0 /* opt_out_value */, false /* origin_opt_out_expected */,
              false /* save_data_enabled_expected */,
              false /* offline_preview_expected */, false /* previews_likely */,
              CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
              base::nullopt /* navigation_restart_penalty */,
              base::nullopt /* hint_generation_timestamp */,
              base::nullopt /* hint_source */);
}

TEST_F(PreviewsUKMObserverTest, ServerLoFiOptOutChip) {
  RunTest(content::PREVIEWS_UNSPECIFIED /* allowed_state */, PreviewsType::LOFI,
          false /* lite_page_received */,
          false /* lite_page_redirect_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          false /* save_data_enabled */, false /* is_offline_preview */,
          CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
          base::nullopt /* navigation_restart_penalty */,
          base::nullopt /* hint_version_string */);

  std::unique_ptr<data_reduction_proxy::DataReductionProxyData> data =
      std::make_unique<data_reduction_proxy::DataReductionProxyData>();
  data->set_used_data_reduction_proxy(true);
  data->set_lofi_received(true);

  // Prepare 3 resources of varying size and configurations, 2 of which have
  // client LoFi set.
  page_load_metrics::ExtraRequestCompleteInfo resources[] = {
      {GURL(kDefaultTestUrl), net::IPEndPoint(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 * 5 /* original_network_content_length */, std::move(data),
       content::ResourceType::kImage, 0, nullptr /* load_timing_info */},
      {GURL(kDefaultTestUrl), net::IPEndPoint(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 /* original_network_content_length */,
       nullptr /* data_reduction_proxy_data */, content::ResourceType::kImage,
       0, nullptr /* load_timing_info */},
  };

  for (const auto& request : resources)
    SimulateLoadedResource(request);

  observer()->BroadcastEventToObservers(PreviewsUITabHelper::OptOutEventKey());
  NavigateToUntrackedUrl();

  ValidateUKM(true /* server_lofi_expected */, false /* client_lofi_expected */,
              false /* lite_page_expected */,
              false /* lite_page_redirect_expected */,
              false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              2 /* opt_out_value */, false /* origin_opt_out_expected */,
              false /* save_data_enabled_expected */,
              false /* offline_preview_expected */, false /* previews_likely */,
              CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
              base::nullopt /* navigation_restart_penalty */,
              base::nullopt /* hint_generation_timestamp */,
              base::nullopt /* hint_source */);
}

TEST_F(PreviewsUKMObserverTest, BothLoFiSeen) {
  RunTest(content::PREVIEWS_UNSPECIFIED /* allowed_state */, PreviewsType::LOFI,
          false /* lite_page_received */,
          false /* lite_page_redirect_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          false /* save_data_enabled */, false /* is_offline_preview */,
          CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
          base::nullopt /* navigation_restart_penalty */,
          base::nullopt /* hint_version_string */);

  std::unique_ptr<data_reduction_proxy::DataReductionProxyData> data1 =
      std::make_unique<data_reduction_proxy::DataReductionProxyData>();
  data1->set_used_data_reduction_proxy(true);
  data1->set_lofi_received(true);

  std::unique_ptr<data_reduction_proxy::DataReductionProxyData> data2 =
      std::make_unique<data_reduction_proxy::DataReductionProxyData>();
  data2->set_used_data_reduction_proxy(true);
  data2->set_client_lofi_requested(true);

  // Prepare 4 resources of varying size and configurations, 1 has Client LoFi,
  // 1 has Server LoFi.
  page_load_metrics::ExtraRequestCompleteInfo resources[] = {
      // Uncached proxied request with .1 compression ratio.
      {GURL(kDefaultTestUrl), net::IPEndPoint(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 * 10 /* original_network_content_length */, std::move(data1),
       content::ResourceType::kImage, 0, nullptr /* load_timing_info */},
      // Uncached proxied request with .5 compression ratio.
      {GURL(kDefaultTestUrl), net::IPEndPoint(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 * 5 /* original_network_content_length */, std::move(data2),
       content::ResourceType::kImage, 0, nullptr /* load_timing_info */},
  };

  for (const auto& request : resources)
    SimulateLoadedResource(request);

  NavigateToUntrackedUrl();
  ValidateUKM(true /* server_lofi_expected */, true /* client_lofi_expected */,
              false /* lite_page_expected */,
              false /* lite_page_redirect_expected */,
              false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              0 /* opt_out_value */, false /* origin_opt_out_expected */,
              false /* save_data_enabled_expected */,
              false /* offline_preview_expected */, false /* previews_likely */,
              CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
              base::nullopt /* navigation_restart_penalty */,
              base::nullopt /* hint_generation_timestamp */,
              base::nullopt /* hint_source */);
}

TEST_F(PreviewsUKMObserverTest, BothLoFiOptOutChip) {
  RunTest(content::PREVIEWS_UNSPECIFIED /* allowed_state */, PreviewsType::LOFI,
          false /* lite_page_received */,
          false /* lite_page_redirect_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          false /* save_data_enabled */, false /* is_offline_preview */,
          CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
          base::nullopt /* navigation_restart_penalty */,
          base::nullopt /* hint_version_string */);

  std::unique_ptr<data_reduction_proxy::DataReductionProxyData> data1 =
      std::make_unique<data_reduction_proxy::DataReductionProxyData>();
  data1->set_used_data_reduction_proxy(true);
  data1->set_lofi_received(true);

  std::unique_ptr<data_reduction_proxy::DataReductionProxyData> data2 =
      std::make_unique<data_reduction_proxy::DataReductionProxyData>();
  data2->set_used_data_reduction_proxy(true);
  data2->set_client_lofi_requested(true);

  // Prepare 4 resources of varying size and configurations, 1 has Client LoFi,
  // 1 has Server LoFi.
  page_load_metrics::ExtraRequestCompleteInfo resources[] = {
      // Uncached proxied request with .1 compression ratio.
      {GURL(kDefaultTestUrl), net::IPEndPoint(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 * 10 /* original_network_content_length */, std::move(data1),
       content::ResourceType::kImage, 0, nullptr /* load_timing_info */},
      // Uncached proxied request with .5 compression ratio.
      {GURL(kDefaultTestUrl), net::IPEndPoint(), -1, false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 * 5 /* original_network_content_length */, std::move(data2),
       content::ResourceType::kImage, 0, nullptr /* load_timing_info */},
  };

  for (const auto& request : resources)
    SimulateLoadedResource(request);
  observer()->BroadcastEventToObservers(PreviewsUITabHelper::OptOutEventKey());
  NavigateToUntrackedUrl();
  ValidateUKM(true /* server_lofi_expected */, true /* client_lofi_expected */,
              false /* lite_page_expected */,
              false /* lite_page_redirect_expected */,
              false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              2 /* opt_out_value */, false /* origin_opt_out_expected */,
              false /* save_data_enabled_expected */,
              false /* offline_preview_expected */, false /* previews_likely */,
              CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
              base::nullopt /* navigation_restart_penalty */,
              base::nullopt /* hint_generation_timestamp */,
              base::nullopt /* hint_source */);
}

TEST_F(PreviewsUKMObserverTest, OriginOptOut) {
  RunTest(content::PREVIEWS_UNSPECIFIED /* allowed_state */, PreviewsType::NONE,
          false /* lite_page_received */,
          false /* lite_page_redirect_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, true /* origin_opt_out */,
          false /* save_data_enabled */, false /* is_offline_preview */,
          CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
          base::nullopt /* navigation_restart_penalty */,
          base::nullopt /* hint_version_string */);

  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, false /* lite_page_expected */,
              false /* lite_page_redirect_expected */,
              false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              0 /* opt_out_value */, true /* origin_opt_out_expected */,
              false /* save_data_enabled_expected */,
              false /* offline_preview_expected */, false /* previews_likely */,
              CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
              base::nullopt /* navigation_restart_penalty */,
              base::nullopt /* hint_generation_timestamp */,
              base::nullopt /* hint_source */);
}

TEST_F(PreviewsUKMObserverTest, DataSaverEnabled) {
  RunTest(content::PREVIEWS_UNSPECIFIED /* allowed_state */, PreviewsType::NONE,
          false /* lite_page_received */,
          false /* lite_page_redirect_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          true /* save_data_enabled */, false /* is_offline_preview */,
          CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
          base::nullopt /* navigation_restart_penalty */,
          base::nullopt /* hint_version_string */);

  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, false /* lite_page_expected */,
              false /* lite_page_redirect_expected */,
              false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              0 /* opt_out_value */, false /* origin_opt_out_expected */,
              true /* save_data_enabled_expected */,
              false /* offline_preview_expected */, false /* previews_likely */,
              CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
              base::nullopt /* navigation_restart_penalty */,
              base::nullopt /* hint_generation_timestamp */,
              base::nullopt /* hint_source */);
}

// Navigation restart penalty can occur independently of a preview being
// committed so we do not consider the opt out tests here.
TEST_F(PreviewsUKMObserverTest, NavigationRestartPenaltySeen) {
  RunTest(
      content::PREVIEWS_UNSPECIFIED /* allowed_state */, PreviewsType::NONE,
      false /* lite_page_received */, false /* lite_page_redirect_received */,
      false /* noscript_on */, false /* resource_loading_hints_on */,
      false /* origin_opt_out */, false /* save_data_enabled */,
      false /* is_offline_preview */, CoinFlipHoldbackResult::kNotSet,
      {} /* eligibility_reasons */,
      base::TimeDelta::FromMilliseconds(1337) /* navigation_restart_penalty */,
      base::nullopt /* hint_version_string */);

  NavigateToUntrackedUrl();

  ValidateUKM(
      false /* server_lofi_expected */, false /* client_lofi_expected */,
      false /* lite_page_expected */, false /* lite_page_redirect_expected */,
      false /* noscript_expected */,
      false /* resource_loading_hints_expected */, 0 /* opt_out_value */,
      false /* origin_opt_out_expected */,
      false /* save_data_enabled_expected */,
      false /* offline_preview_expected */, false /* previews_likely */,
      CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
      base::TimeDelta::FromMilliseconds(1337) /* navigation_restart_penalty */,
      base::nullopt /* hint_generation_timestamp */,
      base::nullopt /* hint_source */);
}

TEST_F(PreviewsUKMObserverTest, PreviewsLikelySet_PreCommitDecision) {
  RunTest(content::OFFLINE_PAGE_ON | content::NOSCRIPT_ON /* allowed_state */,
          PreviewsType::NONE, false /* lite_page_received */,
          false /* lite_page_redirect_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          true /* save_data_enabled */, true /* is_offline_preview */,
          CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
          base::nullopt /* navigation_restart_penalty */,
          base::nullopt /* hint_version_string */);

  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, false /* lite_page_expected */,
              false /* lite_page_redirect_expected */,
              false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              0 /* opt_out_value */, false /* origin_opt_out_expected */,
              true /* save_data_enabled_expected */,
              true /* offline_preview_expected */, true /* previews_likely */,
              CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
              base::nullopt /* navigation_restart_penalty */,
              base::nullopt /* hint_generation_timestamp */,
              base::nullopt /* hint_source */);
}

TEST_F(PreviewsUKMObserverTest, PreviewsLikelyNotSet_PostCommitDecision) {
  RunTest(content::NOSCRIPT_ON /* allowed_state */, PreviewsType::NONE,
          false /* lite_page_received */,
          false /* lite_page_redirect_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          true /* save_data_enabled */, false /* is_offline_preview */,
          CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
          base::nullopt /* navigation_restart_penalty */,
          base::nullopt /* hint_version_string */);

  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, false /* lite_page_expected */,
              false /* lite_page_redirect_expected */,
              false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              0 /* opt_out_value */, false /* origin_opt_out_expected */,
              true /* save_data_enabled_expected */,
              false /* offline_preview_expected */, false /* previews_likely */,
              CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
              base::nullopt /* navigation_restart_penalty */,
              base::nullopt /* hint_generation_timestamp */,
              base::nullopt /* hint_source */);
}

TEST_F(PreviewsUKMObserverTest, PreviewsLikelyNotSet_PreviewsOff) {
  RunTest(content::PREVIEWS_OFF /* allowed_state */, PreviewsType::NONE,
          false /* lite_page_received */,
          false /* lite_page_redirect_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          true /* save_data_enabled */, false /* is_offline_preview */,
          CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
          base::nullopt /* navigation_restart_penalty */,
          base::nullopt /* hint_version_string */);

  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, false /* lite_page_expected */,
              false /* lite_page_redirect_expected */,
              false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              0 /* opt_out_value */, false /* origin_opt_out_expected */,
              true /* save_data_enabled_expected */,
              false /* offline_preview_expected */, false /* previews_likely */,
              CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
              base::nullopt /* navigation_restart_penalty */,
              base::nullopt /* hint_generation_timestamp */,
              base::nullopt /* hint_source */);
}

TEST_F(PreviewsUKMObserverTest, CoinFlipResult_Holdback) {
  RunTest(content::OFFLINE_PAGE_ON /* allowed_state */, PreviewsType::NONE,
          false /* lite_page_received */,
          false /* lite_page_redirect_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          true /* save_data_enabled */, true /* is_offline_preview */,
          CoinFlipHoldbackResult::kHoldback, {} /* eligibility_reasons */,
          base::nullopt /* navigation_restart_penalty */,
          base::nullopt /* hint_version_string */);

  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, false /* lite_page_expected */,
              false /* lite_page_redirect_expected */,
              false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              0 /* opt_out_value */, false /* origin_opt_out_expected */,
              true /* save_data_enabled_expected */,
              true /* offline_preview_expected */, true /* previews_likely */,
              CoinFlipHoldbackResult::kHoldback, {} /* eligibility_reasons */,
              base::nullopt /* navigation_restart_penalty */,
              base::nullopt /* hint_generation_timestamp */,
              base::nullopt /* hint_source */);
}

TEST_F(PreviewsUKMObserverTest, CoinFlipResult_Allowed) {
  RunTest(content::OFFLINE_PAGE_ON /* allowed_state */, PreviewsType::NONE,
          false /* lite_page_received */,
          false /* lite_page_redirect_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          true /* save_data_enabled */, true /* is_offline_preview */,
          CoinFlipHoldbackResult::kAllowed, {} /* eligibility_reasons */,
          base::nullopt /* navigation_restart_penalty */,
          base::nullopt /* hint_version_string */);

  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, false /* lite_page_expected */,
              false /* lite_page_redirect_expected */,
              false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              0 /* opt_out_value */, false /* origin_opt_out_expected */,
              true /* save_data_enabled_expected */,
              true /* offline_preview_expected */, true /* previews_likely */,
              CoinFlipHoldbackResult::kAllowed, {} /* eligibility_reasons */,
              base::nullopt /* navigation_restart_penalty */,
              base::nullopt /* hint_generation_timestamp */,
              base::nullopt /* hint_source */);
}

TEST_F(PreviewsUKMObserverTest, LogPreviewsEligibilityReason_WithAllowed) {
  RunTest(content::PREVIEWS_UNSPECIFIED /* allowed_state */, PreviewsType::NONE,
          false /* lite_page_received */,
          false /* lite_page_redirect_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          true /* save_data_enabled */, false /* is_offline_preview */,
          CoinFlipHoldbackResult::kNotSet,
          {{PreviewsType::OFFLINE,
            PreviewsEligibilityReason::BLACKLIST_UNAVAILABLE},
           {PreviewsType::LITE_PAGE,
            PreviewsEligibilityReason::BLACKLIST_UNAVAILABLE},
           {PreviewsType::NOSCRIPT,
            PreviewsEligibilityReason::BLACKLIST_DATA_NOT_LOADED},
           // ALLOWED is equal to zero and should not be recorded.
           {PreviewsType::LITE_PAGE_REDIRECT,
            PreviewsEligibilityReason::ALLOWED}} /* eligibility_reasons */,
          base::nullopt /* navigation_restart_penalty */,
          base::nullopt /* hint_version_string */);

  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, false /* lite_page_expected */,
              false /* lite_page_redirect_expected */,
              false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              0 /* opt_out_value */, false /* origin_opt_out_expected */,
              true /* save_data_enabled_expected */,
              false /* offline_preview_expected */, false /* previews_likely */,
              CoinFlipHoldbackResult::kNotSet,
              {{PreviewsType::OFFLINE,
                PreviewsEligibilityReason::BLACKLIST_UNAVAILABLE},
               {PreviewsType::LITE_PAGE,
                PreviewsEligibilityReason::BLACKLIST_UNAVAILABLE},
               {PreviewsType::NOSCRIPT,
                PreviewsEligibilityReason::
                    BLACKLIST_DATA_NOT_LOADED}} /* eligibility_reasons */,
              base::nullopt /* navigation_restart_penalty */,
              base::nullopt /* hint_generation_timestamp */,
              base::nullopt /* hint_source */);
}

TEST_F(PreviewsUKMObserverTest, LogPreviewsEligibilityReason_NoneAllowed) {
  RunTest(content::PREVIEWS_UNSPECIFIED /* allowed_state */, PreviewsType::NONE,
          false /* lite_page_received */,
          false /* lite_page_redirect_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          true /* save_data_enabled */, false /* is_offline_preview */,
          CoinFlipHoldbackResult::kNotSet,
          {{PreviewsType::OFFLINE,
            PreviewsEligibilityReason::BLACKLIST_UNAVAILABLE},
           {PreviewsType::LITE_PAGE,
            PreviewsEligibilityReason::BLACKLIST_UNAVAILABLE},
           {PreviewsType::NOSCRIPT,
            PreviewsEligibilityReason::BLACKLIST_DATA_NOT_LOADED},
           {PreviewsType::LITE_PAGE_REDIRECT,
            PreviewsEligibilityReason::
                BLACKLIST_DATA_NOT_LOADED}} /* eligibility_reasons */,
          base::nullopt /* navigation_restart_penalty */,
          base::nullopt /* hint_version_string */);

  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, false /* lite_page_expected */,
              false /* lite_page_redirect_expected */,
              false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              0 /* opt_out_value */, false /* origin_opt_out_expected */,
              true /* save_data_enabled_expected */,
              false /* offline_preview_expected */, false /* previews_likely */,
              CoinFlipHoldbackResult::kNotSet,
              {{PreviewsType::OFFLINE,
                PreviewsEligibilityReason::BLACKLIST_UNAVAILABLE},
               {PreviewsType::LITE_PAGE,
                PreviewsEligibilityReason::BLACKLIST_UNAVAILABLE},
               {PreviewsType::NOSCRIPT,
                PreviewsEligibilityReason::BLACKLIST_DATA_NOT_LOADED},
               {PreviewsType::LITE_PAGE_REDIRECT,
                PreviewsEligibilityReason::
                    BLACKLIST_DATA_NOT_LOADED}} /* eligibility_reasons */,
              base::nullopt /* navigation_restart_penalty */,
              base::nullopt /* hint_generation_timestamp */,
              base::nullopt /* hint_source */);
}

TEST_F(PreviewsUKMObserverTest, LogOptimizationGuideHintVersion_NoHintSource) {
  optimization_guide::proto::Version hint_version;
  hint_version.mutable_generation_timestamp()->set_seconds(123);
  std::string hint_version_string;
  hint_version.SerializeToString(&hint_version_string);
  RunTest(content::PREVIEWS_UNSPECIFIED /* allowed_state */, PreviewsType::NONE,
          false /* lite_page_received */,
          false /* lite_page_redirect_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          true /* save_data_enabled */, false /* is_offline_preview */,
          CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
          base::nullopt /* navigation_restart_penalty */, hint_version_string);

  NavigateToUntrackedUrl();

  ValidateUKM(
      false /* server_lofi_expected */, false /* client_lofi_expected */,
      false /* lite_page_expected */, false /* lite_page_redirect_expected */,
      false /* noscript_expected */,
      false /* resource_loading_hints_expected */, 0 /* opt_out_value */,
      false /* origin_opt_out_expected */,
      true /* save_data_enabled_expected */,
      false /* offline_preview_expected */, false /* previews_likely */,
      CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
      base::nullopt /* navigation_restart_penalty */,
      123 /* hint_generation_timestamp */, base::nullopt /* hint_source */);
}

TEST_F(PreviewsUKMObserverTest,
       LogOptimizationGuideHintVersion_NoHintGenerationTimestamp) {
  optimization_guide::proto::Version hint_version;
  hint_version.set_hint_source(
      optimization_guide::proto::HINT_SOURCE_OPTIMIZATION_HINTS_COMPONENT);
  std::string hint_version_string;
  hint_version.SerializeToString(&hint_version_string);
  RunTest(content::PREVIEWS_UNSPECIFIED /* allowed_state */, PreviewsType::NONE,
          false /* lite_page_received */,
          false /* lite_page_redirect_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          true /* save_data_enabled */, false /* is_offline_preview */,
          CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
          base::nullopt /* navigation_restart_penalty */, hint_version_string);

  NavigateToUntrackedUrl();

  ValidateUKM(
      false /* server_lofi_expected */, false /* client_lofi_expected */,
      false /* lite_page_expected */, false /* lite_page_redirect_expected */,
      false /* noscript_expected */,
      false /* resource_loading_hints_expected */, 0 /* opt_out_value */,
      false /* origin_opt_out_expected */,
      true /* save_data_enabled_expected */,
      false /* offline_preview_expected */, false /* previews_likely */,
      CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
      base::nullopt /* navigation_restart_penalty */,
      base::nullopt /* hint_generation_timestamp */, 1 /* hint_source */);
}

TEST_F(PreviewsUKMObserverTest,
       LogOptimizationGuideHintVersion_NotActuallyAVersionProto) {
  RunTest(content::PREVIEWS_UNSPECIFIED /* allowed_state */, PreviewsType::NONE,
          false /* lite_page_received */,
          false /* lite_page_redirect_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          true /* save_data_enabled */, false /* is_offline_preview */,
          CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
          base::nullopt /* navigation_restart_penalty */, "notahintversion");

  NavigateToUntrackedUrl();

  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, false /* lite_page_expected */,
              false /* lite_page_redirect_expected */,
              false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              0 /* opt_out_value */, false /* origin_opt_out_expected */,
              true /* save_data_enabled_expected */,
              false /* offline_preview_expected */, false /* previews_likely */,
              CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
              base::nullopt /* navigation_restart_penalty */,
              base::nullopt /* hint_generation_timestamp */,
              base::nullopt /* hint_source */);
}

TEST_F(PreviewsUKMObserverTest, CheckReportingForHidden) {
  RunTest(content::PREVIEWS_UNSPECIFIED /* allowed_state */, PreviewsType::NONE,
          false /* lite_page_received */,
          false /* lite_page_redirect_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          true /* save_data_enabled */, false /* is_offline_preview */,
          CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
          base::nullopt /* navigation_restart_penalty */,
          base::nullopt /* hint_version_string */);

  web_contents()->WasHidden();

  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, false /* lite_page_expected */,
              false /* lite_page_redirect_expected */,
              false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              0 /* opt_out_value */, false /* origin_opt_out_expected */,
              true /* save_data_enabled_expected */,
              false /* offline_preview_expected */, false /* previews_likely */,
              CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
              base::nullopt /* navigation_restart_penalty */,
              base::nullopt /* hint_generation_timestamp */,
              base::nullopt /* hint_source */);
}

TEST_F(PreviewsUKMObserverTest, CheckReportingForFlushMetrics) {
  RunTest(content::PREVIEWS_UNSPECIFIED /* allowed_state */, PreviewsType::NONE,
          false /* lite_page_received */,
          false /* lite_page_redirect_received */, false /* noscript_on */,
          false /* resource_loading_hints_on */, false /* origin_opt_out */,
          true /* save_data_enabled */, false /* is_offline_preview */,
          CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
          base::nullopt /* navigation_restart_penalty */,
          base::nullopt /* hint_version_string */);

  SimulateAppEnterBackground();

  ValidateUKM(false /* server_lofi_expected */,
              false /* client_lofi_expected */, false /* lite_page_expected */,
              false /* lite_page_redirect_expected */,
              false /* noscript_expected */,
              false /* resource_loading_hints_expected */,
              0 /* opt_out_value */, false /* origin_opt_out_expected */,
              true /* save_data_enabled_expected */,
              false /* offline_preview_expected */, false /* previews_likely */,
              CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
              base::nullopt /* navigation_restart_penalty */,
              base::nullopt /* hint_generation_timestamp */,
              base::nullopt /* hint_source */);
}

TEST_F(PreviewsUKMObserverTest, TestPageEndReasonUMA) {
  for (int i = static_cast<int>(PreviewsType::NONE);
       i < static_cast<int>(PreviewsType::LAST); i++) {
    PreviewsType type = static_cast<PreviewsType>(i);
    if (type == PreviewsType::DEPRECATED_AMP_REDIRECTION)
      continue;

    base::HistogramTester tester;
    RunTest(content::PREVIEWS_UNSPECIFIED /* allowed_state */, type,
            false /* lite_page_received */,
            false /* lite_page_redirect_received */, false /* noscript_on */,
            false /* resource_loading_hints_on */, false /* origin_opt_out */,
            false /* save_data_enabled */, false /* is_offline_preview */,
            CoinFlipHoldbackResult::kNotSet, {} /* eligibility_reasons */,
            base::nullopt /* navigation_restart_penalty */,
            base::nullopt /* hint_version_string */);

    NavigateToUntrackedUrl();

    // The top level metric is not recorded on a non-preview.
    if (type != PreviewsType::NONE) {
      tester.ExpectUniqueSample(
          "Previews.PageEndReason",
          page_load_metrics::PageEndReason::END_NEW_NAVIGATION, 1);
    }
    tester.ExpectUniqueSample(
        "Previews.PageEndReason." + GetStringNameForType(type),
        page_load_metrics::PageEndReason::END_NEW_NAVIGATION, 1);
  }
}

}  // namespace

}  // namespace previews
