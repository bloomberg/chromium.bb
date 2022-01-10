// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_config.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_state.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/content_setting_bubble_contents.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/permission_request_chip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/user_education/feature_promo_controller_views.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_ui_selector.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/mock_permission_request.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/test/button_test_api.h"

namespace {
// Test implementation of PermissionUiSelector that always returns a canned
// decision.
class TestQuietNotificationPermissionUiSelector
    : public permissions::PermissionUiSelector {
 public:
  explicit TestQuietNotificationPermissionUiSelector(
      const Decision& canned_decision)
      : canned_decision_(canned_decision) {}
  ~TestQuietNotificationPermissionUiSelector() override = default;

 protected:
  // permissions::PermissionUiSelector:
  void SelectUiToUse(permissions::PermissionRequest* request,
                     DecisionMadeCallback callback) override {
    std::move(callback).Run(canned_decision_);
  }

  bool IsPermissionRequestSupported(
      permissions::RequestType request_type) override {
    return request_type == permissions::RequestType::kNotifications;
  }

 private:
  Decision canned_decision_;
};
}  // namespace

class PermissionChipInteractiveTest : public InProcessBrowserTest {
 public:
  PermissionChipInteractiveTest() {
    scoped_feature_list_.InitWithFeatures(
        {permissions::features::kPermissionChip},
        {permissions::features::kPermissionChipGestureSensitive,
         permissions::features::kPermissionChipRequestTypeSensitive});
  }

  PermissionChipInteractiveTest(const PermissionChipInteractiveTest&) = delete;
  PermissionChipInteractiveTest& operator=(
      const PermissionChipInteractiveTest&) = delete;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    test_api_ =
        std::make_unique<test::PermissionRequestManagerTestApi>(browser());
  }

  content::RenderFrameHost* GetActiveMainFrame() {
    return browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
  }

  void RequestPermission(permissions::RequestType type) {
    GURL requesting_origin("https://example.com");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), requesting_origin));
    test_api_->AddSimpleRequest(GetActiveMainFrame(), type);
    base::RunLoop().RunUntilIdle();
  }

  PermissionChip* GetChip() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    return browser_view->toolbar()->location_bar()->chip();
  }

  void ClickOnChip(PermissionChip* chip) {
    ASSERT_TRUE(chip != nullptr);
    views::test::ButtonTestApi(chip->button())
        .NotifyClick(ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(),
                                    gfx::Point(), ui::EventTimeForNow(),
                                    ui::EF_LEFT_MOUSE_BUTTON, 0));
    base::RunLoop().RunUntilIdle();
  }

  void ExpectQuietAbusiveChip() {
    // PermissionChip lifetime is bound to a permission prompt view.
    ASSERT_TRUE(test_api_->manager()->view_for_testing());
    // The quiet chip will be shown even if the chip experiment is disabled.
    PermissionChip* chip = GetChip();
    ASSERT_TRUE(chip);

    EXPECT_FALSE(chip->should_expand_for_testing());
    EXPECT_FALSE(chip->get_chip_button_for_testing()->is_animating());
    EXPECT_EQ(OmniboxChipButton::Theme::kLowVisibility,
              chip->get_chip_button_for_testing()->get_theme_for_testing());
  }

  void ExpectQuietChip() {
    // PermissionChip lifetime is bound to a permission prompt view.
    ASSERT_TRUE(test_api_->manager()->view_for_testing());

    // The quiet chip will be shown even if the chip experiment is disabled.
    PermissionChip* chip = GetChip();
    ASSERT_TRUE(chip);

    EXPECT_TRUE(chip->should_expand_for_testing());
    EXPECT_TRUE(chip->get_chip_button_for_testing()->is_animating());
    EXPECT_EQ(OmniboxChipButton::Theme::kLowVisibility,
              chip->get_chip_button_for_testing()->get_theme_for_testing());
  }

  void ExpectNormalChip() {
    // PermissionChip lifetime is bound to a permission prompt view.
    ASSERT_TRUE(test_api_->manager()->view_for_testing());
    PermissionChip* chip = GetChip();
    ASSERT_TRUE(chip);

    EXPECT_TRUE(chip->should_expand_for_testing());
    EXPECT_TRUE(chip->get_chip_button_for_testing()->is_animating());
    // TODO(crbug.com/1232460): Verify that OmniboxChipButton::is_animating is
    // true. Right now the value is flaky.
    EXPECT_EQ(OmniboxChipButton::Theme::kNormalVisibility,
              chip->get_chip_button_for_testing()->get_theme_for_testing());
  }

  ContentSettingImageView& GetContentSettingImageView(
      ContentSettingImageModel::ImageType image_type) {
    LocationBarView* location_bar_view =
        BrowserView::GetBrowserViewForBrowser(browser())->GetLocationBarView();
    return **std::find_if(
        location_bar_view->GetContentSettingViewsForTest().begin(),
        location_bar_view->GetContentSettingViewsForTest().end(),
        [image_type](ContentSettingImageView* view) {
          return view->GetTypeForTesting() == image_type;
        });
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<test::PermissionRequestManagerTestApi> test_api_;
};

