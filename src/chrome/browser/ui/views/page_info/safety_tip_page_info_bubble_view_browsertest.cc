// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/engagement/site_engagement_score.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_test_utils.h"
#include "chrome/browser/lookalikes/safety_tips/reputation_web_contents_observer.h"
#include "chrome/browser/lookalikes/safety_tips/safety_tip_test_utils.h"
#include "chrome/browser/lookalikes/safety_tips/safety_tip_ui_helper.h"
#include "chrome/browser/lookalikes/safety_tips/safety_tips.pb.h"
#include "chrome/browser/lookalikes/safety_tips/safety_tips_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view_base.h"
#include "chrome/browser/ui/views/page_info/safety_tip_page_info_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/safe_browsing/db/v4_protocol_manager_util.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace {

enum class UIStatus {
  kDisabled,
  kEnabled,
  kEnabledWithEditDistance,
};

// An engagement score above MEDIUM.
const int kHighEngagement = 20;

// An engagement score below MEDIUM.
const int kLowEngagement = 1;

class ClickEvent : public ui::Event {
 public:
  ClickEvent() : ui::Event(ui::ET_UNKNOWN, base::TimeTicks(), 0) {}
};

// Simulates a link click navigation. We don't use
// ui_test_utils::NavigateToURL(const GURL&) because it simulates the user
// typing the URL, causing the site to have a site engagement score of at
// least LOW.
//
// This function waits for the load to complete since it is based on the
// synchronous ui_test_utils::NavigatToURL.
void NavigateToURL(Browser* browser,
                   const GURL& url,
                   WindowOpenDisposition disposition) {
  NavigateParams params(browser, url, ui::PAGE_TRANSITION_LINK);
  params.initiator_origin = url::Origin::Create(GURL("about:blank"));
  params.disposition = disposition;
  params.is_renderer_initiated = true;

  ui_test_utils::NavigateToURL(&params);
}

void PerformMouseClickOnView(views::View* view) {
  ui::AXActionData data;
  data.action = ax::mojom::Action::kDoDefault;
  view->HandleAccessibleAction(data);
}

bool IsUIShowing() {
  return PageInfoBubbleViewBase::BUBBLE_SAFETY_TIP ==
         PageInfoBubbleViewBase::GetShownBubbleType();
}

void CloseWarningIgnore() {
  if (!PageInfoBubbleViewBase::GetPageInfoBubbleForTesting()) {
    return;
  }
  auto* widget =
      PageInfoBubbleViewBase::GetPageInfoBubbleForTesting()->GetWidget();
  views::test::WidgetDestroyedWaiter waiter(widget);
  widget->CloseWithReason(views::Widget::ClosedReason::kCloseButtonClicked);
  waiter.Wait();
}

// Sets the absolute Site Engagement |score| for the testing origin.
void SetEngagementScore(Browser* browser, const GURL& url, double score) {
  SiteEngagementService::Get(browser->profile())
      ->ResetBaseScoreForURL(url, score);
}

// Clicks the location icon to open the page info bubble.
void OpenPageInfoBubble(Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  LocationIconView* location_icon_view =
      browser_view->toolbar()->location_bar()->location_icon_view();
  ASSERT_TRUE(location_icon_view);
  ClickEvent event;
  location_icon_view->ShowBubble(event);
  views::BubbleDialogDelegateView* page_info =
      PageInfoBubbleViewBase::GetPageInfoBubbleForTesting();
  EXPECT_NE(nullptr, page_info);
  page_info->set_close_on_deactivate(false);
}

// Go to |url| in such a way as to trigger a bad reputation safety tip. This is
// just for convenience, since how we trigger warnings will change. Even if the
// warning is triggered, it may not be shown if the URL is opened in the
// background.
//
// This function blocks the entire host + path, ignoring query parameters.
void TriggerWarning(Browser* browser,
                    const GURL& url,
                    WindowOpenDisposition disposition) {
  std::string host;
  std::string path;
  std::string query;
  safe_browsing::V4ProtocolManagerUtil::CanonicalizeUrl(url, &host, &path,
                                                        &query);
  // For simplicity, ignore query
  SetSafetyTipBadRepPatterns({host + path});
  SetEngagementScore(browser, url, kLowEngagement);
  NavigateToURL(browser, url, disposition);
}

}  // namespace

class SafetyTipPageInfoBubbleViewBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<UIStatus> {
 protected:
  UIStatus ui_status() const { return GetParam(); }

  void SetUp() override {
    switch (ui_status()) {
      case UIStatus::kDisabled:
        feature_list_.InitAndDisableFeature(features::kSafetyTipUI);
        break;
      case UIStatus::kEnabled:
        feature_list_.InitWithFeaturesAndParameters(
            {{features::kSafetyTipUI, {}},
             {features::kLookalikeUrlNavigationSuggestionsUI,
              {{"topsites", "true"}}}},
            {});
        break;
      case UIStatus::kEnabledWithEditDistance:
        feature_list_.InitWithFeaturesAndParameters(
            {{features::kSafetyTipUI,
              {{"editdistance", "true"},
               {"editdistance_siteengagement", "true"}}},
             {features::kLookalikeUrlNavigationSuggestionsUI,
              {{"topsites", "true"}}}},
            {});
    }

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  GURL GetURL(const char* hostname) const {
    return embedded_test_server()->GetURL(hostname, "/title1.html");
  }

  GURL GetURLWithoutPath(const char* hostname) const {
    return GetURL(hostname).GetWithEmptyPath();
  }

  void ClickLeaveButton() {
    // This class is a friend to SafetyTipPageInfoBubbleView.
    auto* bubble = static_cast<SafetyTipPageInfoBubbleView*>(
        PageInfoBubbleViewBase::GetPageInfoBubbleForTesting());
    PerformMouseClickOnView(bubble->GetLeaveButtonForTesting());
  }

  void CloseWarningLeaveSite(Browser* browser) {
    if (ui_status() == UIStatus::kDisabled) {
      return;
    }
    content::TestNavigationObserver navigation_observer(
        browser->tab_strip_model()->GetActiveWebContents(), 1);
    ClickLeaveButton();
    navigation_observer.Wait();
  }

  bool IsUIShowingIfEnabled() {
    return ui_status() == UIStatus::kDisabled ? true : IsUIShowing();
  }

  void CheckPageInfoShowsSafetyTipInfo(Browser* browser) {
    if (ui_status() == UIStatus::kDisabled) {
      return;
    }

    OpenPageInfoBubble(browser);
    views::BubbleDialogDelegateView* page_info =
        PageInfoBubbleViewBase::GetPageInfoBubbleForTesting();
    ASSERT_TRUE(page_info);
    EXPECT_EQ(page_info->GetWindowTitle(),
              l10n_util::GetStringUTF16(IDS_PAGE_INFO_SAFETY_TIP_SUMMARY));
  }

  void CheckPageInfoDoesNotShowSafetyTipInfo(Browser* browser) {
    OpenPageInfoBubble(browser);
    views::BubbleDialogDelegateView* page_info =
        PageInfoBubbleViewBase::GetPageInfoBubbleForTesting();
    ASSERT_TRUE(page_info);
    EXPECT_NE(page_info->GetWindowTitle(),
              l10n_util::GetStringUTF16(IDS_PAGE_INFO_SAFETY_TIP_SUMMARY));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         SafetyTipPageInfoBubbleViewBrowserTest,
                         ::testing::Values(UIStatus::kDisabled,
                                           UIStatus::kEnabled,
                                           UIStatus::kEnabledWithEditDistance));

// Ensure normal sites with low engagement are not blocked.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       NoShowOnLowEngagement) {
  auto kNavigatedUrl = GetURL("site1.com");
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(IsUIShowing());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoDoesNotShowSafetyTipInfo(browser()));
}

