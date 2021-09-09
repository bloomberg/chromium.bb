// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_modal/autofill_address_profile/save_address_profile_infobar_modal_overlay_mediator.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/guid.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_save_update_address_profile_delegate_ios.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/save_address_profile_infobar_modal_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/save_address_profile_infobar_modal_overlay_responses.h"
#include "ios/chrome/browser/overlays/test/fake_overlay_request_callback_installer.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_edit_address_profile_modal_consumer.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_save_address_profile_modal_consumer.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using autofill_address_profile_infobar_overlays::
    SaveAddressProfileModalRequestConfig;
using save_address_profile_infobar_modal_responses::EditedProfileSaveAction;
using save_address_profile_infobar_modal_responses::CancelViewAction;

// Test fixture for SaveAddressProfileInfobarModalOverlayMediator.
class SaveAddressProfileInfobarModalOverlayMediatorTest : public PlatformTest {
 public:
  SaveAddressProfileInfobarModalOverlayMediatorTest()
      : callback_installer_(&callback_receiver_,
                            {EditedProfileSaveAction::ResponseSupport(),
                             CancelViewAction::ResponseSupport()}),
        mediator_delegate_(
            OCMStrictProtocolMock(@protocol(OverlayRequestMediatorDelegate))) {
    autofill::AutofillProfile profile = autofill::test::GetFullProfile();
    std::unique_ptr<autofill::AutofillSaveUpdateAddressProfileDelegateIOS>
        delegate = std::make_unique<
            autofill::AutofillSaveUpdateAddressProfileDelegateIOS>(
            profile, /*original_profile=*/nullptr, /*locale=*/"en-US",
            base::DoNothing());
    delegate_ = delegate.get();
    infobar_ = std::make_unique<InfoBarIOS>(
        InfobarType::kInfobarTypeSaveAutofillAddressProfile,
        std::move(delegate));

    request_ =
        OverlayRequest::CreateWithConfig<SaveAddressProfileModalRequestConfig>(
            infobar_.get());
    callback_installer_.InstallCallbacks(request_.get());

    mediator_ = [[SaveAddressProfileInfobarModalOverlayMediator alloc]
        initWithRequest:request_.get()];
    mediator_.delegate = mediator_delegate_;
  }

  ~SaveAddressProfileInfobarModalOverlayMediatorTest() override {
    EXPECT_CALL(callback_receiver_, CompletionCallback(request_.get()));
    EXPECT_OCMOCK_VERIFY(mediator_delegate_);
  }

 protected:
  autofill::AutofillSaveUpdateAddressProfileDelegateIOS* delegate_;
  std::unique_ptr<InfoBarIOS> infobar_;
  MockOverlayRequestCallbackReceiver callback_receiver_;
  FakeOverlayRequestCallbackInstaller callback_installer_;
  std::unique_ptr<OverlayRequest> request_;
  SaveAddressProfileInfobarModalOverlayMediator* mediator_ = nil;
  id<OverlayRequestMediatorDelegate> mediator_delegate_ = nil;
};

// Tests that calling saveEditedProfileWithData: triggers a
// EditedProfileSaveAction response.
TEST_F(SaveAddressProfileInfobarModalOverlayMediatorTest, EditAction) {
  EXPECT_CALL(callback_receiver_,
              DispatchCallback(request_.get(),
                               EditedProfileSaveAction::ResponseSupport()));
  OCMExpect([mediator_delegate_ stopOverlayForMediator:mediator_]);
  [mediator_ saveEditedProfileWithData:@{}.mutableCopy];
}

// Tests that calling dismissInfobarModal triggers a CancelViewAction response.
TEST_F(SaveAddressProfileInfobarModalOverlayMediatorTest, CancelAction) {
  EXPECT_CALL(
      callback_receiver_,
      DispatchCallback(request_.get(), CancelViewAction::ResponseSupport()));
  OCMExpect([mediator_delegate_ stopOverlayForMediator:mediator_]);
  [mediator_ dismissInfobarModal:nil];
}