IN_PROC_BROWSER_TEST_F(PermissionChipInteractiveTest,
                       ChipAutoPopupBubbleDisabled) {
  ASSERT_FALSE(base::FeatureList::IsEnabled(
      permissions::features::kPermissionChipGestureSensitive));
  ASSERT_FALSE(base::FeatureList::IsEnabled(
      permissions::features::kPermissionChipRequestTypeSensitive));

  RequestPermission(permissions::RequestType::kGeolocation);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::LOCATION_BAR_LEFT_CHIP);

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  RequestPermission(permissions::RequestType::kNotifications);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::LOCATION_BAR_LEFT_CHIP);

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  RequestPermission(permissions::RequestType::kMidiSysex);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::LOCATION_BAR_LEFT_CHIP);
}

class ChipGestureSensitiveEnabledInteractiveTest
    : public PermissionChipInteractiveTest {
 public:
  ChipGestureSensitiveEnabledInteractiveTest() {
    scoped_feature_list_.InitAndEnableFeature(
        permissions::features::kPermissionChipGestureSensitive);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ChipGestureSensitiveEnabledInteractiveTest,
                       ChipAutoPopupBubbleEnabled) {
  ASSERT_TRUE(base::FeatureList::IsEnabled(
      permissions::features::kPermissionChipGestureSensitive));
  ASSERT_FALSE(base::FeatureList::IsEnabled(
      permissions::features::kPermissionChipRequestTypeSensitive));

  RequestPermission(permissions::RequestType::kGeolocation);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE);

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  RequestPermission(permissions::RequestType::kNotifications);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE);

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  RequestPermission(permissions::RequestType::kMidiSysex);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE);
}

class ChipRequestTypeSensitiveEnabledInteractiveTest
    : public PermissionChipInteractiveTest {
 public:
  ChipRequestTypeSensitiveEnabledInteractiveTest() {
    scoped_feature_list_.InitAndEnableFeature(
        permissions::features::kPermissionChipRequestTypeSensitive);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// `kGeolocation` and `kNotifications` are excluded from the sensitive request
// type experiment.
IN_PROC_BROWSER_TEST_F(ChipRequestTypeSensitiveEnabledInteractiveTest,
                       ChipAutoPopupBubbleEnabled) {
  ASSERT_FALSE(base::FeatureList::IsEnabled(
      permissions::features::kPermissionChipGestureSensitive));
  ASSERT_TRUE(base::FeatureList::IsEnabled(
      permissions::features::kPermissionChipRequestTypeSensitive));

  RequestPermission(permissions::RequestType::kGeolocation);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::LOCATION_BAR_LEFT_CHIP);

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  RequestPermission(permissions::RequestType::kNotifications);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::LOCATION_BAR_LEFT_CHIP);

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  RequestPermission(permissions::RequestType::kMidiSysex);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE);
}

