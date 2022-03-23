// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "build/build_config.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_enroll_bubble_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/autofill/payments/virtual_card_enroll_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/virtual_card_enroll_icon_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/metrics/payments/virtual_card_enrollment_metrics.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/test_legal_message_line.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_observer.h"

namespace autofill {

namespace {

constexpr int kCardImageWidthInPx = 32;
constexpr int kCardImageLengthInPx = 20;

}  // namespace

class VirtualCardEnrollBubbleViewsInteractiveUiTest
    : public InProcessBrowserTest {
 public:
  VirtualCardEnrollBubbleViewsInteractiveUiTest() = default;
  ~VirtualCardEnrollBubbleViewsInteractiveUiTest() override = default;
  VirtualCardEnrollBubbleViewsInteractiveUiTest(
      const VirtualCardEnrollBubbleViewsInteractiveUiTest&) = delete;
  VirtualCardEnrollBubbleViewsInteractiveUiTest& operator=(
      const VirtualCardEnrollBubbleViewsInteractiveUiTest&) = delete;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    VirtualCardEnrollBubbleControllerImpl* controller =
        static_cast<VirtualCardEnrollBubbleControllerImpl*>(
            VirtualCardEnrollBubbleControllerImpl::GetOrCreate(
                browser()->tab_strip_model()->GetActiveWebContents()));
    DCHECK(controller);
    CreateVirtualCardEnrollmentFields();
  }

  void CreateVirtualCardEnrollmentFields() {
    CreditCard credit_card = test::GetFullServerCard();
    LegalMessageLines google_legal_message = {
        TestLegalMessageLine("google_test_legal_message")};
    LegalMessageLines issuer_legal_message = {
        TestLegalMessageLine("issuer_test_legal_message")};

    upstream_virtual_card_enrollment_fields_.credit_card = credit_card;
    upstream_virtual_card_enrollment_fields_.card_art_image = &card_art_image_;
    upstream_virtual_card_enrollment_fields_.google_legal_message =
        google_legal_message;
    upstream_virtual_card_enrollment_fields_.issuer_legal_message =
        issuer_legal_message;
    upstream_virtual_card_enrollment_fields_.virtual_card_enrollment_source =
        VirtualCardEnrollmentSource::kUpstream;

    downstream_virtual_card_enrollment_fields_ =
        upstream_virtual_card_enrollment_fields_;
    downstream_virtual_card_enrollment_fields_.virtual_card_enrollment_source =
        VirtualCardEnrollmentSource::kDownstream;

    settings_page_virtual_card_enrollment_fields_ =
        upstream_virtual_card_enrollment_fields_;
    settings_page_virtual_card_enrollment_fields_
        .virtual_card_enrollment_source =
        VirtualCardEnrollmentSource::kSettingsPage;
  }

  void ShowBubbleAndWaitUntilShown(
      const VirtualCardEnrollmentFields& virtual_card_enrollment_fields,
      base::OnceClosure accept_virtual_card_callback,
      base::OnceClosure decline_virtual_card_callback) {
    base::RunLoop run_loop;
    base::RepeatingClosure bubble_shown_closure_for_testing_ =
        run_loop.QuitClosure();
    GetController()->SetBubbleShownClosureForTesting(
        bubble_shown_closure_for_testing_);

    GetController()->ShowBubble(
        virtual_card_enrollment_fields,
        /*accept_virtual_card_callback*/ base::DoNothing(),
        /*decline_virtual_card_callback*/ base::DoNothing());
  }

  void ReshowBubble() { GetController()->ReshowBubble(); }

  bool IsIconVisible() { return GetIconView() && GetIconView()->GetVisible(); }

  VirtualCardEnrollBubbleControllerImpl* GetController() {
    if (!browser() || !browser()->tab_strip_model() ||
        !browser()->tab_strip_model()->GetActiveWebContents()) {
      return nullptr;
    }

    return VirtualCardEnrollBubbleControllerImpl::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  VirtualCardEnrollBubbleViews* GetBubbleViews() {
    VirtualCardEnrollBubbleControllerImpl* controller = GetController();
    if (!controller)
      return nullptr;

    return static_cast<VirtualCardEnrollBubbleViews*>(
        controller->GetVirtualCardEnrollBubbleView());
  }

  VirtualCardEnrollIconView* GetIconView() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    PageActionIconView* icon =
        browser_view->toolbar_button_provider()->GetPageActionIconView(
            PageActionIconType::kVirtualCardEnroll);
    DCHECK(icon);
    return static_cast<VirtualCardEnrollIconView*>(icon);
  }

  const VirtualCardEnrollmentFields&
  downstream_virtual_card_enrollment_fields() {
    return downstream_virtual_card_enrollment_fields_;
  }

  const VirtualCardEnrollmentFields& upstream_virtual_card_enrollment_fields() {
    return upstream_virtual_card_enrollment_fields_;
  }