// Ensure blocked sites with high engagement are not blocked.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       NoShowOnHighEngagement) {
  auto kNavigatedUrl = GetURL("site1.com");
  SetSafetyTipBadRepPatterns({"site1.com/"});

  SetEngagementScore(browser(), kNavigatedUrl, kHighEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(IsUIShowing());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoDoesNotShowSafetyTipInfo(browser()));
}

// Ensure blocked sites get blocked.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest, ShowOnBlock) {
  auto kNavigatedUrl = GetURL("site1.com");
  SetSafetyTipBadRepPatterns({"site1.com/"});

  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(IsUIShowingIfEnabled());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoShowsSafetyTipInfo(browser()));
}

// After the user clicks 'leave site', the user should end up on a safe domain.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       LeaveSiteLeavesSite) {
  auto kNavigatedUrl = GetURL("site1.com");
  if (ui_status() == UIStatus::kDisabled) {
    return;
  }

  TriggerWarning(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);

  CloseWarningLeaveSite(browser());
  EXPECT_FALSE(IsUIShowing());
  EXPECT_NE(kNavigatedUrl,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoDoesNotShowSafetyTipInfo(browser()));
}

// If the user clicks 'leave site', the warning should re-appear when the user
// re-visits the page.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       LeaveSiteStillWarnsAfter) {
  auto kNavigatedUrl = GetURL("site1.com");
  TriggerWarning(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);

  CloseWarningLeaveSite(browser());

  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);

  EXPECT_TRUE(IsUIShowingIfEnabled());
  EXPECT_EQ(kNavigatedUrl,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoShowsSafetyTipInfo(browser()));
}

// After the user closes the warning, they should still be on the same domain.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       IgnoreWarningStaysOnPage) {
  if (ui_status() == UIStatus::kDisabled) {
    return;
  }
  auto kNavigatedUrl = GetURL("site1.com");
  TriggerWarning(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);

  CloseWarningIgnore();
  EXPECT_FALSE(IsUIShowing());
  EXPECT_EQ(kNavigatedUrl,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoShowsSafetyTipInfo(browser()));
}

// If the user closes the bubble, the warning should not re-appear when the user
// re-visits the page.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       IgnoreWarningStopsWarning) {
  if (ui_status() == UIStatus::kDisabled) {
    return;
  }
  auto kNavigatedUrl = GetURL("site1.com");
  TriggerWarning(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);

  CloseWarningIgnore();

  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);

  EXPECT_FALSE(IsUIShowing());
  EXPECT_EQ(kNavigatedUrl,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoDoesNotShowSafetyTipInfo(browser()));
}

// Non main-frame navigations should be ignored.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       IgnoreIFrameNavigations) {
  const GURL kNavigatedUrl =
      embedded_test_server()->GetURL("a.com", "/iframe_cross_site.html");
  const GURL kFrameUrl =
      embedded_test_server()->GetURL("b.com", "/title1.html");
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  SetSafetyTipBadRepPatterns({"a.com/"});

  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(IsUIShowingIfEnabled());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoShowsSafetyTipInfo(browser()));
}

// Background tabs shouldn't open a bubble initially, but should when they
// become visible.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       BubbleWaitsForVisible) {
  auto kFlaggedUrl = GetURL("site1.com");

  TriggerWarning(browser(), kFlaggedUrl,
                 WindowOpenDisposition::NEW_BACKGROUND_TAB);
  EXPECT_FALSE(IsUIShowing());

  auto* tab_strip = browser()->tab_strip_model();
  tab_strip->ActivateTabAt(tab_strip->active_index() + 1);

  EXPECT_TRUE(IsUIShowingIfEnabled());
}

// Tests that Safety Tips do NOT trigger on lookalike domains that trigger an
// interstitial.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       SkipLookalikeInterstitialed) {
  const GURL kNavigatedUrl = GetURL("googlé.com");
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(IsUIShowing());
}