class ChipDisabledInteractiveTest : public PermissionChipInteractiveTest {
 public:
  ChipDisabledInteractiveTest() {
    scoped_feature_list_.InitWithFeatures(
        {permissions::features::kPermissionChipGestureSensitive,
         permissions::features::kPermissionChipRequestTypeSensitive},
        {permissions::features::kPermissionChip});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ChipDisabledInteractiveTest,
                       ChipAutoPopupBubbleEnabled) {
  ASSERT_FALSE(
      base::FeatureList::IsEnabled(permissions::features::kPermissionChip));
  ASSERT_TRUE(base::FeatureList::IsEnabled(
      permissions::features::kPermissionChipGestureSensitive));
  ASSERT_TRUE(base::FeatureList::IsEnabled(
      permissions::features::kPermissionChipRequestTypeSensitive));

  RequestPermission(permissions::RequestType::kGeolocation);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::ANCHORED_BUBBLE);

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  RequestPermission(permissions::RequestType::kNotifications);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::ANCHORED_BUBBLE);

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  RequestPermission(permissions::RequestType::kMidiSysex);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::ANCHORED_BUBBLE);
}

class QuietChipAutoPopupBubbleInteractiveTest
    : public PermissionChipInteractiveTest {
 public:
  QuietChipAutoPopupBubbleInteractiveTest() {
    scoped_feature_list_.InitWithFeatures(
        {permissions::features::kPermissionChip,
         features::kQuietNotificationPrompts,
         permissions::features::kPermissionQuietChip,
         permissions::features::kPermissionChipGestureSensitive,
         permissions::features::kPermissionChipRequestTypeSensitive},
        {});
  }

 protected:
  using QuietUiReason = permissions::PermissionUiSelector::QuietUiReason;
  using WarningReason = permissions::PermissionUiSelector::WarningReason;

  void SetCannedUiDecision(absl::optional<QuietUiReason> quiet_ui_reason,
                           absl::optional<WarningReason> warning_reason) {
    test_api_->manager()->set_permission_ui_selector_for_testing(
        std::make_unique<TestQuietNotificationPermissionUiSelector>(
            permissions::PermissionUiSelector::Decision(quiet_ui_reason,
                                                        warning_reason)));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(QuietChipAutoPopupBubbleInteractiveTest,
                       IgnoreChipHistogramsTest) {
  base::HistogramTester histograms;

  RequestPermission(permissions::RequestType::kGeolocation);

  ASSERT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE);

  test_api_->manager()->Ignore();

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount(
      "Permissions.Prompt.Geolocation.LocationBarLeftChipAutoBubble.Action",
      static_cast<int>(permissions::PermissionAction::IGNORED), 1);
}

IN_PROC_BROWSER_TEST_F(QuietChipAutoPopupBubbleInteractiveTest,
                       GrantedChipHistogramsTest) {
  base::HistogramTester histograms;

  RequestPermission(permissions::RequestType::kGeolocation);

  ASSERT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE);

  test_api_->manager()->Accept();

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount(
      "Permissions.Prompt.Geolocation.LocationBarLeftChipAutoBubble.Action",
      static_cast<int>(permissions::PermissionAction::GRANTED), 1);
}

IN_PROC_BROWSER_TEST_F(QuietChipAutoPopupBubbleInteractiveTest,
                       DeniedChipHistogramsTest) {
  base::HistogramTester histograms;

  RequestPermission(permissions::RequestType::kGeolocation);

  ASSERT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE);

  test_api_->manager()->Deny();

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount(
      "Permissions.Prompt.Geolocation.LocationBarLeftChipAutoBubble.Action",
      static_cast<int>(permissions::PermissionAction::DENIED), 1);
}

IN_PROC_BROWSER_TEST_F(QuietChipAutoPopupBubbleInteractiveTest,
                       DismissedChipHistogramsTest) {
  base::HistogramTester histograms;

  RequestPermission(permissions::RequestType::kGeolocation);

  ASSERT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE);
  base::TimeDelta duration = base::Milliseconds(42);
  test_api_->manager()->set_time_to_decision_for_test(duration);

  test_api_->manager()->Dismiss();

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount(
      "Permissions.Prompt.Geolocation.LocationBarLeftChipAutoBubble.Action",
      static_cast<int>(permissions::PermissionAction::DISMISSED), 1);

  histograms.ExpectTimeBucketCount(
      "Permissions.Prompt.Geolocation.LocationBarLeftChipAutoBubble.Dismissed."
      "TimeToAction",
      duration, 1);
}

