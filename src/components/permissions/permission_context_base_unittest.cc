// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_context_base.h"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_request_id.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/test_permissions_client.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_util.h"

namespace permissions {

const char* const kPermissionsKillSwitchFieldStudy =
    PermissionContextBase::kPermissionsKillSwitchFieldStudy;
const char* const kPermissionsKillSwitchBlockedValue =
    PermissionContextBase::kPermissionsKillSwitchBlockedValue;
const char kPermissionsKillSwitchTestGroup[] = "TestGroup";

class TestPermissionContext : public PermissionContextBase {
 public:
  TestPermissionContext(content::BrowserContext* browser_context,
                        const ContentSettingsType content_settings_type)
      : PermissionContextBase(browser_context,
                              content_settings_type,
                              blink::mojom::FeaturePolicyFeature::kNotFound),
        tab_context_updated_(false) {}

  ~TestPermissionContext() override {}

  const std::vector<ContentSetting>& decisions() const { return decisions_; }

  bool tab_context_updated() const { return tab_context_updated_; }

  // Once a decision for the requested permission has been made, run the
  // callback.
  void TrackPermissionDecision(ContentSetting content_setting) {
    decisions_.push_back(content_setting);
    // Null check required here as the quit_closure_ can also be run and reset
    // first from within DecidePermission.
    if (quit_closure_) {
      quit_closure_.Run();
      quit_closure_.Reset();
    }
  }

  ContentSetting GetContentSettingFromMap(const GURL& url_a,
                                          const GURL& url_b) {
    auto* map = PermissionsClient::Get()->GetSettingsMap(browser_context());
    return map->GetContentSetting(url_a.GetOrigin(), url_b.GetOrigin(),
                                  content_settings_type(), std::string());
  }

  void RequestPermission(content::WebContents* web_contents,
                         const PermissionRequestID& id,
                         const GURL& requesting_frame,
                         bool user_gesture,
                         BrowserPermissionCallback callback) override {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    PermissionContextBase::RequestPermission(web_contents, id, requesting_frame,
                                             true /* user_gesture */,
                                             std::move(callback));
    run_loop.Run();
  }

  void DecidePermission(content::WebContents* web_contents,
                        const PermissionRequestID& id,
                        const GURL& requesting_origin,
                        const GURL& embedding_origin,
                        bool user_gesture,
                        BrowserPermissionCallback callback) override {
    PermissionContextBase::DecidePermission(web_contents, id, requesting_origin,
                                            embedding_origin, user_gesture,
                                            std::move(callback));
    if (respond_permission_) {
      respond_permission_.Run();
      respond_permission_.Reset();
    } else {
      // Stop the run loop from spinning indefinitely if no response callback
      // has been set, as is the case with TestParallelRequests.
      quit_closure_.Run();
      quit_closure_.Reset();
    }
  }

  // Set the callback to run if the permission is being responded to in the
  // test. This is left empty where no response is needed, such as in parallel
  // requests, invalid origin, and killswitch.
  void SetRespondPermissionCallback(base::Closure callback) {
    respond_permission_ = callback;
  }

 protected:
  void UpdateTabContext(const PermissionRequestID& id,
                        const GURL& requesting_origin,
                        bool allowed) override {
    tab_context_updated_ = true;
  }

  bool IsRestrictedToSecureOrigins() const override { return false; }

 private:
  std::vector<ContentSetting> decisions_;
  bool tab_context_updated_;
  base::Closure quit_closure_;
  // Callback for responding to a permission once the request has been completed
  // (valid URL, kill switch disabled)
  base::Closure respond_permission_;
  DISALLOW_COPY_AND_ASSIGN(TestPermissionContext);
};

class TestKillSwitchPermissionContext : public TestPermissionContext {
 public:
  TestKillSwitchPermissionContext(
      content::BrowserContext* browser_context,
      const ContentSettingsType content_settings_type)
      : TestPermissionContext(browser_context, content_settings_type) {
    ResetFieldTrialList();
  }

  void ResetFieldTrialList() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.Init();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(TestKillSwitchPermissionContext);
};

class TestSecureOriginRestrictedPermissionContext
    : public TestPermissionContext {
 public:
  TestSecureOriginRestrictedPermissionContext(
      content::BrowserContext* browser_context,
      const ContentSettingsType content_settings_type)
      : TestPermissionContext(browser_context, content_settings_type) {}

 protected:
  bool IsRestrictedToSecureOrigins() const override { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestSecureOriginRestrictedPermissionContext);
};

class TestPermissionsClientBypassExtensionOriginCheck
    : public TestPermissionsClient {
 public:
  bool CanBypassEmbeddingOriginCheck(const GURL& requesting_origin,
                                     const GURL& embedding_origin) override {
    return requesting_origin.SchemeIs("chrome-extension");
  }
};

class PermissionContextBaseTests : public content::RenderViewHostTestHarness {
 protected:
  PermissionContextBaseTests() {}
  ~PermissionContextBaseTests() override {}