// Tests that Safety Tips trigger on lookalike domains that don't qualify for an
// interstitial.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       TriggersOnLookalike) {
  // This domain is a top domain, but not top 500.
  const GURL kNavigatedUrl = GetURL("googlé.sk");
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(IsUIShowingIfEnabled());
}

// Tests that Safety Tips trigger (or not) on lookalike domains with edit
// distance when enabled, and not otherwise.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       TriggersOnEditDistance) {
  // This domain is an edit distance of one from the top 500.
  const GURL kNavigatedUrl = GetURL("goooglé.com");
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_EQ(IsUIShowing(), ui_status() == UIStatus::kEnabledWithEditDistance);
}

// Tests that the SafetyTipShown histogram triggers correctly.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       SafetyTipShownHistogram) {
  const char kHistogramName[] = "Security.SafetyTips.SafetyTipShown";
  base::HistogramTester histograms;

  auto kNavigatedUrl = GetURL("site1.com");
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  histograms.ExpectBucketCount(kHistogramName,
                               security_state::SafetyTipStatus::kNone, 1);

  auto kBadRepUrl = GetURL("site2.com");
  TriggerWarning(browser(), kBadRepUrl, WindowOpenDisposition::CURRENT_TAB);
  CloseWarningLeaveSite(browser());
  histograms.ExpectBucketCount(
      kHistogramName, security_state::SafetyTipStatus::kBadReputation, 1);

  const GURL kLookalikeUrl = GetURL("googlé.sk");
  SetEngagementScore(browser(), kLookalikeUrl, kLowEngagement);
  NavigateToURL(browser(), kLookalikeUrl, WindowOpenDisposition::CURRENT_TAB);
  histograms.ExpectBucketCount(kHistogramName,
                               security_state::SafetyTipStatus::kLookalike, 1);
  histograms.ExpectTotalCount(kHistogramName, 3);
}

// Tests that the SafetyTipIgnoredPageLoad histogram triggers correctly.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       SafetyTipIgnoredPageLoadHistogram) {
  if (ui_status() == UIStatus::kDisabled) {
    return;
  }
  base::HistogramTester histograms;
  auto kNavigatedUrl = GetURL("site1.com");
  TriggerWarning(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  CloseWarningIgnore();
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  histograms.ExpectBucketCount("Security.SafetyTips.SafetyTipIgnoredPageLoad",
                               security_state::SafetyTipStatus::kBadReputation,
                               1);
}

// Tests that Safety Tip interactions are recorded in a histogram.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       InteractionsHistogram) {
  const std::string kHistogramPrefix = "Security.SafetyTips.Interaction.";

  // These histograms are only recorded when the UI feature is enabled, so bail
  // out when disabled.
  if (ui_status() != UIStatus::kEnabledWithEditDistance) {
    return;
  }

  {
    // This domain is an edit distance of one from the top 500.
    const GURL kNavigatedUrl = GetURL("goooglé.com");
    SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
    base::HistogramTester histogram_tester;
    NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
    // The histogram should not be recorded until the user has interacted with
    // the safety tip.
    histogram_tester.ExpectTotalCount(kHistogramPrefix + "SafetyTip_Lookalike",
                                      0);
    CloseWarningLeaveSite(browser());
    histogram_tester.ExpectUniqueSample(
        kHistogramPrefix + "SafetyTip_Lookalike",
        safety_tips::SafetyTipInteraction::kLeaveSite, 1);
  }

  {
    base::HistogramTester histogram_tester;
    auto kNavigatedUrl = GetURL("site1.com");
    TriggerWarning(browser(), kNavigatedUrl,
                   WindowOpenDisposition::CURRENT_TAB);
    // The histogram should not be recorded until the user has interacted with
    // the safety tip.
    histogram_tester.ExpectTotalCount(
        kHistogramPrefix + "SafetyTip_BadReputation", 0);
    CloseWarningIgnore();
    histogram_tester.ExpectUniqueSample(
        kHistogramPrefix + "SafetyTip_BadReputation",
        safety_tips::SafetyTipInteraction::kDismiss, 1);
  }
}