IN_PROC_BROWSER_TEST_F(QuietChipAutoPopupBubbleInteractiveTest,
                       QuietChipNonAbusiveUmaTest) {
  base::HistogramTester histograms;

  for (QuietUiReason reason : {QuietUiReason::kEnabledInPrefs,
                               QuietUiReason::kPredictedVeryUnlikelyGrant}) {
    SetCannedUiDecision(reason, absl::nullopt);

    RequestPermission(permissions::RequestType::kNotifications);

    ASSERT_EQ(
        test_api_->manager()->current_request_prompt_disposition_for_testing(),
        permissions::PermissionPromptDisposition::LOCATION_BAR_LEFT_QUIET_CHIP);

    ClickOnChip(GetChip());

    test_api_->manager()->Ignore();
    base::RunLoop().RunUntilIdle();
  }

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount(
      "Permissions.Prompt.Notifications.LocationBarLeftQuietChip.Ignored."
      "DidShowBubble",
      static_cast<int>(true), 2);

  histograms.ExpectBucketCount(
      "Permissions.Prompt.Notifications.LocationBarLeftQuietChip.Ignored."
      "DidClickManage",
      static_cast<int>(false), 2);
}

IN_PROC_BROWSER_TEST_F(QuietChipAutoPopupBubbleInteractiveTest,
                       QuietChipNonAbusiveClickManageUmaTest) {
  base::HistogramTester histograms;

  for (QuietUiReason reason : {QuietUiReason::kEnabledInPrefs,
                               QuietUiReason::kPredictedVeryUnlikelyGrant}) {
    SetCannedUiDecision(reason, absl::nullopt);

    RequestPermission(permissions::RequestType::kNotifications);

    ASSERT_EQ(
        test_api_->manager()->current_request_prompt_disposition_for_testing(),
        permissions::PermissionPromptDisposition::LOCATION_BAR_LEFT_QUIET_CHIP);

    ClickOnChip(GetChip());

    views::View* bubble_view = GetChip()->get_prompt_bubble_view_for_testing();
    ContentSettingBubbleContents* permission_prompt_bubble =
        static_cast<ContentSettingBubbleContents*>(bubble_view);

    ASSERT_TRUE(permission_prompt_bubble != nullptr);

    permission_prompt_bubble->managed_button_clicked_for_test();

    test_api_->manager()->Ignore();
    base::RunLoop().RunUntilIdle();
  }

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount(
      "Permissions.Prompt.Notifications.LocationBarLeftQuietChip.Ignored."
      "DidShowBubble",
      static_cast<int>(true), 2);

  histograms.ExpectBucketCount(
      "Permissions.Prompt.Notifications.LocationBarLeftQuietChip.Ignored."
      "DidClickManage",
      static_cast<int>(true), 2);
}

IN_PROC_BROWSER_TEST_F(QuietChipAutoPopupBubbleInteractiveTest,
                       QuietChipAbusiveUmaTest) {
  base::HistogramTester histograms;

  for (QuietUiReason reason : {QuietUiReason::kTriggeredByCrowdDeny,
                               QuietUiReason::kTriggeredDueToAbusiveRequests,
                               QuietUiReason::kTriggeredDueToAbusiveContent}) {
    SetCannedUiDecision(reason, absl::nullopt);

    RequestPermission(permissions::RequestType::kNotifications);

    ASSERT_EQ(
        test_api_->manager()->current_request_prompt_disposition_for_testing(),
        permissions::PermissionPromptDisposition::
            LOCATION_BAR_LEFT_QUIET_ABUSIVE_CHIP);

    ClickOnChip(GetChip());

    test_api_->manager()->Ignore();
    base::RunLoop().RunUntilIdle();
  }

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount(
      "Permissions.Prompt.Notifications.LocationBarLeftQuietAbusiveChip."
      "Ignored."
      "DidShowBubble",
      static_cast<int>(true), 3);

  histograms.ExpectBucketCount(
      "Permissions.Prompt.Notifications.LocationBarLeftQuietAbusiveChip."
      "Ignored.DidClickLearnMore",
      static_cast<int>(false), 3);
}