  // Accept or dismiss the permission prompt.
  void RespondToPermission(TestPermissionContext* context,
                           const PermissionRequestID& id,
                           const GURL& url,
                           ContentSetting response) {
    DCHECK(response == CONTENT_SETTING_ALLOW ||
           response == CONTENT_SETTING_BLOCK ||
           response == CONTENT_SETTING_ASK);
    using AutoResponseType = PermissionRequestManager::AutoResponseType;
    AutoResponseType decision = AutoResponseType::DISMISS;
    if (response == CONTENT_SETTING_ALLOW)
      decision = AutoResponseType::ACCEPT_ALL;
    else if (response == CONTENT_SETTING_BLOCK)
      decision = AutoResponseType::DENY_ALL;
    prompt_factory_->set_response_type(decision);
  }

  void TestAskAndDecide_TestContent(ContentSettingsType content_settings_type,
                                    ContentSetting decision) {
    ukm::InitializeSourceUrlRecorderForWebContents(web_contents());
    ukm::TestAutoSetUkmRecorder ukm_recorder;
    TestPermissionContext permission_context(browser_context(),
                                             content_settings_type);
    GURL url("https://www.google.com");
    SetUpUrl(url);
    base::HistogramTester histograms;

    const PermissionRequestID id(
        web_contents()->GetMainFrame()->GetProcess()->GetID(),
        web_contents()->GetMainFrame()->GetRoutingID(), -1);
    permission_context.SetRespondPermissionCallback(base::Bind(
        &PermissionContextBaseTests::RespondToPermission,
        base::Unretained(this), &permission_context, id, url, decision));
    permission_context.RequestPermission(
        web_contents(), id, url, true /* user_gesture */,
        base::Bind(&TestPermissionContext::TrackPermissionDecision,
                   base::Unretained(&permission_context)));
    ASSERT_EQ(1u, permission_context.decisions().size());
    EXPECT_EQ(decision, permission_context.decisions()[0]);
    EXPECT_TRUE(permission_context.tab_context_updated());

    std::string decision_string;
    base::Optional<PermissionAction> action;
    if (decision == CONTENT_SETTING_ALLOW) {
      decision_string = "Accepted";
      action = PermissionAction::GRANTED;
    } else if (decision == CONTENT_SETTING_BLOCK) {
      decision_string = "Denied";
      action = PermissionAction::DENIED;
    } else if (decision == CONTENT_SETTING_ASK) {
      decision_string = "Dismissed";
      action = PermissionAction::DISMISSED;
    }

    if (!decision_string.empty()) {
      histograms.ExpectUniqueSample(
          "Permissions.Prompt." + decision_string + ".PriorDismissCount2." +
              PermissionUtil::GetPermissionString(content_settings_type),
          0, 1);
      histograms.ExpectUniqueSample(
          "Permissions.Prompt." + decision_string + ".PriorIgnoreCount2." +
              PermissionUtil::GetPermissionString(content_settings_type),
          0, 1);
#if defined(OS_ANDROID)
      histograms.ExpectUniqueSample(
          "Permissions.Action.WithDisposition.ModalDialog",
          static_cast<int>(action.value()), 1);
#else
      histograms.ExpectUniqueSample(
          "Permissions.Action.WithDisposition.AnchoredBubble",
          static_cast<int>(action.value()), 1);
#endif
    }

    EXPECT_EQ(decision, permission_context.GetContentSettingFromMap(url, url));

    histograms.ExpectUniqueSample(
        "Permissions.AutoBlocker.EmbargoPromptSuppression",
        static_cast<int>(PermissionEmbargoStatus::NOT_EMBARGOED), 1);
    histograms.ExpectUniqueSample(
        "Permissions.AutoBlocker.EmbargoStatus",
        static_cast<int>(PermissionEmbargoStatus::NOT_EMBARGOED), 1);

    if (action.has_value()) {
      auto entries = ukm_recorder.GetEntriesByName("Permission");
      EXPECT_EQ(1u, entries.size());
      auto* entry = entries.front();
      ukm_recorder.ExpectEntrySourceHasUrl(entry, url);

      size_t num_values = 0;

      EXPECT_NE(ContentSettingTypeToHistogramValue(content_settings_type,
                                                   &num_values),
                -1);

      EXPECT_EQ(*ukm_recorder.GetEntryMetric(entry, "Source"),
                static_cast<int64_t>(PermissionSourceUI::PROMPT));
      EXPECT_EQ(*ukm_recorder.GetEntryMetric(entry, "PermissionType"),
                static_cast<int64_t>(ContentSettingTypeToHistogramValue(
                    content_settings_type, &num_values)));
      EXPECT_EQ(*ukm_recorder.GetEntryMetric(entry, "Action"),
                static_cast<int64_t>(action.value()));

#if defined(OS_ANDROID)
      EXPECT_EQ(
          *ukm_recorder.GetEntryMetric(entry, "PromptDisposition"),
          static_cast<int64_t>(PermissionPromptDisposition::MODAL_DIALOG));
#else
      EXPECT_EQ(
          *ukm_recorder.GetEntryMetric(entry, "PromptDisposition"),
          static_cast<int64_t>(PermissionPromptDisposition::ANCHORED_BUBBLE));
#endif
    }
  }

