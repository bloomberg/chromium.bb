// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_DELEGATE_IOS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_DELEGATE_IOS_H_

#include <memory>

#include "base/callback.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

namespace autofill {

// A delegate for the prompt that enables the user to allow or deny storing
// an address profile gathered from a form submission. Only used on iOS.
class AutofillSaveUpdateAddressProfileDelegateIOS
    : public ConfirmInfoBarDelegate {
 public:
  AutofillSaveUpdateAddressProfileDelegateIOS(
      const AutofillProfile& profile,
      const AutofillProfile* original_profile,
      const std::string& locale,
      AutofillClient::AddressProfileSavePromptCallback callback);
  AutofillSaveUpdateAddressProfileDelegateIOS(
      const AutofillSaveUpdateAddressProfileDelegateIOS&) = delete;
  AutofillSaveUpdateAddressProfileDelegateIOS& operator=(
      const AutofillSaveUpdateAddressProfileDelegateIOS&) = delete;
  ~AutofillSaveUpdateAddressProfileDelegateIOS() override;

  // Returns |delegate| as an AutofillSaveUpdateAddressProfileDelegateIOS, or
  // nullptr if it is of another type.
  static AutofillSaveUpdateAddressProfileDelegateIOS* FromInfobarDelegate(
      infobars::InfoBarDelegate* delegate);

  // Returns the address in envelope style in the |profile_|.
  std::u16string GetEnvelopeStyleAddress() const;

  // Returns the phone number in the |profile_|.
  std::u16string GetPhoneNumber() const;

  // Returns the email address in the |profile_|.
  std::u16string GetEmailAddress() const;

  // Returns the subtitle text to be displayed in the save/update banner.
  std::u16string GetDescription() const;

  // Returns subtitle for the update modal.
  std::u16string GetSubtitle();

  // Returns the message button text.
  std::u16string GetMessageActionText() const;

  // Returns the data stored in the |profile_| corresponding to |type|.
  std::u16string GetProfileInfo(ServerFieldType type) const;

  // Returns the profile difference map between |profile_| and
  // |original_profile_|.
  std::vector<ProfileValueDifference> GetProfileDiff() const;

  virtual void EditAccepted();
  void EditDeclined();
  void MessageTimeout();
  void MessageDeclined();

  // Updates |profile_| |type| value to |value|.
  void SetProfileInfo(const ServerFieldType& type, const std::u16string& value);

  const autofill::AutofillProfile* GetProfile() const;
  const autofill::AutofillProfile* GetOriginalProfile() const;

  // ConfirmInfoBarDelegate
  int GetIconId() const override;
  std::u16string GetMessageText() const override;
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  bool Accept() override;
  bool Cancel() override;
  bool EqualsDelegate(infobars::InfoBarDelegate* delegate) const override;

#if defined(UNIT_TEST)
  // Getter for |user_decision_|. Used for the testing purposes.
  AutofillClient::SaveAddressProfileOfferUserDecision user_decision() const {
    return user_decision_;
  }
#endif

 private:
  // Fires the |address_profile_save_prompt_callback_| callback with
  // |user_decision_|.
  void RunSaveAddressProfilePromptCallback();

  // Sets |user_decision_| based on |user_decision|.
  void SetUserDecision(
      AutofillClient::SaveAddressProfileOfferUserDecision user_decision);

  // The application locale.
  std::string locale_;

  // The profile that will be saved if the user accepts.
  AutofillProfile profile_;

  // The original profile that will be updated if the user accepts the update
  // prompt. NULL if saving a new profile.
  absl::optional<AutofillProfile> original_profile_;

  // The callback to run once the user makes a decision.
  AutofillClient::AddressProfileSavePromptCallback
      address_profile_save_prompt_callback_;

  // Records the last user decision based on the interactions with the
  // banner/modal to be sent with |address_profile_save_prompt_callback_|.
  AutofillClient::SaveAddressProfileOfferUserDecision user_decision_ =
      AutofillClient::SaveAddressProfileOfferUserDecision::kIgnored;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_DELEGATE_IOS_H_
