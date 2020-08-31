// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/local_card_migration_bubble_controller_impl.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/ui/autofill/payments/local_card_migration_bubble.h"
#include "chrome/browser/ui/autofill/test/test_autofill_bubble_handler.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "content/public/test/mock_navigation_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Bucket;
using testing::ElementsAre;

namespace autofill {

namespace {

class TestLocalCardMigrationBubbleControllerImpl
    : public LocalCardMigrationBubbleControllerImpl {
 public:
  static void CreateForTesting(content::WebContents* web_contents) {
    web_contents->SetUserData(
        UserDataKey(),
        std::make_unique<TestLocalCardMigrationBubbleControllerImpl>(
            web_contents));
  }

  explicit TestLocalCardMigrationBubbleControllerImpl(
      content::WebContents* web_contents)
      : LocalCardMigrationBubbleControllerImpl(web_contents) {}

  void set_elapsed(base::TimeDelta elapsed) { elapsed_ = elapsed; }

  void SimulateNavigation() {
    content::RenderFrameHost* rfh = web_contents()->GetMainFrame();
    content::MockNavigationHandle navigation_handle(GURL(), rfh);
    navigation_handle.set_has_committed(true);
    DidFinishNavigation(&navigation_handle);
  }

 protected:
  base::TimeDelta Elapsed() const override { return elapsed_; }

 private:
  base::TimeDelta elapsed_;
};

}  // namespace

class LocalCardMigrationBubbleControllerImplTest
    : public BrowserWithTestWindowTest {
 public:
  LocalCardMigrationBubbleControllerImplTest() {}

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    AddTab(browser(), GURL("about:blank"));
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    TestLocalCardMigrationBubbleControllerImpl::CreateForTesting(web_contents);
  }

 protected:
  void ShowBubble() {
    controller()->ShowBubble(base::BindOnce(&LocalCardMigrationCallback));
  }

  void CloseBubble(PaymentsBubbleClosedReason closed_reason =
                       PaymentsBubbleClosedReason::kNotInteracted) {
    controller()->OnBubbleClosed(closed_reason);
  }

  void CloseAndReshowBubble() {
    CloseBubble();
    controller()->ReshowBubble();
  }

  TestLocalCardMigrationBubbleControllerImpl* controller() {
    return static_cast<TestLocalCardMigrationBubbleControllerImpl*>(
        TestLocalCardMigrationBubbleControllerImpl::FromWebContents(
            browser()->tab_strip_model()->GetActiveWebContents()));
  }

 private:
  static void LocalCardMigrationCallback() {}

  DISALLOW_COPY_AND_ASSIGN(LocalCardMigrationBubbleControllerImplTest);
};

TEST_F(LocalCardMigrationBubbleControllerImplTest,
       Metrics_FirstShow_ShowBubble) {
  base::HistogramTester histogram_tester;
  ShowBubble();

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.LocalCardMigrationBubbleOffer.FirstShow"),
      ElementsAre(
          Bucket(AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_REQUESTED, 1),
          Bucket(AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_SHOWN, 1)));
}

TEST_F(LocalCardMigrationBubbleControllerImplTest, Metrics_Reshows_ShowBubble) {
  base::HistogramTester histogram_tester;
  ShowBubble();
  CloseAndReshowBubble();

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.LocalCardMigrationBubbleOffer.Reshows"),
      ElementsAre(
          Bucket(AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_REQUESTED, 1),
          Bucket(AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_SHOWN, 1)));
}

TEST_F(LocalCardMigrationBubbleControllerImplTest,
       Metrics_FirstShow_SaveButton) {
  ShowBubble();

  base::HistogramTester histogram_tester;
  controller()->OnConfirmButtonClicked();
  CloseBubble();

  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationBubbleUserInteraction.FirstShow",
      AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_CLOSED_ACCEPTED, 1);
}

TEST_F(LocalCardMigrationBubbleControllerImplTest, Metrics_Reshows_SaveButton) {
  ShowBubble();
  CloseAndReshowBubble();

  base::HistogramTester histogram_tester;
  controller()->OnConfirmButtonClicked();
  CloseBubble();

  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationBubbleUserInteraction.Reshows",
      AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_CLOSED_ACCEPTED, 1);
}

TEST_F(LocalCardMigrationBubbleControllerImplTest,
       Metrics_FirstShow_CancelButton) {
  ShowBubble();

  base::HistogramTester histogram_tester;
  controller()->OnCancelButtonClicked();
  CloseBubble();

  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationBubbleUserInteraction.FirstShow",
      AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_CLOSED_DENIED, 1);
}

TEST_F(LocalCardMigrationBubbleControllerImplTest,
       Metrics_Reshows_CancelButton) {
  ShowBubble();
  CloseAndReshowBubble();

  base::HistogramTester histogram_tester;
  controller()->OnCancelButtonClicked();
  CloseBubble();

  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationBubbleUserInteraction.Reshows",
      AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_CLOSED_DENIED, 1);
}