  void DismissMultipleTimesAndExpectBlock(
      const GURL& url,
      ContentSettingsType content_settings_type,
      uint32_t iterations) {
    base::HistogramTester histograms;

    // Dismiss |iterations| times. The final dismiss should change the decision
    // from dismiss to block, and hence change the persisted content setting.
    for (uint32_t i = 0; i < iterations; ++i) {
      TestPermissionContext permission_context(browser_context(),
                                               content_settings_type);
      const PermissionRequestID id(
          web_contents()->GetMainFrame()->GetProcess()->GetID(),
          web_contents()->GetMainFrame()->GetRoutingID(), i);

      permission_context.SetRespondPermissionCallback(
          base::Bind(&PermissionContextBaseTests::RespondToPermission,
                     base::Unretained(this), &permission_context, id, url,
                     CONTENT_SETTING_ASK));

      permission_context.RequestPermission(
          web_contents(), id, url, true /* user_gesture */,
          base::Bind(&TestPermissionContext::TrackPermissionDecision,
                     base::Unretained(&permission_context)));
      histograms.ExpectTotalCount(
          "Permissions.Prompt.Dismissed.PriorDismissCount2." +
              PermissionUtil::GetPermissionString(content_settings_type),
          i + 1);
      histograms.ExpectBucketCount(
          "Permissions.Prompt.Dismissed.PriorDismissCount2." +
              PermissionUtil::GetPermissionString(content_settings_type),
          i, 1);

      histograms.ExpectTotalCount("Permissions.AutoBlocker.EmbargoStatus",
                                  i + 1);

      PermissionResult result = permission_context.GetPermissionStatus(
          nullptr /* render_frame_host */, url, url);

      histograms.ExpectUniqueSample(
          "Permissions.AutoBlocker.EmbargoPromptSuppression",
          static_cast<int>(PermissionEmbargoStatus::NOT_EMBARGOED), i + 1);
      if (i < 2) {
        EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);
        EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
        histograms.ExpectUniqueSample(
            "Permissions.AutoBlocker.EmbargoStatus",
            static_cast<int>(PermissionEmbargoStatus::NOT_EMBARGOED), i + 1);
      } else {
        EXPECT_EQ(PermissionStatusSource::MULTIPLE_DISMISSALS, result.source);
        EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
        histograms.ExpectBucketCount(
            "Permissions.AutoBlocker.EmbargoStatus",
            static_cast<int>(PermissionEmbargoStatus::REPEATED_DISMISSALS), 1);
      }

      ASSERT_EQ(1u, permission_context.decisions().size());
      EXPECT_EQ(CONTENT_SETTING_ASK, permission_context.decisions()[0]);
      EXPECT_TRUE(permission_context.tab_context_updated());
    }

    TestPermissionContext permission_context(browser_context(),
                                             content_settings_type);
    const PermissionRequestID id(
        web_contents()->GetMainFrame()->GetProcess()->GetID(),
        web_contents()->GetMainFrame()->GetRoutingID(), -1);

    permission_context.SetRespondPermissionCallback(
        base::Bind(&PermissionContextBaseTests::RespondToPermission,
                   base::Unretained(this), &permission_context, id, url,
                   CONTENT_SETTING_ASK));

    permission_context.RequestPermission(
        web_contents(), id, url, true /* user_gesture */,
        base::Bind(&TestPermissionContext::TrackPermissionDecision,
                   base::Unretained(&permission_context)));

