// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_banner/save_address_profile/save_address_profile_infobar_banner_overlay_mediator.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/guid.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_save_address_profile_delegate_ios.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#include "ios/chrome/browser/overlays/public/infobar_banner/infobar_banner_overlay_responses.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/save_address_profile_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/ui/infobars/banners/test/fake_infobar_banner_consumer.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using save_address_profile_infobar_overlays::
    SaveAddressProfileBannerRequestConfig;

// Test fixture for SaveAddressProfileInfobarBannerOverlayMediator.
using SaveAddressProfileInfobarBannerOverlayMediatorTest = PlatformTest;

// Tests that a SaveAddressProfileInfobarBannerOverlayMediator correctly sets up
// its consumer.
TEST_F(SaveAddressProfileInfobarBannerOverlayMediatorTest, SetUpConsumer) {
  autofill::AutofillProfile profile(base::GenerateGUID(),
                                    "https://www.example.com/");
  std::unique_ptr<autofill::AutofillSaveAddressProfileDelegateIOS>
      passed_delegate =
          std::make_unique<autofill::AutofillSaveAddressProfileDelegateIOS>(
              profile,
              base::BindOnce(^(
                  autofill::AutofillClient::SaveAddressProfileOfferUserDecision
                      user_decision,
                  autofill::AutofillProfile profile){
              }));
  autofill::AutofillSaveAddressProfileDelegateIOS* delegate =
      passed_delegate.get();
  InfoBarIOS infobar(InfobarType::kInfobarTypeSaveAutofillAddressProfile,
                     std::move(passed_delegate));

  // Package the infobar into an OverlayRequest, then create a mediator that
  // uses this request in order to set up a fake consumer.
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<SaveAddressProfileBannerRequestConfig>(
          &infobar);
  SaveAddressProfileInfobarBannerOverlayMediator* mediator =
      [[SaveAddressProfileInfobarBannerOverlayMediator alloc]
          initWithRequest:request.get()];
  FakeInfobarBannerConsumer* consumer =
      [[FakeInfobarBannerConsumer alloc] init];
  mediator.consumer = consumer;
  // Verify that the infobar was set up properly.
  EXPECT_NSEQ(base::SysUTF16ToNSString(delegate->GetMessageText()),
              consumer.titleText);
  EXPECT_NSEQ(base::SysUTF16ToNSString(delegate->GetMessageActionText()),
              consumer.buttonText);
  EXPECT_NSEQ(base::SysUTF16ToNSString(delegate->GetMessageDescriptionText()),
              consumer.subtitleText);
}
