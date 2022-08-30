// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/card_unmask_otp_input_dialog_controller_impl.h"

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/payments/otp_unmask_result.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

class TestCardUnmaskOtpInputDialogView : public CardUnmaskOtpInputDialogView {
 public:
  TestCardUnmaskOtpInputDialogView() = default;
  ~TestCardUnmaskOtpInputDialogView() override = default;
  void ShowPendingState() override {}
  void ShowInvalidState(const std::u16string& invalid_label_text) override {}
  void Dismiss(bool show_confirmation_before_closing,
               bool user_closed_dialog) override {}
};

class TestCardUnmaskOtpInputDialogControllerImpl
    : public CardUnmaskOtpInputDialogControllerImpl {
 public:
  static void CreateForTesting(content::WebContents* web_contents) {
    web_contents->SetUserData(
        UserDataKey(),
        std::make_unique<TestCardUnmaskOtpInputDialogControllerImpl>(
            web_contents));
  }

  explicit TestCardUnmaskOtpInputDialogControllerImpl(
      content::WebContents* web_contents)
      : CardUnmaskOtpInputDialogControllerImpl(web_contents) {}

  void ShowDialog(TestCardUnmaskOtpInputDialogView* dialog_view) {
    dialog_view_ = dialog_view;
  }
};

}  // namespace

class CardUnmaskOtpInputDialogControllerImplTest
    : public ChromeRenderViewHostTestHarness {
 public:
  CardUnmaskOtpInputDialogControllerImplTest() = default;
  CardUnmaskOtpInputDialogControllerImplTest(
      const CardUnmaskOtpInputDialogControllerImplTest&) = delete;
  CardUnmaskOtpInputDialogControllerImplTest& operator=(
      const CardUnmaskOtpInputDialogControllerImplTest&) = delete;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    TestCardUnmaskOtpInputDialogControllerImpl::CreateForTesting(
        web_contents());
  }

  TestCardUnmaskOtpInputDialogControllerImpl* controller() {
    return static_cast<TestCardUnmaskOtpInputDialogControllerImpl*>(
        TestCardUnmaskOtpInputDialogControllerImpl::FromWebContents(
            web_contents()));
  }

  TestCardUnmaskOtpInputDialogView* test_dialog() { return test_dialog_.get(); }

 private:
  std::unique_ptr<TestCardUnmaskOtpInputDialogView> test_dialog_ =
      std::make_unique<TestCardUnmaskOtpInputDialogView>();
};

TEST_F(CardUnmaskOtpInputDialogControllerImplTest,
       DialogCancelledByUserBeforeConfirmation_NoTemporaryError) {
  base::HistogramTester histogram_tester;

  DCHECK(controller());
  controller()->OnDialogClosed(/*user_closed_dialog=*/true,
                               /*server_request_succeeded=*/false);

  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpInputDialog.SmsOtp.Result",
      AutofillMetrics::OtpInputDialogResult::
          kDialogCancelledByUserBeforeConfirmation,
      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpInputDialog.SmsOtp.Result.WithNoTemporaryError",
      AutofillMetrics::OtpInputDialogResult::
          kDialogCancelledByUserBeforeConfirmation,
      1);
}

TEST_F(CardUnmaskOtpInputDialogControllerImplTest,
       DialogCancelledByUserBeforeConfirmation_OtpMistmatch) {
  base::HistogramTester histogram_tester;

  DCHECK(controller());
  controller()->ShowDialog(test_dialog());
  controller()->OnOtpVerificationResult(OtpUnmaskResult::kOtpMismatch);
  controller()->OnDialogClosed(/*user_closed_dialog=*/true,
                               /*server_request_succeeded=*/false);

  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpInputDialog.SmsOtp.ErrorMessageShown",
      AutofillMetrics::OtpInputDialogError::kOtpMismatchError, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpInputDialog.SmsOtp.Result",
      AutofillMetrics::OtpInputDialogResult::
          kDialogCancelledByUserBeforeConfirmation,
      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpInputDialog.SmsOtp.Result.WithPreviousTemporaryError",
      AutofillMetrics::OtpInputDialogResult::
          kDialogCancelledByUserBeforeConfirmation,
      1);
}

TEST_F(CardUnmaskOtpInputDialogControllerImplTest,
       DialogCancelledByUserAfterConfirmation_OtpExpired) {
  base::HistogramTester histogram_tester;

  DCHECK(controller());
  controller()->ShowDialog(test_dialog());
  controller()->OnOkButtonClicked(/*otp=*/u"123456");
  controller()->OnOtpVerificationResult(OtpUnmaskResult::kOtpExpired);
  controller()->OnDialogClosed(/*user_closed_dialog=*/true,
                               /*server_request_succeeded=*/false);

  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpInputDialog.SmsOtp.ErrorMessageShown",
      AutofillMetrics::OtpInputDialogError::kOtpExpiredError, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpInputDialog.SmsOtp.Result",
      AutofillMetrics::OtpInputDialogResult::
          kDialogCancelledByUserAfterConfirmation,
      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpInputDialog.SmsOtp.Result.WithPreviousTemporaryError",
      AutofillMetrics::OtpInputDialogResult::
          kDialogCancelledByUserAfterConfirmation,
      1);
}

TEST_F(CardUnmaskOtpInputDialogControllerImplTest, ServerRequestSucceeded) {
  base::HistogramTester histogram_tester;

  DCHECK(controller());
  controller()->OnDialogClosed(/*user_closed_dialog=*/false,
                               /*server_request_succeeded=*/true);

  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpInputDialog.SmsOtp.Result",
      AutofillMetrics::OtpInputDialogResult::
          kDialogClosedAfterVerificationSucceeded,
      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpInputDialog.SmsOtp.Result.WithNoTemporaryError",
      AutofillMetrics::OtpInputDialogResult::
          kDialogClosedAfterVerificationSucceeded,
      1);
}

TEST_F(CardUnmaskOtpInputDialogControllerImplTest, ServerRequestFailed) {
  base::HistogramTester histogram_tester;

  DCHECK(controller());
  controller()->OnDialogClosed(/*user_closed_dialog=*/false,
                               /*server_request_succeeded=*/false);

  histogram_tester.ExpectUniqueSample("Autofill.OtpInputDialog.SmsOtp.Result",
                                      AutofillMetrics::OtpInputDialogResult::
                                          kDialogClosedAfterVerificationFailed,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpInputDialog.SmsOtp.Result.WithNoTemporaryError",
      AutofillMetrics::OtpInputDialogResult::
          kDialogClosedAfterVerificationFailed,
      1);
}

TEST_F(CardUnmaskOtpInputDialogControllerImplTest, NewCodeLinkClicked) {
  base::HistogramTester histogram_tester;

  DCHECK(controller());
  controller()->OnNewCodeLinkClicked();

  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpInputDialog.SmsOtp.NewOtpRequested", true, 1);
}

}  // namespace autofill