    PermissionResult result = permission_context.GetPermissionStatus(
        nullptr /* render_frame_host */, url, url);
    EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
    EXPECT_EQ(PermissionStatusSource::MULTIPLE_DISMISSALS, result.source);
    histograms.ExpectBucketCount(
        "Permissions.AutoBlocker.EmbargoPromptSuppression",
        static_cast<int>(PermissionEmbargoStatus::REPEATED_DISMISSALS), 1);
  }

  void TestBlockOnSeveralDismissals_TestContent() {
    GURL url("https://www.google.com");
    SetUpUrl(url);
    base::HistogramTester histograms;

    {
      // Ensure that > 3 dismissals behaves correctly when the
      // BlockPromptsIfDismissedOften feature is off.
      base::test::ScopedFeatureList feature_list;
      feature_list.InitAndDisableFeature(
          features::kBlockPromptsIfDismissedOften);

      for (uint32_t i = 0; i < 4; ++i) {
        TestPermissionContext permission_context(
            browser_context(), ContentSettingsType::GEOLOCATION);

        const PermissionRequestID id(
            web_contents()->GetMainFrame()->GetProcess()->GetID(),
            web_contents()->GetMainFrame()->GetRoutingID(), i);

        permission_context.SetRespondPermissionCallback(
            base::Bind(&PermissionContextBaseTests::RespondToPermission,
                       base::Unretained(this), &permission_context, id, url,
                       CONTENT_SETTING_ASK));
        permission_context.RequestPermission(
            web_contents(), id, url, true /* user_gesture */,
            base::Bind(&TestPermissionContext::TrackPermissionDecision,
                       base::Unretained(&permission_context)));
        histograms.ExpectTotalCount(
            "Permissions.Prompt.Dismissed.PriorDismissCount2.Geolocation",
            i + 1);
        histograms.ExpectBucketCount(
            "Permissions.Prompt.Dismissed.PriorDismissCount2.Geolocation", i,
            1);
        histograms.ExpectUniqueSample(
            "Permissions.AutoBlocker.EmbargoPromptSuppression",
            static_cast<int>(PermissionEmbargoStatus::NOT_EMBARGOED), i + 1);
        histograms.ExpectUniqueSample(
            "Permissions.AutoBlocker.EmbargoStatus",
            static_cast<int>(PermissionEmbargoStatus::NOT_EMBARGOED), i + 1);

        ASSERT_EQ(1u, permission_context.decisions().size());
        EXPECT_EQ(CONTENT_SETTING_ASK, permission_context.decisions()[0]);
        EXPECT_TRUE(permission_context.tab_context_updated());
        EXPECT_EQ(CONTENT_SETTING_ASK,
                  permission_context.GetContentSettingFromMap(url, url));
      }

      // Flush the dismissal counts.
      auto* map = PermissionsClient::Get()->GetSettingsMap(browser_context());
      map->ClearSettingsForOneType(
          ContentSettingsType::PERMISSION_AUTOBLOCKER_DATA);
    }

    EXPECT_TRUE(
        base::FeatureList::IsEnabled(features::kBlockPromptsIfDismissedOften));

    // Sanity check independence per permission type by checking two of them.
    DismissMultipleTimesAndExpectBlock(url, ContentSettingsType::GEOLOCATION,
                                       3);
    DismissMultipleTimesAndExpectBlock(url, ContentSettingsType::NOTIFICATIONS,
                                       3);
  }

  void TestVariationBlockOnSeveralDismissals_TestContent() {
    GURL url("https://www.google.com");
    SetUpUrl(url);
    base::HistogramTester histograms;

    std::map<std::string, std::string> params;
    params
        [PermissionDecisionAutoBlocker::GetPromptDismissCountKeyForTesting()] =
            "5";
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        features::kBlockPromptsIfDismissedOften, params);

    std::map<std::string, std::string> actual_params;
    EXPECT_TRUE(base::GetFieldTrialParamsByFeature(
        features::kBlockPromptsIfDismissedOften, &actual_params));
    EXPECT_EQ(params, actual_params);

    {
      std::map<std::string, std::string> actual_params;
      EXPECT_TRUE(base::GetFieldTrialParamsByFeature(
          features::kBlockPromptsIfDismissedOften, &actual_params));
      EXPECT_EQ(params, actual_params);
    }

