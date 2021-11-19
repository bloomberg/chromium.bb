// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_client.h"

#include "base/no_destructor.h"
#include "components/autofill/core/browser/autofill_ablation_study.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/single_field_form_fill_router.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/version_info/channel.h"

namespace autofill {

AutofillClient::PopupOpenArgs::PopupOpenArgs() = default;
AutofillClient::PopupOpenArgs::PopupOpenArgs(
    const gfx::RectF& element_bounds,
    base::i18n::TextDirection text_direction,
    std::vector<autofill::Suggestion> suggestions,
    AutoselectFirstSuggestion autoselect_first_suggestion,
    PopupType popup_type)
    : element_bounds(element_bounds),
      text_direction(text_direction),
      suggestions(std::move(suggestions)),
      autoselect_first_suggestion(autoselect_first_suggestion),
      popup_type(popup_type) {}
AutofillClient::PopupOpenArgs::PopupOpenArgs(
    const AutofillClient::PopupOpenArgs&) = default;
AutofillClient::PopupOpenArgs::PopupOpenArgs(AutofillClient::PopupOpenArgs&&) =
    default;
AutofillClient::PopupOpenArgs::~PopupOpenArgs() = default;
AutofillClient::PopupOpenArgs& AutofillClient::PopupOpenArgs::operator=(
    const AutofillClient::PopupOpenArgs&) = default;
AutofillClient::PopupOpenArgs& AutofillClient::PopupOpenArgs::operator=(
    AutofillClient::PopupOpenArgs&&) = default;

version_info::Channel AutofillClient::GetChannel() const {
  return version_info::Channel::UNKNOWN;
}

std::unique_ptr<SingleFieldFormFillRouter>
AutofillClient::GetSingleFieldFormFillRouter() {
  return std::make_unique<SingleFieldFormFillRouter>(
      GetAutocompleteHistoryManager());
}

AutofillOfferManager* AutofillClient::GetAutofillOfferManager() {
  return nullptr;
}

std::string AutofillClient::GetVariationConfigCountryCode() const {
  return std::string();
}

profile_metrics::BrowserProfileType AutofillClient::GetProfileType() const {
  // This is an abstract interface and thus never instantiated directly,
  // therefore it is safe to always return |kRegular| here.
  return profile_metrics::BrowserProfileType::kRegular;
}

void AutofillClient::ShowUnmaskAuthenticatorSelectionDialog(
    const std::vector<CardUnmaskChallengeOption>& challenge_options,
    base::OnceCallback<void(const std::string&)>
        confirm_unmask_challenge_option_callback,
    base::OnceClosure cancel_unmasking_closure) {
  // This is overridden by platform subclasses. Currently only
  // ChromeAutofillClient (Chrome Desktop and Clank) implements this.
}

void AutofillClient::DismissUnmaskAuthenticatorSelectionDialog(
    bool server_success) {
  // This is overridden by platform subclasses. Currently only
  // ChromeAutofillClient (Chrome Desktop and Clank) implements this.
}

#if !defined(OS_IOS)
std::unique_ptr<webauthn::InternalAuthenticator>
AutofillClient::CreateCreditCardInternalAuthenticator(
    content::RenderFrameHost* rfh) {
  return nullptr;
}
#endif

void AutofillClient::ShowCardUnmaskOtpInputDialog(
    const size_t& otp_length,
    base::WeakPtr<OtpUnmaskDelegate> delegate) {
  // This is overridden by platform subclasses. Currently only
  // ChromeAutofillClient (Chrome Desktop and Clank) implements this.
}

void AutofillClient::OnUnmaskOtpVerificationResult(
    OtpUnmaskResult unmask_result) {
  // This is overridden by platform subclasses. Currently only
  // ChromeAutofillClient (Chrome Desktop and Clank) implements this.
}

void AutofillClient::ShowOfferNotificationIfApplicable(
    const AutofillOfferData* offer) {
  // This is overridden by platform subclasses. Currently only
  // ChromeAutofillClient (Chrome Desktop and Clank) implements this.
}

void AutofillClient::OnVirtualCardDataAvailable(
    const std::u16string& masked_card_identifier_string,
    const CreditCard* credit_card,
    const std::u16string& cvc,
    const gfx::Image& card_image) {
  // This is overridden by platform subclasses. Currently only
  // ChromeAutofillClient (Chrome Desktop & Android) implements this.
}

void AutofillClient::ShowVirtualCardErrorDialog(bool is_permanent_error) {
  // This is overridden by platform subclasses. Currently only
  // ChromeAutofillClient (Chrome Desktop & Android) implements this.
}

void AutofillClient::ShowAutofillProgressDialog(
    base::OnceClosure cancel_callback) {
  // This is overridden by platform subclasses. Currently only
  // ChromeAutofillClient (Chrome Desktop & Android) implements this.
}

void AutofillClient::CloseAutofillProgressDialog(
    bool show_confirmation_before_closing) {
  // This is overridden by platform subclasses. Currently only
  // ChromeAutofillClient (Chrome Desktop & Android) implements this.
}

bool AutofillClient::IsAutofillAssistantShowing() {
  return false;
}

LogManager* AutofillClient::GetLogManager() const {
  return nullptr;
}

const AutofillAblationStudy& AutofillClient::GetAblationStudy() const {
  // As finch configs are profile independent we can use a static instance here.
  static base::NoDestructor<AutofillAblationStudy> ablation_study;
  return *ablation_study;
}

}  // namespace autofill