  const VirtualCardEnrollmentFields&
  settings_page_virtual_card_enrollment_fields() {
    return settings_page_virtual_card_enrollment_fields_;
  }

  const VirtualCardEnrollmentFields& GetFieldsForSource(
      VirtualCardEnrollmentSource source) {
    switch (source) {
      case VirtualCardEnrollmentSource::kUpstream:
        return upstream_virtual_card_enrollment_fields();
      case VirtualCardEnrollmentSource::kDownstream:
        return downstream_virtual_card_enrollment_fields();
      default:
        return settings_page_virtual_card_enrollment_fields();
    }
  }

 private:
  VirtualCardEnrollmentFields downstream_virtual_card_enrollment_fields_;
  VirtualCardEnrollmentFields upstream_virtual_card_enrollment_fields_;
  VirtualCardEnrollmentFields settings_page_virtual_card_enrollment_fields_;
  gfx::Image card_art_image_ =
      gfx::test::CreateImage(kCardImageWidthInPx, kCardImageLengthInPx);
};

// Invokes a bubble showing to test if it is showing and the icon is visible.
IN_PROC_BROWSER_TEST_F(VirtualCardEnrollBubbleViewsInteractiveUiTest,
                       ShowBubble) {
  ShowBubbleAndWaitUntilShown(upstream_virtual_card_enrollment_fields(),
                               base::DoNothing(), base::DoNothing());
  EXPECT_TRUE(GetBubbleViews());
  EXPECT_TRUE(IsIconVisible());

  ShowBubbleAndWaitUntilShown(downstream_virtual_card_enrollment_fields(),
                               base::DoNothing(), base::DoNothing());
  EXPECT_TRUE(GetBubbleViews());
  EXPECT_TRUE(IsIconVisible());

  ShowBubbleAndWaitUntilShown(settings_page_virtual_card_enrollment_fields(),
                               base::DoNothing(), base::DoNothing());
  EXPECT_TRUE(GetBubbleViews());
  EXPECT_TRUE(IsIconVisible());
}

IN_PROC_BROWSER_TEST_F(VirtualCardEnrollBubbleViewsInteractiveUiTest,
                       Metrics_BubbleLostFocus) {
  base::HistogramTester histogram_tester;
  std::string histogram_name;

  for (VirtualCardEnrollmentSource virtual_card_enrollment_source :
       {VirtualCardEnrollmentSource::kUpstream,
        VirtualCardEnrollmentSource::kDownstream,
        VirtualCardEnrollmentSource::kSettingsPage}) {
    ShowBubbleAndWaitUntilShown(
        GetFieldsForSource(virtual_card_enrollment_source), base::DoNothing(),
        base::DoNothing());

    ASSERT_TRUE(GetBubbleViews());
    ASSERT_TRUE(IsIconVisible());

    // Mock deactivation and lose focus.
    views::test::WidgetDestroyedWaiter destroyed_waiter(
        GetBubbleViews()->GetWidget());
    GetBubbleViews()->GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kLostFocus);
    destroyed_waiter.Wait();

    histogram_tester.ExpectBucketCount(
        "Autofill.VirtualCardEnrollBubble.Result." +
            VirtualCardEnrollmentSourceToMetricSuffix(
                virtual_card_enrollment_source) +
            ".FirstShow",
        VirtualCardEnrollmentBubbleResult::
            VIRTUAL_CARD_ENROLLMENT_BUBBLE_LOST_FOCUS,
        1);
  }
}

IN_PROC_BROWSER_TEST_F(VirtualCardEnrollBubbleViewsInteractiveUiTest,
                       Metrics_BubbleAccepted) {
  base::HistogramTester histogram_tester;
  std::string histogram_name;

  for (VirtualCardEnrollmentSource virtual_card_enrollment_source :
       {VirtualCardEnrollmentSource::kUpstream,
        VirtualCardEnrollmentSource::kDownstream,
        VirtualCardEnrollmentSource::kSettingsPage}) {
    ShowBubbleAndWaitUntilShown(
        GetFieldsForSource(virtual_card_enrollment_source), base::DoNothing(),
        base::DoNothing());

    ASSERT_TRUE(GetBubbleViews());
    ASSERT_TRUE(IsIconVisible());

    // Mock deactivation and accept.
    views::test::WidgetDestroyedWaiter destroyed_waiter(
        GetBubbleViews()->GetWidget());
    GetBubbleViews()->GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kAcceptButtonClicked);
    destroyed_waiter.Wait();

    histogram_tester.ExpectBucketCount(
        "Autofill.VirtualCardEnrollBubble.Result." +
            VirtualCardEnrollmentSourceToMetricSuffix(
                virtual_card_enrollment_source) +
            ".FirstShow",
        VirtualCardEnrollmentBubbleResult::
            VIRTUAL_CARD_ENROLLMENT_BUBBLE_ACCEPTED,
        1);
  }
}