    for (uint32_t i = 0; i < 5; ++i) {
      TestPermissionContext permission_context(browser_context(),
                                               ContentSettingsType::MIDI_SYSEX);

      const PermissionRequestID id(
          web_contents()->GetMainFrame()->GetProcess()->GetID(),
          web_contents()->GetMainFrame()->GetRoutingID(), i);
      permission_context.SetRespondPermissionCallback(
          base::Bind(&PermissionContextBaseTests::RespondToPermission,
                     base::Unretained(this), &permission_context, id, url,
                     CONTENT_SETTING_ASK));
      permission_context.RequestPermission(
          web_contents(), id, url, true /* user_gesture */,
          base::Bind(&TestPermissionContext::TrackPermissionDecision,
                     base::Unretained(&permission_context)));

      EXPECT_EQ(1u, permission_context.decisions().size());
      ASSERT_EQ(CONTENT_SETTING_ASK, permission_context.decisions()[0]);
      EXPECT_TRUE(permission_context.tab_context_updated());
      PermissionResult result = permission_context.GetPermissionStatus(
          nullptr /* render_frame_host */, url, url);

      histograms.ExpectTotalCount(
          "Permissions.Prompt.Dismissed.PriorDismissCount2.MidiSysEx", i + 1);
      histograms.ExpectBucketCount(
          "Permissions.Prompt.Dismissed.PriorDismissCount2.MidiSysEx", i, 1);
      histograms.ExpectUniqueSample(
          "Permissions.AutoBlocker.EmbargoPromptSuppression",
          static_cast<int>(PermissionEmbargoStatus::NOT_EMBARGOED), i + 1);
      histograms.ExpectTotalCount("Permissions.AutoBlocker.EmbargoStatus",
                                  i + 1);
      if (i < 4) {
        EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
        EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);
        histograms.ExpectUniqueSample(
            "Permissions.AutoBlocker.EmbargoStatus",
            static_cast<int>(PermissionEmbargoStatus::NOT_EMBARGOED), i + 1);
      } else {
        EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
        EXPECT_EQ(PermissionStatusSource::MULTIPLE_DISMISSALS, result.source);
        histograms.ExpectBucketCount(
            "Permissions.AutoBlocker.EmbargoStatus",
            static_cast<int>(PermissionEmbargoStatus::REPEATED_DISMISSALS), 1);
      }
    }

    // Ensure that we finish in the block state.
    TestPermissionContext permission_context(browser_context(),
                                             ContentSettingsType::MIDI_SYSEX);
    PermissionResult result = permission_context.GetPermissionStatus(
        nullptr /* render_frame_host */, url, url);
    EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
    EXPECT_EQ(PermissionStatusSource::MULTIPLE_DISMISSALS, result.source);
  }

  void TestRequestPermissionInvalidUrl(
      ContentSettingsType content_settings_type) {
    base::HistogramTester histograms;
    TestPermissionContext permission_context(browser_context(),
                                             content_settings_type);
    GURL url;
    ASSERT_FALSE(url.is_valid());
    controller().LoadURL(url, content::Referrer(), ui::PAGE_TRANSITION_TYPED,
                         std::string());

    const PermissionRequestID id(
        web_contents()->GetMainFrame()->GetProcess()->GetID(),
        web_contents()->GetMainFrame()->GetRoutingID(), -1);
    permission_context.RequestPermission(
        web_contents(), id, url, true /* user_gesture */,
        base::Bind(&TestPermissionContext::TrackPermissionDecision,
                   base::Unretained(&permission_context)));

    ASSERT_EQ(1u, permission_context.decisions().size());
    EXPECT_EQ(CONTENT_SETTING_BLOCK, permission_context.decisions()[0]);
    EXPECT_TRUE(permission_context.tab_context_updated());
    EXPECT_EQ(CONTENT_SETTING_ASK,
              permission_context.GetContentSettingFromMap(url, url));
    histograms.ExpectTotalCount(
        "Permissions.AutoBlocker.EmbargoPromptSuppression", 0);
  }

  void TestGrantAndRevoke_TestContent(ContentSettingsType content_settings_type,
                                      ContentSetting expected_default) {
    TestPermissionContext permission_context(browser_context(),
                                             content_settings_type);
    GURL url("https://www.google.com");
    SetUpUrl(url);

    const PermissionRequestID id(
        web_contents()->GetMainFrame()->GetProcess()->GetID(),
        web_contents()->GetMainFrame()->GetRoutingID(), -1);
    permission_context.SetRespondPermissionCallback(
        base::Bind(&PermissionContextBaseTests::RespondToPermission,
                   base::Unretained(this), &permission_context, id, url,
                   CONTENT_SETTING_ALLOW));

    permission_context.RequestPermission(
        web_contents(), id, url, true /* user_gesture */,
        base::Bind(&TestPermissionContext::TrackPermissionDecision,
                   base::Unretained(&permission_context)));

    ASSERT_EQ(1u, permission_context.decisions().size());
    EXPECT_EQ(CONTENT_SETTING_ALLOW, permission_context.decisions()[0]);
    EXPECT_TRUE(permission_context.tab_context_updated());
    EXPECT_EQ(CONTENT_SETTING_ALLOW,
              permission_context.GetContentSettingFromMap(url, url));

    // Try to reset permission.
    permission_context.ResetPermission(url.GetOrigin(), url.GetOrigin());
    ContentSetting setting_after_reset =
        permission_context.GetContentSettingFromMap(url, url);
    ContentSetting default_setting =
        PermissionsClient::Get()
            ->GetSettingsMap(browser_context())
            ->GetDefaultContentSetting(content_settings_type, nullptr);
    EXPECT_EQ(default_setting, setting_after_reset);
  }

  void TestGlobalPermissionsKillSwitch(
      ContentSettingsType content_settings_type) {
    TestKillSwitchPermissionContext permission_context(browser_context(),
                                                       content_settings_type);
    permission_context.ResetFieldTrialList();

    EXPECT_FALSE(permission_context.IsPermissionKillSwitchOn());
    std::map<std::string, std::string> params;
    params[PermissionUtil::GetPermissionString(content_settings_type)] =
        kPermissionsKillSwitchBlockedValue;
    base::AssociateFieldTrialParams(kPermissionsKillSwitchFieldStudy,
                                    kPermissionsKillSwitchTestGroup, params);
    base::FieldTrialList::CreateFieldTrial(kPermissionsKillSwitchFieldStudy,
                                           kPermissionsKillSwitchTestGroup);
    EXPECT_TRUE(permission_context.IsPermissionKillSwitchOn());
  }

  void TestSecureOriginRestrictedPermissionContextCheck(
      const std::string& requesting_url_spec,
      const std::string& embedding_url_spec,
      bool expect_allowed) {
    GURL requesting_origin(requesting_url_spec);
    GURL embedding_origin(embedding_url_spec);
    TestSecureOriginRestrictedPermissionContext permission_context(
        browser_context(), ContentSettingsType::GEOLOCATION);
    bool result = permission_context.IsPermissionAvailableToOrigins(
        requesting_origin, embedding_origin);
    EXPECT_EQ(expect_allowed, result)
        << "test case (requesting, embedding): (" << requesting_url_spec << ", "
        << embedding_url_spec << ") with secure-origin requirement"
        << " on";

    // With no secure-origin limitation, this check should always return pass.
    TestPermissionContext new_context(browser_context(),
                                      ContentSettingsType::GEOLOCATION);
    result = new_context.IsPermissionAvailableToOrigins(requesting_origin,
                                                        embedding_origin);
    EXPECT_EQ(true, result)
        << "test case (requesting, embedding): (" << requesting_url_spec << ", "
        << embedding_url_spec << ") with secure-origin requirement"
        << " off";
  }

  // Don't call this more than once in the same test, as it persists data to
  // HostContentSettingsMap.
  void TestParallelRequests(ContentSetting response) {
    TestPermissionContext permission_context(browser_context(),
                                             ContentSettingsType::GEOLOCATION);
    GURL url("http://www.google.com");
    SetUpUrl(url);

    const PermissionRequestID id0(
        web_contents()->GetMainFrame()->GetProcess()->GetID(),
        web_contents()->GetMainFrame()->GetRoutingID(), 0);
    const PermissionRequestID id1(
        web_contents()->GetMainFrame()->GetProcess()->GetID(),
        web_contents()->GetMainFrame()->GetRoutingID(), 1);

    // Request a permission without setting the callback to DecidePermission.
    permission_context.RequestPermission(
        web_contents(), id0, url, true /* user_gesture */,
        base::Bind(&TestPermissionContext::TrackPermissionDecision,
                   base::Unretained(&permission_context)));

    EXPECT_EQ(0u, permission_context.decisions().size());

    // Set the callback, and make a second permission request.
    permission_context.SetRespondPermissionCallback(base::Bind(
        &PermissionContextBaseTests::RespondToPermission,
        base::Unretained(this), &permission_context, id0, url, response));
    permission_context.RequestPermission(
        web_contents(), id1, url, true /* user_gesture */,
        base::Bind(&TestPermissionContext::TrackPermissionDecision,
                   base::Unretained(&permission_context)));

    ASSERT_EQ(2u, permission_context.decisions().size());
    EXPECT_EQ(response, permission_context.decisions()[0]);
    EXPECT_EQ(response, permission_context.decisions()[1]);
    EXPECT_TRUE(permission_context.tab_context_updated());

    EXPECT_EQ(response, permission_context.GetContentSettingFromMap(url, url));
  }

  void TestVirtualURL(const GURL& loaded_url,
                      const GURL& virtual_url,
                      const ContentSetting want_response,
                      const PermissionStatusSource& want_source) {
    TestPermissionContext permission_context(browser_context(),
                                             ContentSettingsType::GEOLOCATION);

    NavigateAndCommit(loaded_url);
    web_contents()->GetController().GetVisibleEntry()->SetVirtualURL(
        virtual_url);

    PermissionResult result = permission_context.GetPermissionStatus(
        web_contents()->GetMainFrame(), virtual_url, virtual_url);
    EXPECT_EQ(result.content_setting, want_response);
    EXPECT_EQ(result.source, want_source);
  }

  void SetUpUrl(const GURL& url) {
    NavigateAndCommit(url);
    prompt_factory_->DocumentOnLoadCompletedInMainFrame();
  }

 private:
  // content::RenderViewHostTestHarness:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    PermissionRequestManager::CreateForWebContents(web_contents());
    PermissionRequestManager* manager =
        PermissionRequestManager::FromWebContents(web_contents());
    prompt_factory_.reset(new MockPermissionPromptFactory(manager));
  }

  void TearDown() override {
    prompt_factory_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<MockPermissionPromptFactory> prompt_factory_;
  TestPermissionsClientBypassExtensionOriginCheck client_;

  DISALLOW_COPY_AND_ASSIGN(PermissionContextBaseTests);
};