IN_PROC_BROWSER_TEST_F(QuietChipAutoPopupBubbleInteractiveTest,
                       QuietChipAbusiveClickLearnMoreUmaTest) {
  base::HistogramTester histograms;

  for (QuietUiReason reason : {QuietUiReason::kTriggeredByCrowdDeny,
                               QuietUiReason::kTriggeredDueToAbusiveRequests,
                               QuietUiReason::kTriggeredDueToAbusiveContent}) {
    SetCannedUiDecision(reason, absl::nullopt);

    RequestPermission(permissions::RequestType::kNotifications);

    ASSERT_EQ(
        test_api_->manager()->current_request_prompt_disposition_for_testing(),
        permissions::PermissionPromptDisposition::
            LOCATION_BAR_LEFT_QUIET_ABUSIVE_CHIP);

    ClickOnChip(GetChip());

    views::View* bubble_view = GetChip()->get_prompt_bubble_view_for_testing();
    ContentSettingBubbleContents* permission_prompt_bubble =
        static_cast<ContentSettingBubbleContents*>(bubble_view);

    ASSERT_TRUE(permission_prompt_bubble != nullptr);

    permission_prompt_bubble->learn_more_button_clicked_for_test();

    test_api_->manager()->Ignore();
    base::RunLoop().RunUntilIdle();
  }

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount(
      "Permissions.Prompt.Notifications.LocationBarLeftQuietAbusiveChip."
      "Ignored."
      "DidShowBubble",
      static_cast<int>(true), 3);

  histograms.ExpectBucketCount(
      "Permissions.Prompt.Notifications.LocationBarLeftQuietAbusiveChip."
      "Ignored.DidClickLearnMore",
      static_cast<int>(true), 3);
}

IN_PROC_BROWSER_TEST_F(QuietChipAutoPopupBubbleInteractiveTest,
                       QuietChipAutoPopupBubbleEnabled) {
  ASSERT_TRUE(
      base::FeatureList::IsEnabled(permissions::features::kPermissionChip));
  ASSERT_TRUE(
      base::FeatureList::IsEnabled(features::kQuietNotificationPrompts));
  ASSERT_TRUE(base::FeatureList::IsEnabled(
      permissions::features::kPermissionQuietChip));
  ASSERT_TRUE(base::FeatureList::IsEnabled(
      permissions::features::kPermissionChipGestureSensitive));
  ASSERT_TRUE(base::FeatureList::IsEnabled(
      permissions::features::kPermissionChipRequestTypeSensitive));

  SetCannedUiDecision(QuietUiReason::kTriggeredDueToAbusiveContent,
                      absl::nullopt);

  RequestPermission(permissions::RequestType::kGeolocation);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE);

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  RequestPermission(permissions::RequestType::kNotifications);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_QUIET_ABUSIVE_CHIP);

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  RequestPermission(permissions::RequestType::kMidiSysex);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE);
}

class QuietUIPromoInteractiveTest : public PermissionChipInteractiveTest {
 public:
  QuietUIPromoInteractiveTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kQuietNotificationPrompts,
        {{QuietNotificationPermissionUiConfig::kEnableAdaptiveActivation,
          "true"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(QuietUIPromoInteractiveTest, QuietUIPromo) {
  auto* profile = browser()->profile();
  // Promo is not enabled by default.
  EXPECT_FALSE(QuietNotificationPermissionUiState::ShouldShowPromo(profile));

  for (const char* origin_spec :
       {"https://a.com", "https://b.com", "https://c.com"}) {
    GURL requesting_origin(origin_spec);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), requesting_origin));
    permissions::MockPermissionRequest notification_request(
        requesting_origin, permissions::RequestType::kNotifications);
    test_api_->manager()->AddRequest(GetActiveMainFrame(),
                                     &notification_request);
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(test_api_->manager()->ShouldCurrentRequestUseQuietUI());
    EXPECT_FALSE(QuietNotificationPermissionUiState::ShouldShowPromo(profile));
    test_api_->manager()->Deny();
    base::RunLoop().RunUntilIdle();
  }

  ContentSettingImageView& quiet_ui_icon = GetContentSettingImageView(
      ContentSettingImageModel::ImageType::NOTIFICATIONS_QUIET_PROMPT);