TEST_F(LocalCardMigrationBubbleControllerImplTest,
       Metrics_FirstShow_NavigateWhileShowing) {
  ShowBubble();

  base::HistogramTester histogram_tester;
  // The bubble should still stick around for up to kSurviveNavigationSeconds
  // (5) seconds regardless of navigation.
  controller()->set_elapsed(base::TimeDelta::FromSeconds(3));
  controller()->SimulateNavigation();

  histogram_tester.ExpectTotalCount(
      "Autofill.LocalCardMigrationBubbleUserInteraction.FirstShow", 0);

  // Wait 3 more seconds (6 total); bubble should go away on next navigation.
  controller()->set_elapsed(base::TimeDelta::FromSeconds(6));
  controller()->SimulateNavigation();

  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationBubbleUserInteraction.FirstShow",
      AutofillMetrics::
          LOCAL_CARD_MIGRATION_BUBBLE_CLOSED_NAVIGATED_WHILE_SHOWING,
      1);
}

TEST_F(LocalCardMigrationBubbleControllerImplTest,
       Metrics_Reshows_NavigateWhileShowing) {
  ShowBubble();
  CloseAndReshowBubble();

  base::HistogramTester histogram_tester;
  // The bubble should still stick around for up to kSurviveNavigationSeconds
  // (5) seconds regardless of navigation.
  controller()->set_elapsed(base::TimeDelta::FromSeconds(3));
  controller()->SimulateNavigation();

  histogram_tester.ExpectTotalCount(
      "Autofill.LocalCardMigrationBubbleUserInteraction.Reshows", 0);

  // Wait 3 more seconds (6 total); bubble should go away on next navigation.
  controller()->set_elapsed(base::TimeDelta::FromSeconds(6));
  controller()->SimulateNavigation();

  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationBubbleUserInteraction.Reshows",
      AutofillMetrics::
          LOCAL_CARD_MIGRATION_BUBBLE_CLOSED_NAVIGATED_WHILE_SHOWING,
      1);
}

TEST_F(LocalCardMigrationBubbleControllerImplTest,
       Metrics_FirstShow_NavigateWhileHidden) {
  ShowBubble();

  base::HistogramTester histogram_tester;
  CloseBubble();
  // Fake-navigate after bubble has been visible for a long time.
  controller()->set_elapsed(base::TimeDelta::FromMinutes(1));
  controller()->SimulateNavigation();

  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationBubbleUserInteraction.FirstShow",
      AutofillMetrics::
          LOCAL_CARD_MIGRATION_BUBBLE_CLOSED_NAVIGATED_WHILE_HIDDEN,
      1);
}

TEST_F(LocalCardMigrationBubbleControllerImplTest,
       Metrics_Reshows_NavigateWhileHidden) {
  ShowBubble();
  CloseAndReshowBubble();

  base::HistogramTester histogram_tester;
  CloseBubble();
  // Fake-navigate after bubble has been visible for a long time.
  controller()->set_elapsed(base::TimeDelta::FromMinutes(1));
  controller()->SimulateNavigation();

  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationBubbleUserInteraction.Reshows",
      AutofillMetrics::
          LOCAL_CARD_MIGRATION_BUBBLE_CLOSED_NAVIGATED_WHILE_HIDDEN,
      1);
}

TEST_F(LocalCardMigrationBubbleControllerImplTest,
       OnlyOneActiveBubble_Repeated) {
  base::HistogramTester histogram_tester;
  ShowBubble();
  ShowBubble();
  ShowBubble();
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.LocalCardMigrationBubbleOffer.FirstShow"),
      ElementsAre(
          Bucket(AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_REQUESTED, 1),
          Bucket(AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_SHOWN, 1)));
}

// Ensures the bubble should still stick around even if the time since bubble
// showing is longer than kCardBubbleSurviveNavigationTime (5 seconds) when the
// feature is enabled.
TEST_F(LocalCardMigrationBubbleControllerImplTest,
       StickyBubble_ShouldNotDismissUponNavigation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillEnableStickyPaymentsBubble);

  ShowBubble();
  base::HistogramTester histogram_tester;
  controller()->set_elapsed(base::TimeDelta::FromSeconds(10));
  controller()->SimulateNavigation();

  histogram_tester.ExpectTotalCount(
      "Autofill.LocalCardMigrationBubbleOffer.FirstShow", 0);
  EXPECT_NE(nullptr, controller()->local_card_migration_bubble_view());
}