// Simulates clicking Accept. The permission should be granted and
// saved for future use.
TEST_F(PermissionContextBaseTests, TestAskAndGrant) {
  TestAskAndDecide_TestContent(ContentSettingsType::NOTIFICATIONS,
                               CONTENT_SETTING_ALLOW);
}

// Simulates clicking Block. The permission should be denied and
// saved for future use.
TEST_F(PermissionContextBaseTests, TestAskAndBlock) {
  TestAskAndDecide_TestContent(ContentSettingsType::GEOLOCATION,
                               CONTENT_SETTING_BLOCK);
}

// Simulates clicking Dismiss (X) in the prompt.
// The permission should be denied but not saved for future use.
TEST_F(PermissionContextBaseTests, TestAskAndDismiss) {
  TestAskAndDecide_TestContent(ContentSettingsType::MIDI_SYSEX,
                               CONTENT_SETTING_ASK);
}

// Simulates clicking Dismiss (X) in the prompt with the block on too
// many dismissals feature active. The permission should be blocked after
// several dismissals.
TEST_F(PermissionContextBaseTests, TestDismissUntilBlocked) {
  TestBlockOnSeveralDismissals_TestContent();
}

// Test setting a custom number of dismissals before block via variations.
TEST_F(PermissionContextBaseTests, TestDismissVariations) {
  TestVariationBlockOnSeveralDismissals_TestContent();
}

