// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/autofill_address_profile/save_address_profile_infobar_modal_interaction_handler.h"

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_save_update_address_profile_delegate_ios.h"
#include "components/autofill/core/browser/field_types.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/autofill_address_profile/save_address_profile_infobar_modal_overlay_request_callback_installer.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

SaveAddressProfileInfobarModalInteractionHandler::
    SaveAddressProfileInfobarModalInteractionHandler() = default;

SaveAddressProfileInfobarModalInteractionHandler::
    ~SaveAddressProfileInfobarModalInteractionHandler() = default;

#pragma mark - Public

void SaveAddressProfileInfobarModalInteractionHandler::PerformMainAction(
    InfoBarIOS* infobar) {
  infobar->set_accepted(GetInfoBarDelegate(infobar)->Accept());
  // Post save, the infobar should not be brought back from the omnibox
  // icon.
  infobar->RemoveSelf();
}

void SaveAddressProfileInfobarModalInteractionHandler::SaveEditedProfile(
    InfoBarIOS* infobar,
    NSDictionary* profileData) {
  for (NSNumber* key in profileData) {
    autofill::ServerFieldType type =
        AutofillTypeFromAutofillUIType((AutofillUIType)[key intValue]);
    std::u16string data = base::SysNSStringToUTF16(profileData[key]);
    GetInfoBarDelegate(infobar)->SetProfileInfo(type, data);
  }
  infobar->set_accepted(GetInfoBarDelegate(infobar)->EditAccepted());
  // On post-edit save, the infobar should not be brought back from the omnibox
  // icon.
  infobar->RemoveSelf();
}

#pragma mark - Private

std::unique_ptr<InfobarModalOverlayRequestCallbackInstaller>
SaveAddressProfileInfobarModalInteractionHandler::CreateModalInstaller() {
  return std::make_unique<
      SaveAddressProfileInfobarModalOverlayRequestCallbackInstaller>(this);
}

autofill::AutofillSaveUpdateAddressProfileDelegateIOS*
SaveAddressProfileInfobarModalInteractionHandler::GetInfoBarDelegate(
    InfoBarIOS* infobar) {
  autofill::AutofillSaveUpdateAddressProfileDelegateIOS* delegate =
      autofill::AutofillSaveUpdateAddressProfileDelegateIOS::
          FromInfobarDelegate(infobar->delegate());
  DCHECK(delegate);
  return delegate;
}