  EXPECT_FALSE(quiet_ui_icon.GetVisible());
  // `ContentSettingImageView::AnimationEnded()` was not triggered and IPH is
  // not shown.
  EXPECT_FALSE(quiet_ui_icon.get_critical_promo_id_for_testing().has_value());

  GURL notification("http://www.notification1.com/");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), notification));
  permissions::MockPermissionRequest notification_request(
      notification, permissions::RequestType::kNotifications);
  test_api_->manager()->AddRequest(GetActiveMainFrame(), &notification_request);
  base::RunLoop().RunUntilIdle();

  // After 3 denied Notifications requests, Adaptive activation enabled quiet
  // permission prompt.
  EXPECT_TRUE(test_api_->manager()->ShouldCurrentRequestUseQuietUI());
  // At the first quiet permission prompt we show IPH.
  ASSERT_TRUE(QuietNotificationPermissionUiState::ShouldShowPromo(profile));

  EXPECT_TRUE(quiet_ui_icon.GetVisible());
  EXPECT_TRUE(quiet_ui_icon.is_animating_label());
  // Animation is reset to trigger `ContentSettingImageView::AnimationEnded()`.
  // `AnimationEnded` contains logic for displaying IPH and marking it as shown.
  quiet_ui_icon.reset_animation_for_testing();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(quiet_ui_icon.is_animating_label());

  // The IPH is showing.
  ASSERT_TRUE(quiet_ui_icon.get_critical_promo_id_for_testing().has_value());
  FeaturePromoControllerViews* iph_controller =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->feature_promo_controller();
  // The critical promo that is currently showing is the one created by a quiet
  // permission prompt.
  EXPECT_TRUE(iph_controller->CriticalPromoIsShowing(
      quiet_ui_icon.get_critical_promo_id_for_testing().value()));

  iph_controller->CloseBubbleForCriticalPromo(
      quiet_ui_icon.get_critical_promo_id_for_testing().value());

  test_api_->manager()->Deny();
  base::RunLoop().RunUntilIdle();

  // After quiet permission prompt was resolved, the critical promo is reset.
  EXPECT_FALSE(quiet_ui_icon.get_critical_promo_id_for_testing().has_value());

  EXPECT_FALSE(quiet_ui_icon.GetVisible());

  // The second Notifications permission request to verify that the IPH is not
  // shown.
  GURL notification2("http://www.notification2.com/");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), notification2));
  permissions::MockPermissionRequest notification_request2(
      notification2, permissions::RequestType::kNotifications);
  test_api_->manager()->AddRequest(GetActiveMainFrame(),
                                   &notification_request2);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_api_->manager()->ShouldCurrentRequestUseQuietUI());
  // At the second quiet permission prompt the IPH should be disabled.
  EXPECT_FALSE(QuietNotificationPermissionUiState::ShouldShowPromo(profile));

  EXPECT_TRUE(quiet_ui_icon.GetVisible());
  EXPECT_TRUE(quiet_ui_icon.is_animating_label());
  quiet_ui_icon.reset_animation_for_testing();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(quiet_ui_icon.is_animating_label());

  // The IPH id is not empty because `ContentSettingImageView::AnimationEnded()`
  // was triggered.
  EXPECT_TRUE(quiet_ui_icon.get_critical_promo_id_for_testing().has_value());
  // The critical promo is not shown.
  EXPECT_FALSE(iph_controller->CriticalPromoIsShowing(
      quiet_ui_icon.get_critical_promo_id_for_testing().value()));

  test_api_->manager()->Deny();
  base::RunLoop().RunUntilIdle();
}