// Simulates non-valid requesting URL.
// The permission should be denied but not saved for future use.
TEST_F(PermissionContextBaseTests, TestNonValidRequestingUrl) {
  TestRequestPermissionInvalidUrl(ContentSettingsType::GEOLOCATION);
  TestRequestPermissionInvalidUrl(ContentSettingsType::NOTIFICATIONS);
  TestRequestPermissionInvalidUrl(ContentSettingsType::MIDI_SYSEX);
#if defined(OS_CHROMEOS)
  TestRequestPermissionInvalidUrl(
      ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER);
#endif
}

// Simulates granting and revoking of permissions.
TEST_F(PermissionContextBaseTests, TestGrantAndRevoke) {
  TestGrantAndRevoke_TestContent(ContentSettingsType::GEOLOCATION,
                                 CONTENT_SETTING_ASK);
  TestGrantAndRevoke_TestContent(ContentSettingsType::MIDI_SYSEX,
                                 CONTENT_SETTING_ASK);
#if defined(OS_ANDROID)
  TestGrantAndRevoke_TestContent(
      ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER, CONTENT_SETTING_ASK);
  // TODO(timvolodine): currently no test for
  // ContentSettingsType::NOTIFICATIONS because notification permissions work
  // differently with infobars as compared to bubbles (crbug.com/453784).
#else
  TestGrantAndRevoke_TestContent(ContentSettingsType::NOTIFICATIONS,
                                 CONTENT_SETTING_ASK);
#endif
}