// Tests that the histograms recording how long the Safety Tip is open are
// recorded properly.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       TimeOpenHistogram) {
  if (ui_status() == UIStatus::kDisabled) {
    return;
  }
  const base::TimeDelta kMinWarningTime = base::TimeDelta::FromMilliseconds(10);

  // Test the histogram for no user action taken.
  {
    base::HistogramTester histograms;
    auto kNavigatedUrl = GetURL("site1.com");
    TriggerWarning(browser(), kNavigatedUrl,
                   WindowOpenDisposition::CURRENT_TAB);
    // Ensure that the tab is open for more than 0 ms, even in the face of bots
    // with bad clocks.
    base::RunLoop run_loop;
    base::PostDelayedTask(FROM_HERE, run_loop.QuitClosure(), kMinWarningTime);
    run_loop.Run();
    NavigateToURL(browser(), GURL("about:blank"),
                  WindowOpenDisposition::CURRENT_TAB);
    auto samples = histograms.GetAllSamples(
        "Security.SafetyTips.OpenTime.NoAction.SafetyTip_BadReputation");
    ASSERT_EQ(1u, samples.size());
    EXPECT_LE(kMinWarningTime.InMilliseconds(), samples.front().min);
  }

  // Test the histogram for when the user adheres to the warning.
  {
    base::HistogramTester histograms;
    auto kNavigatedUrl = GetURL("site1.com");
    TriggerWarning(browser(), kNavigatedUrl,
                   WindowOpenDisposition::CURRENT_TAB);
    base::RunLoop run_loop;
    base::PostDelayedTask(FROM_HERE, run_loop.QuitClosure(), kMinWarningTime);
    run_loop.Run();
    CloseWarningLeaveSite(browser());
    auto samples = histograms.GetAllSamples(
        "Security.SafetyTips.OpenTime.LeaveSite.SafetyTip_BadReputation");
    ASSERT_EQ(1u, samples.size());
    EXPECT_LE(kMinWarningTime.InMilliseconds(), samples.front().min);
  }

  // Test the histogram for when the user dismisses the warning.
  {
    base::HistogramTester histograms;
    auto kNavigatedUrl = GetURL("site1.com");
    TriggerWarning(browser(), kNavigatedUrl,
                   WindowOpenDisposition::CURRENT_TAB);
    base::RunLoop run_loop;
    base::PostDelayedTask(FROM_HERE, run_loop.QuitClosure(), kMinWarningTime);
    run_loop.Run();
    CloseWarningIgnore();
    auto samples = histograms.GetAllSamples(
        "Security.SafetyTips.OpenTime.Dismiss.SafetyTip_BadReputation");
    ASSERT_EQ(1u, samples.size());
    EXPECT_LE(kMinWarningTime.InMilliseconds(), samples.front().min);
  }
}

// Tests that Safety Tips aren't triggered on 'unknown' flag types from the
// component updater. This permits us to add new flag types to the component
// without breaking this release.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       NotShownOnUnknownFlag) {
  auto kNavigatedUrl = GetURL("site1.com");
  SetSafetyTipPatternsWithFlagType(
      {"site1.com/"}, chrome_browser_safety_tips::FlaggedPage::UNKNOWN);

  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(IsUIShowing());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoDoesNotShowSafetyTipInfo(browser()));
}

// Tests that Safety Tips aren't triggered on domains flagged as 'YOUNG_DOMAIN'
// in the component. This permits us to use this flag in the component without
// breaking this release.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       NotShownOnYoungDomain) {
  auto kNavigatedUrl = GetURL("site1.com");
  SetSafetyTipPatternsWithFlagType(
      {"site1.com/"}, chrome_browser_safety_tips::FlaggedPage::YOUNG_DOMAIN);

  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(IsUIShowing());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoDoesNotShowSafetyTipInfo(browser()));
}
