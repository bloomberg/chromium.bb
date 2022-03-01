// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/save_update_address_profile_flow_manager.h"

#include <utility>

#include "chrome/browser/ui/android/autofill/save_update_address_profile_prompt_view_android.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/messages/android/messages_feature.h"

namespace autofill {

SaveUpdateAddressProfileFlowManager::SaveUpdateAddressProfileFlowManager() =
    default;
SaveUpdateAddressProfileFlowManager::~SaveUpdateAddressProfileFlowManager() =
    default;

void SaveUpdateAddressProfileFlowManager::OfferSave(
    content::WebContents* web_contents,
    const AutofillProfile& profile,
    const AutofillProfile* original_profile,
    AutofillClient::AddressProfileSavePromptCallback callback) {
  DCHECK(web_contents);
  DCHECK(callback);
  DCHECK(base::FeatureList::IsEnabled(
      autofill::features::kAutofillAddressProfileSavePrompt));

  // If the message or prompt is already shown, suppress the incoming offer.
  if (save_update_address_profile_message_controller_.IsMessageDisplayed() ||
      save_update_address_profile_prompt_controller_) {
    std::move(callback).Run(
        AutofillClient::SaveAddressProfileOfferUserDecision::kAutoDeclined,
        profile);
    return;
  }

  if (base::FeatureList::IsEnabled(
          messages::kMessagesForAndroidInfrastructure)) {
    ShowConfirmationMessage(web_contents, profile, original_profile,
                            std::move(callback));
  } else {
    // Fallback to the default behavior without confirmation.
    std::move(callback).Run(
        AutofillClient::SaveAddressProfileOfferUserDecision::kUserNotAsked,
        profile);
  }
}

SaveUpdateAddressProfileMessageController*
SaveUpdateAddressProfileFlowManager::GetMessageControllerForTest() {
  return &save_update_address_profile_message_controller_;
}

SaveUpdateAddressProfilePromptController*
SaveUpdateAddressProfileFlowManager::GetPromptControllerForTest() {
  return save_update_address_profile_prompt_controller_.get();
}

void SaveUpdateAddressProfileFlowManager::ShowConfirmationMessage(
    content::WebContents* web_contents,
    const AutofillProfile& profile,
    const AutofillProfile* original_profile,
    AutofillClient::AddressProfileSavePromptCallback callback) {
  save_update_address_profile_message_controller_.DisplayMessage(
      web_contents, profile, original_profile, std::move(callback),
      base::BindOnce(
          &SaveUpdateAddressProfileFlowManager::ShowPromptWithDetails,
          // Passing base::Unretained(this) is safe since |this|
          // owns the controller.
          base::Unretained(this)));
}

void SaveUpdateAddressProfileFlowManager::ShowPromptWithDetails(
    content::WebContents* web_contents,
    const AutofillProfile& profile,
    const AutofillProfile* original_profile,
    AutofillClient::AddressProfileSavePromptCallback callback) {
  auto prompt_view_android =
      std::make_unique<SaveUpdateAddressProfilePromptViewAndroid>(web_contents);
  save_update_address_profile_prompt_controller_ = std::make_unique<
      SaveUpdateAddressProfilePromptController>(
      std::move(prompt_view_android), profile, original_profile,
      std::move(callback),
      /*dismissal_callback=*/
      base::BindOnce(
          &SaveUpdateAddressProfileFlowManager::OnPromptWithDetailsDismissed,
          // Passing base::Unretained(this) is safe since |this|
          // owns the controller.
          base::Unretained(this)));
  save_update_address_profile_prompt_controller_->DisplayPrompt();
}

void SaveUpdateAddressProfileFlowManager::OnPromptWithDetailsDismissed() {
  save_update_address_profile_prompt_controller_.reset();
}

}  // namespace autofill