// Tests the global kill switch by enabling/disabling the Field Trials.
TEST_F(PermissionContextBaseTests, TestGlobalKillSwitch) {
  TestGlobalPermissionsKillSwitch(ContentSettingsType::GEOLOCATION);
  TestGlobalPermissionsKillSwitch(ContentSettingsType::NOTIFICATIONS);
  TestGlobalPermissionsKillSwitch(ContentSettingsType::MIDI_SYSEX);
  TestGlobalPermissionsKillSwitch(ContentSettingsType::DURABLE_STORAGE);
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  TestGlobalPermissionsKillSwitch(
      ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER);
#endif
  TestGlobalPermissionsKillSwitch(ContentSettingsType::MEDIASTREAM_MIC);
  TestGlobalPermissionsKillSwitch(ContentSettingsType::MEDIASTREAM_CAMERA);
}

// Tests that secure origins are examined if switch is on, or ignored if off.
TEST_F(PermissionContextBaseTests,
       TestSecureOriginRestrictedPermissionContextSwitch) {
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddSecureScheme("chrome-extension");
  struct {
    std::string requesting_url_spec;
    std::string embedding_url_spec;
    bool expect_permission_allowed;
  } kTestCases[] = {
      // Secure-origins that should be allowed.
      {"https://google.com", "https://foo.com",
       /*expect_allowed=*/true},
      {"https://www.bar.com", "https://foo.com",
       /*expect_allowed=*/true},
      {"https://localhost", "http://localhost",
       /*expect_allowed=*/true},

      {"http://localhost", "https://google.com",
       /*expect_allowed=*/true},
      {"https://google.com", "http://localhost",
       /*expect_allowed=*/true},
      {"https://foo.com", "file://some-file",
       /*expect_allowed=*/true},
      {"file://some-file", "https://foo.com",
       /*expect_allowed=*/true},
      {"https://foo.com", "about:blank",
       /*expect_allowed=*/true},
      {"about:blank", "https://foo.com",
       /*expect_allowed=*/true},

      // Extensions are exempt from checking the embedder chain.
      {"chrome-extension://some-extension", "http://not-secure.com",
       /*expect_allowed=*/true},

      // Insecure-origins that should be blocked.
      {"http://foo.com", "file://some-file",
       /*expect_allowed=*/false},
      {"fake://foo.com", "about:blank",
       /*expect_allowed=*/false},
      {"http://localhost", "http://foo.com",
       /*expect_allowed=*/false},
      {"http://localhost", "foo.com",
       /*expect_allowed=*/false},
      {"http://bar.com", "https://foo.com",
       /*expect_permission_allowed=*/false},
      {"https://foo.com", "http://bar.com",
       /*expect_permission_allowed=*/false},
      {"http://localhost", "http://foo.com",
       /*expect_permission_allowed=*/false},
      {"http://foo.com", "http://localhost",
       /*expect_permission_allowed=*/false},
      {"bar.com", "https://foo.com", /*expect_permission_allowed=*/false},
      {"https://foo.com", "bar.com", /*expect_permission_allowed=*/false}};
  for (const auto& test_case : kTestCases) {
    TestSecureOriginRestrictedPermissionContextCheck(
        test_case.requesting_url_spec, test_case.embedding_url_spec,
        test_case.expect_permission_allowed);
  }
}

TEST_F(PermissionContextBaseTests, TestParallelRequestsAllowed) {
  TestParallelRequests(CONTENT_SETTING_ALLOW);
}

TEST_F(PermissionContextBaseTests, TestParallelRequestsBlocked) {
  TestParallelRequests(CONTENT_SETTING_BLOCK);
}

TEST_F(PermissionContextBaseTests, TestParallelRequestsDismissed) {
  TestParallelRequests(CONTENT_SETTING_ASK);
}

TEST_F(PermissionContextBaseTests, TestVirtualURLDifferentOrigin) {
  TestVirtualURL(GURL("http://www.google.com"), GURL("http://foo.com"),
                 CONTENT_SETTING_BLOCK,
                 PermissionStatusSource::VIRTUAL_URL_DIFFERENT_ORIGIN);
}

TEST_F(PermissionContextBaseTests, TestVirtualURLNotHTTP) {
  TestVirtualURL(GURL("chrome://foo"), GURL("chrome://newtab"),
                 CONTENT_SETTING_ASK, PermissionStatusSource::UNSPECIFIED);
}

TEST_F(PermissionContextBaseTests, TestVirtualURLSameOrigin) {
  TestVirtualURL(GURL("http://www.google.com"),
                 GURL("http://www.google.com/foo"), CONTENT_SETTING_ASK,
                 PermissionStatusSource::UNSPECIFIED);
}

}  // namespace permissions