class QuietChipPermissionPromptBubbleViewInteractiveTest
    : public PermissionChipInteractiveTest {
 public:
  QuietChipPermissionPromptBubbleViewInteractiveTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kQuietNotificationPrompts,
         permissions::features::kPermissionQuietChip},
        {});
  }

 protected:
  using QuietUiReason = permissions::PermissionUiSelector::QuietUiReason;
  using WarningReason = permissions::PermissionUiSelector::WarningReason;

  void SetCannedUiDecision(absl::optional<QuietUiReason> quiet_ui_reason,
                           absl::optional<WarningReason> warning_reason) {
    test_api_->manager()->set_permission_ui_selector_for_testing(
        std::make_unique<TestQuietNotificationPermissionUiSelector>(
            permissions::PermissionUiSelector::Decision(quiet_ui_reason,
                                                        warning_reason)));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(QuietChipPermissionPromptBubbleViewInteractiveTest,
                       LoudChipIsShownForNonAbusiveRequests) {
  SetCannedUiDecision(absl::nullopt, absl::nullopt);

  RequestPermission(permissions::RequestType::kGeolocation);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::LOCATION_BAR_LEFT_CHIP);

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  RequestPermission(permissions::RequestType::kNotifications);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::LOCATION_BAR_LEFT_CHIP);
}

IN_PROC_BROWSER_TEST_F(QuietChipPermissionPromptBubbleViewInteractiveTest,
                       QuietChipIsShownForAbusiveRequests) {
  for (QuietUiReason reason : {QuietUiReason::kTriggeredByCrowdDeny,
                               QuietUiReason::kTriggeredDueToAbusiveRequests,
                               QuietUiReason::kTriggeredDueToAbusiveContent}) {
    SetCannedUiDecision(reason, absl::nullopt);

    RequestPermission(permissions::RequestType::kGeolocation);

    EXPECT_EQ(
        test_api_->manager()->current_request_prompt_disposition_for_testing(),
        permissions::PermissionPromptDisposition::LOCATION_BAR_LEFT_CHIP);

    test_api_->manager()->Accept();
    base::RunLoop().RunUntilIdle();

    RequestPermission(permissions::RequestType::kNotifications);

    // Quiet Chip is enabled, that means a quiet chip will be shown even if the
    // Chip experiment is disabled.
    EXPECT_EQ(
        test_api_->manager()->current_request_prompt_disposition_for_testing(),
        permissions::PermissionPromptDisposition::
            LOCATION_BAR_LEFT_QUIET_ABUSIVE_CHIP);
  }
}

// The quiet UI icon is verified to make sure that the quiet chip is not shown
// when the quiet icon is shown.
IN_PROC_BROWSER_TEST_F(QuietChipPermissionPromptBubbleViewInteractiveTest,
                       QuietChipIsNotShownForNonAbusiveRequests) {
  SetCannedUiDecision(absl::nullopt, absl::nullopt);

  ContentSettingImageView& quiet_ui_icon = GetContentSettingImageView(
      ContentSettingImageModel::ImageType::NOTIFICATIONS_QUIET_PROMPT);
  EXPECT_FALSE(quiet_ui_icon.GetVisible());
  EXPECT_FALSE(GetChip());

  RequestPermission(permissions::RequestType::kGeolocation);

  EXPECT_FALSE(quiet_ui_icon.GetVisible());
  ExpectNormalChip();

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  RequestPermission(permissions::RequestType::kNotifications);

  EXPECT_FALSE(quiet_ui_icon.GetVisible());
  ExpectNormalChip();

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(QuietChipPermissionPromptBubbleViewInteractiveTest,
                       NotAnimatedQuietChipIsShownForAbusiveRequests) {
  for (QuietUiReason reason : {QuietUiReason::kTriggeredByCrowdDeny,
                               QuietUiReason::kTriggeredDueToAbusiveRequests,
                               QuietUiReason::kTriggeredDueToAbusiveContent}) {
    SetCannedUiDecision(reason, absl::nullopt);

    ContentSettingImageView& quiet_ui_icon = GetContentSettingImageView(
        ContentSettingImageModel::ImageType::NOTIFICATIONS_QUIET_PROMPT);
    EXPECT_FALSE(quiet_ui_icon.GetVisible());
    EXPECT_FALSE(GetChip());

    RequestPermission(permissions::RequestType::kGeolocation);

    EXPECT_FALSE(quiet_ui_icon.GetVisible());
    ExpectNormalChip();

    test_api_->manager()->Accept();
    base::RunLoop().RunUntilIdle();

    RequestPermission(permissions::RequestType::kNotifications);

    EXPECT_FALSE(quiet_ui_icon.GetVisible());
    ExpectQuietAbusiveChip();

    test_api_->manager()->Accept();
    base::RunLoop().RunUntilIdle();
  }
}

IN_PROC_BROWSER_TEST_F(QuietChipPermissionPromptBubbleViewInteractiveTest,
                       AnimatedQuietChipIsShownForNonAbusiveRequests) {
  for (QuietUiReason reason : {QuietUiReason::kEnabledInPrefs,
                               QuietUiReason::kPredictedVeryUnlikelyGrant}) {
    SetCannedUiDecision(reason, absl::nullopt);

    ContentSettingImageView& quiet_ui_icon = GetContentSettingImageView(
        ContentSettingImageModel::ImageType::NOTIFICATIONS_QUIET_PROMPT);
    EXPECT_FALSE(quiet_ui_icon.GetVisible());
    EXPECT_FALSE(GetChip());

    RequestPermission(permissions::RequestType::kGeolocation);

    EXPECT_FALSE(quiet_ui_icon.GetVisible());
    ExpectNormalChip();

    test_api_->manager()->Accept();
    base::RunLoop().RunUntilIdle();

    RequestPermission(permissions::RequestType::kNotifications);

    EXPECT_FALSE(quiet_ui_icon.GetVisible());
    ExpectQuietChip();

    test_api_->manager()->Accept();
    base::RunLoop().RunUntilIdle();
  }
}

// Test that the quiet prompt disposition differs when permission is considered
// abusive (currently only applicable for Notifications) vs. when permission is
// not considered abusive.
IN_PROC_BROWSER_TEST_F(QuietChipPermissionPromptBubbleViewInteractiveTest,
                       DispositionAbusiveContentTest) {
  SetCannedUiDecision(QuietUiReason::kTriggeredDueToAbusiveContent,
                      WarningReason::kAbusiveContent);

  RequestPermission(permissions::RequestType::kGeolocation);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::LOCATION_BAR_LEFT_CHIP);

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  RequestPermission(permissions::RequestType::kNotifications);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_QUIET_ABUSIVE_CHIP);
}