// Test class to ensure the local card migration bubble result is logged
// correctly. The boolean Param of this class decides whether the new logging
// experiment has been enabled.
class LocalCardMigrationBubbleLoggingTest
    : public LocalCardMigrationBubbleControllerImplTest,
      public ::testing::WithParamInterface<bool> {
 public:
  LocalCardMigrationBubbleLoggingTest() = default;
  ~LocalCardMigrationBubbleLoggingTest() override = default;

  void SetUp() override {
    LocalCardMigrationBubbleControllerImplTest::SetUp();
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kAutofillEnableFixedPaymentsBubbleLogging);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kAutofillEnableFixedPaymentsBubbleLogging);
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(LocalCardMigrationBubbleLoggingTest, FirstShow_BubbleAccepted) {
  base::HistogramTester histogram_tester;
  ShowBubble();
  CloseBubble(PaymentsBubbleClosedReason::kAccepted);

  if (GetParam()) {
    histogram_tester.ExpectUniqueSample(
        "Autofill.LocalCardMigrationBubbleResult.FirstShow",
        AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_ACCEPTED, 1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Autofill.LocalCardMigrationBubbleResult.FirstShow", 0);
  }
}

TEST_P(LocalCardMigrationBubbleLoggingTest, FirstShow_BubbleClosed) {
  base::HistogramTester histogram_tester;
  ShowBubble();
  CloseBubble(PaymentsBubbleClosedReason::kClosed);

  if (GetParam()) {
    histogram_tester.ExpectUniqueSample(
        "Autofill.LocalCardMigrationBubbleResult.FirstShow",
        AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_CLOSED, 1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Autofill.LocalCardMigrationBubbleResult.FirstShow", 0);
  }
}

TEST_P(LocalCardMigrationBubbleLoggingTest, FirstShow_BubbleNotInteracted) {
  base::HistogramTester histogram_tester;
  ShowBubble();
  CloseBubble(PaymentsBubbleClosedReason::kNotInteracted);

  if (GetParam()) {
    histogram_tester.ExpectUniqueSample(
        "Autofill.LocalCardMigrationBubbleResult.FirstShow",
        AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_NOT_INTERACTED, 1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Autofill.LocalCardMigrationBubbleResult.FirstShow", 0);
  }
}

TEST_P(LocalCardMigrationBubbleLoggingTest, FirstShow_BubbleLostFocus) {
  base::HistogramTester histogram_tester;
  ShowBubble();
  CloseBubble(PaymentsBubbleClosedReason::kLostFocus);

  if (GetParam()) {
    histogram_tester.ExpectUniqueSample(
        "Autofill.LocalCardMigrationBubbleResult.FirstShow",
        AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_LOST_FOCUS, 1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Autofill.LocalCardMigrationBubbleResult.FirstShow", 0);
  }
}

TEST_P(LocalCardMigrationBubbleLoggingTest, Reshows_BubbleAccepted) {
  base::HistogramTester histogram_tester;
  ShowBubble();
  CloseAndReshowBubble();
  CloseBubble(PaymentsBubbleClosedReason::kAccepted);

  if (GetParam()) {
    histogram_tester.ExpectUniqueSample(
        "Autofill.LocalCardMigrationBubbleResult.FirstShow",
        AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_NOT_INTERACTED, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.LocalCardMigrationBubbleResult.Reshows",
        AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_ACCEPTED, 1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Autofill.LocalCardMigrationBubbleResult.FirstShow", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.LocalCardMigrationBubbleResult.Reshows", 0);
  }
}

TEST_P(LocalCardMigrationBubbleLoggingTest, Reshows_BubbleClosed) {
  base::HistogramTester histogram_tester;
  ShowBubble();
  CloseAndReshowBubble();
  CloseBubble(PaymentsBubbleClosedReason::kClosed);

  if (GetParam()) {
    histogram_tester.ExpectUniqueSample(
        "Autofill.LocalCardMigrationBubbleResult.FirstShow",
        AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_NOT_INTERACTED, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.LocalCardMigrationBubbleResult.Reshows",
        AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_CLOSED, 1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Autofill.LocalCardMigrationBubbleResult.FirstShow", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.LocalCardMigrationBubbleResult.Reshows", 0);
  }
}

TEST_P(LocalCardMigrationBubbleLoggingTest, Reshows_BubbleNotInteracted) {
  base::HistogramTester histogram_tester;
  ShowBubble();
  CloseAndReshowBubble();
  CloseBubble(PaymentsBubbleClosedReason::kNotInteracted);

  if (GetParam()) {
    histogram_tester.ExpectUniqueSample(
        "Autofill.LocalCardMigrationBubbleResult.FirstShow",
        AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_NOT_INTERACTED, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.LocalCardMigrationBubbleResult.Reshows",
        AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_NOT_INTERACTED, 1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Autofill.LocalCardMigrationBubbleResult.FirstShow", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.LocalCardMigrationBubbleResult.Reshows", 0);
  }
}

TEST_P(LocalCardMigrationBubbleLoggingTest, Reshows_BubbleLostFocus) {
  base::HistogramTester histogram_tester;
  ShowBubble();
  CloseAndReshowBubble();
  CloseBubble(PaymentsBubbleClosedReason::kLostFocus);

  if (GetParam()) {
    histogram_tester.ExpectUniqueSample(
        "Autofill.LocalCardMigrationBubbleResult.FirstShow",
        AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_NOT_INTERACTED, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.LocalCardMigrationBubbleResult.Reshows",
        AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_LOST_FOCUS, 1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Autofill.LocalCardMigrationBubbleResult.FirstShow", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.LocalCardMigrationBubbleResult.Reshows", 0);
  }
}

INSTANTIATE_TEST_SUITE_P(LocalCardMigrationBubbleControllerImplTest,
                         LocalCardMigrationBubbleLoggingTest,
                         ::testing::Bool());

}  // namespace autofill