class NotInteractedAndClosedMetricsTest
    : public VirtualCardEnrollBubbleViewsInteractiveUiTest,
      public testing::WithParamInterface<VirtualCardEnrollmentSource> {
 public:
  NotInteractedAndClosedMetricsTest() = default;
  ~NotInteractedAndClosedMetricsTest() override = default;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    NotInteractedAndClosedMetricsTest,
    testing::Values(VirtualCardEnrollmentSource::kUpstream,
                    VirtualCardEnrollmentSource::kDownstream,
                    VirtualCardEnrollmentSource::kSettingsPage));

IN_PROC_BROWSER_TEST_P(NotInteractedAndClosedMetricsTest,
                       NotInteractedTest_AllSources) {
  VirtualCardEnrollmentSource virtual_card_enrollment_source = GetParam();
  base::HistogramTester histogram_tester;
  std::string histogram_name;
  ShowBubbleAndWaitUntilShown(
      GetFieldsForSource(virtual_card_enrollment_source), base::DoNothing(),
      base::DoNothing());

  ASSERT_TRUE(GetBubbleViews());
  ASSERT_TRUE(IsIconVisible());

  // Mock browser being closed.
  views::test::WidgetDestroyedWaiter destroyed_waiter(
      GetBubbleViews()->GetWidget());
  browser()->tab_strip_model()->CloseAllTabs();
  destroyed_waiter.Wait();

  histogram_tester.ExpectBucketCount(
      "Autofill.VirtualCardEnrollBubble.Result." +
          VirtualCardEnrollmentSourceToMetricSuffix(
              virtual_card_enrollment_source) +
          ".FirstShow",
      VirtualCardEnrollmentBubbleResult::
          VIRTUAL_CARD_ENROLLMENT_BUBBLE_NOT_INTERACTED,
      1);
}

IN_PROC_BROWSER_TEST_P(NotInteractedAndClosedMetricsTest,
                       ShownAndLostFocusTest_AllSources) {
  base::HistogramTester histogram_tester;
  VirtualCardEnrollmentSource virtual_card_enrollment_source = GetParam();
  ShowBubbleAndWaitUntilShown(
      GetFieldsForSource(virtual_card_enrollment_source), base::DoNothing(),
      base::DoNothing());

  EXPECT_TRUE(GetBubbleViews());
  EXPECT_TRUE(IsIconVisible());

  histogram_tester.ExpectBucketCount(
      "Autofill.VirtualCardEnrollBubble.Shown." +
          VirtualCardEnrollmentSourceToMetricSuffix(
              virtual_card_enrollment_source),
      false, 1);

  // Mock deactivation due to clicking the close button.
  views::test::WidgetDestroyedWaiter destroyed_waiter1(
      GetBubbleViews()->GetWidget());
  GetBubbleViews()->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
  destroyed_waiter1.Wait();

  // Confirm .FirstShow metrics.
  histogram_tester.ExpectBucketCount(
      "Autofill.VirtualCardEnrollBubble.Result." +
          VirtualCardEnrollmentSourceToMetricSuffix(
              virtual_card_enrollment_source) +
          ".FirstShow",
      VirtualCardEnrollmentBubbleResult::VIRTUAL_CARD_ENROLLMENT_BUBBLE_CLOSED,
      1);

  // Bubble is reshown by the user.
  ReshowBubble();

  histogram_tester.ExpectBucketCount(
      "Autofill.VirtualCardEnrollBubble.Shown." +
          VirtualCardEnrollmentSourceToMetricSuffix(
              virtual_card_enrollment_source),
      true, 1);

  // Mock deactivation due to clicking the close button.
  views::test::WidgetDestroyedWaiter destroyed_waiter2(
      GetBubbleViews()->GetWidget());
  GetBubbleViews()->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
  destroyed_waiter2.Wait();

  // Confirm .Reshows metrics.
  histogram_tester.ExpectUniqueSample(
      "Autofill.VirtualCardEnrollBubble.Result." +
          VirtualCardEnrollmentSourceToMetricSuffix(
              virtual_card_enrollment_source) +
          ".Reshows",
      VirtualCardEnrollmentBubbleResult::VIRTUAL_CARD_ENROLLMENT_BUBBLE_CLOSED,
      1);

  // Bubble is reshown by the user. Closing a reshown bubble makes the
  // browser inactive for some reason, so we must reactivate it first.
  browser()->window()->Activate();
  ReshowBubble();

  histogram_tester.ExpectBucketCount(
      "Autofill.VirtualCardEnrollBubble.Shown." +
          VirtualCardEnrollmentSourceToMetricSuffix(
              virtual_card_enrollment_source),
      true, 2);
}

}  // namespace autofill