IN_PROC_BROWSER_TEST_F(QuietChipPermissionPromptBubbleViewInteractiveTest,
                       DispositionCrowdDenyTest) {
  SetCannedUiDecision(QuietUiReason::kTriggeredByCrowdDeny, absl::nullopt);

  RequestPermission(permissions::RequestType::kGeolocation);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::LOCATION_BAR_LEFT_CHIP);

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  RequestPermission(permissions::RequestType::kNotifications);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_QUIET_ABUSIVE_CHIP);
}

IN_PROC_BROWSER_TEST_F(QuietChipPermissionPromptBubbleViewInteractiveTest,
                       DispositionEnabledInPrefsTest) {
  SetCannedUiDecision(QuietUiReason::kEnabledInPrefs, absl::nullopt);

  RequestPermission(permissions::RequestType::kGeolocation);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::LOCATION_BAR_LEFT_CHIP);

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  RequestPermission(permissions::RequestType::kNotifications);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::LOCATION_BAR_LEFT_QUIET_CHIP);
}

IN_PROC_BROWSER_TEST_F(QuietChipPermissionPromptBubbleViewInteractiveTest,
                       DispositionPredictedVeryUnlikelyGrantTest) {
  SetCannedUiDecision(QuietUiReason::kPredictedVeryUnlikelyGrant,
                      absl::nullopt);

  RequestPermission(permissions::RequestType::kGeolocation);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::LOCATION_BAR_LEFT_CHIP);

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  RequestPermission(permissions::RequestType::kNotifications);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::LOCATION_BAR_LEFT_QUIET_CHIP);
}

IN_PROC_BROWSER_TEST_F(QuietChipPermissionPromptBubbleViewInteractiveTest,
                       DispositionAbusiveRequestsTest) {
  SetCannedUiDecision(QuietUiReason::kTriggeredDueToAbusiveRequests,
                      WarningReason::kAbusiveRequests);

  RequestPermission(permissions::RequestType::kGeolocation);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::LOCATION_BAR_LEFT_CHIP);

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  RequestPermission(permissions::RequestType::kNotifications);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_QUIET_ABUSIVE_CHIP);
}
