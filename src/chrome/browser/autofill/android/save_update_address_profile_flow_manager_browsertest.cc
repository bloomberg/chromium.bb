// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/save_update_address_profile_flow_manager.h"

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class SaveUpdateAddressProfileFlowManagerBrowserTest
    : public AndroidBrowserTest {
 public:
  SaveUpdateAddressProfileFlowManagerBrowserTest() = default;
  ~SaveUpdateAddressProfileFlowManagerBrowserTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        features::kAutofillAddressProfileSavePrompt);
    AndroidBrowserTest::SetUp();
    profile_ = test::GetFullProfile();
    original_profile_ = test::GetFullProfile2();
  }

  // AndroidBrowserTest:
  void SetUpOnMainThread() override {
    flow_manager_ = std::make_unique<SaveUpdateAddressProfileFlowManager>();
  }

  void TearDownOnMainThread() override {
    // Destroy `flow_manager_` before WebContents are destroyed.
    flow_manager_.reset();
  }

  content::WebContents* GetWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  bool IsMessageDisplayed() {
    return flow_manager_->GetMessageControllerForTest()->IsMessageDisplayed();
  }

  bool IsPromptDisplayed() {
    return !!flow_manager_->GetPromptControllerForTest();
  }

  AutofillProfile profile_;
  AutofillProfile original_profile_;
  std::unique_ptr<SaveUpdateAddressProfileFlowManager> flow_manager_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SaveUpdateAddressProfileFlowManagerBrowserTest,
                       TriggerAutoDeclineDecisionIfMessageIsDisplayed) {
  flow_manager_->OfferSave(GetWebContents(), profile_, &original_profile_,
                           /*callback=*/base::DoNothing());
  EXPECT_TRUE(IsMessageDisplayed());
  EXPECT_FALSE(IsPromptDisplayed());

  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback>
      another_save_callback;
  AutofillProfile another_profile = test::GetFullProfile2();
  EXPECT_CALL(
      another_save_callback,
      Run(AutofillClient::SaveAddressProfileOfferUserDecision::kAutoDeclined,
          another_profile));
  flow_manager_->OfferSave(GetWebContents(), another_profile,
                           /*original_profile=*/nullptr,
                           another_save_callback.Get());
}

IN_PROC_BROWSER_TEST_F(SaveUpdateAddressProfileFlowManagerBrowserTest,
                       TriggerAutoDeclineDecisionIfPromptIsDisplayed) {
  flow_manager_->OfferSave(GetWebContents(), profile_, &original_profile_,
                           /*callback=*/base::DoNothing());
  // Proceed with message to prompt.
  flow_manager_->GetMessageControllerForTest()->OnPrimaryAction();
  flow_manager_->GetMessageControllerForTest()->DismissMessageForTest(
      messages::DismissReason::PRIMARY_ACTION);
  EXPECT_FALSE(IsMessageDisplayed());
  EXPECT_TRUE(IsPromptDisplayed());

  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback>
      another_save_callback;
  AutofillProfile another_profile = test::GetFullProfile2();
  EXPECT_CALL(
      another_save_callback,
      Run(AutofillClient::SaveAddressProfileOfferUserDecision::kAutoDeclined,
          another_profile));
  flow_manager_->OfferSave(GetWebContents(), another_profile,
                           /*original_profile=*/nullptr,
                           another_save_callback.Get());
}

}  // namespace autofill
