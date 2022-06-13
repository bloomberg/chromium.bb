// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_controller_impl.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_view.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/grit/components_scaled_resources.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

CardUnmaskPromptControllerImpl::CardUnmaskPromptControllerImpl(
    PrefService* pref_service)
    : pref_service_(pref_service) {}

CardUnmaskPromptControllerImpl::~CardUnmaskPromptControllerImpl() {
  if (card_unmask_view_)
    card_unmask_view_->ControllerGone();
}

void CardUnmaskPromptControllerImpl::ShowPrompt(
    CardUnmaskPromptViewFactory card_unmask_view_factory,
    const CreditCard& card,
    AutofillClient::UnmaskCardReason reason,
    base::WeakPtr<CardUnmaskDelegate> delegate) {
  if (card_unmask_view_)
    card_unmask_view_->ControllerGone();

  new_card_link_clicked_ = false;
  shown_timestamp_ = AutofillClock::Now();
  pending_details_ = CardUnmaskDelegate::UserProvidedUnmaskDetails();
  card_ = card;
  reason_ = reason;
  delegate_ = delegate;
  card_unmask_view_ = std::move(card_unmask_view_factory).Run();
  card_unmask_view_->Show();
  unmasking_result_ = AutofillClient::PaymentsRpcResult::kNone;
  unmasking_number_of_attempts_ = 0;
  AutofillMetrics::LogUnmaskPromptEvent(AutofillMetrics::UNMASK_PROMPT_SHOWN,
                                        card_.HasNonEmptyValidNickname());
}

void CardUnmaskPromptControllerImpl::OnVerificationResult(
    AutofillClient::PaymentsRpcResult result) {
  if (!card_unmask_view_)
    return;

  std::u16string error_message;
  switch (result) {
    case AutofillClient::PaymentsRpcResult::kSuccess:
      break;

    case AutofillClient::PaymentsRpcResult::kTryAgainFailure: {
      error_message = l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_UNMASK_PROMPT_ERROR_TRY_AGAIN_CVC);
      break;
    }

    case AutofillClient::PaymentsRpcResult::kPermanentFailure: {
      error_message = l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_UNMASK_PROMPT_ERROR_PERMANENT);
      break;
    }

    case AutofillClient::PaymentsRpcResult::kNetworkError: {
      error_message = l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_UNMASK_PROMPT_ERROR_NETWORK);
      break;
    }

    case AutofillClient::PaymentsRpcResult::kVcnRetrievalTryAgainFailure: {
      error_message = l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_TEMPORARY_ERROR_DESCRIPTION);
      break;
    }

    case AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure: {
      error_message = l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_PERMANENT_ERROR_DESCRIPTION);
      break;
    }

    case AutofillClient::PaymentsRpcResult::kNone:
      NOTREACHED();
      return;
  }

  unmasking_result_ = result;
  AutofillClient::PaymentsRpcCardType card_type =
      card_.record_type() == CreditCard::VIRTUAL_CARD
          ? AutofillClient::PaymentsRpcCardType::kVirtualCard
          : AutofillClient::PaymentsRpcCardType::kServerCard;

  AutofillMetrics::LogRealPanResult(result, card_type);
  AutofillMetrics::LogUnmaskingDuration(
      AutofillClock::Now() - verify_timestamp_, result, card_type);
  if (ShouldDismissUnmaskPromptUponResult(unmasking_result_)) {
    card_unmask_view_->Dismiss();
  } else {
    card_unmask_view_->GotVerificationResult(error_message,
                                             AllowsRetry(result));
  }
}

void CardUnmaskPromptControllerImpl::OnUnmaskDialogClosed() {
  card_unmask_view_ = nullptr;
  LogOnCloseEvents();
  unmasking_result_ = AutofillClient::PaymentsRpcResult::kNone;
  if (delegate_)
    delegate_->OnUnmaskPromptClosed();
}

void CardUnmaskPromptControllerImpl::OnUnmaskPromptAccepted(
    const std::u16string& cvc,
    const std::u16string& exp_month,
    const std::u16string& exp_year,
    bool enable_fido_auth) {
  verify_timestamp_ = AutofillClock::Now();
  unmasking_number_of_attempts_++;
  unmasking_result_ = AutofillClient::PaymentsRpcResult::kNone;
  card_unmask_view_->DisableAndWaitForVerification();

  DCHECK(InputCvcIsValid(cvc));
  base::TrimWhitespace(cvc, base::TRIM_ALL, &pending_details_.cvc);
  if (ShouldRequestExpirationDate()) {
    DCHECK(InputExpirationIsValid(exp_month, exp_year));
    pending_details_.exp_month = exp_month;
    pending_details_.exp_year = exp_year;
  }

  // On Android, FIDO authentication is fully launched and its checkbox should
  // always be shown. Remember the last choice the user made on this device.
#if defined(OS_ANDROID)
  pending_details_.enable_fido_auth = enable_fido_auth;
  pref_service_->SetBoolean(
      prefs::kAutofillCreditCardFidoAuthOfferCheckboxState, enable_fido_auth);
#endif

  // There is a chance the delegate has disappeared (i.e. tab closed) before the
  // unmask response came in. Avoid a crash.
  if (delegate_)
    delegate_->OnUnmaskPromptAccepted(pending_details_);
}

void CardUnmaskPromptControllerImpl::NewCardLinkClicked() {
  new_card_link_clicked_ = true;
}

std::u16string CardUnmaskPromptControllerImpl::GetWindowTitle() const {
#if defined(OS_IOS)
  // The iOS UI has less room for the title so it shows a shorter string.
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_CARD_UNMASK_PROMPT_TITLE);
#else
  // Set title for VCN retrieval errors first.
  if (unmasking_result_ ==
      AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure) {
    return l10n_util::GetStringUTF16(
        IDS_AUTOFILL_VIRTUAL_CARD_PERMANENT_ERROR_TITLE);
  } else if (unmasking_result_ ==
             AutofillClient::PaymentsRpcResult::kVcnRetrievalTryAgainFailure) {
    return l10n_util::GetStringUTF16(
        IDS_AUTOFILL_VIRTUAL_CARD_TEMPORARY_ERROR_TITLE);
  }

  return l10n_util::GetStringFUTF16(
      ShouldRequestExpirationDate()
          ? IDS_AUTOFILL_CARD_UNMASK_PROMPT_EXPIRED_TITLE
          : IDS_AUTOFILL_CARD_UNMASK_PROMPT_TITLE,
      card_.CardIdentifierStringForAutofillDisplay());
#endif
}

std::u16string CardUnmaskPromptControllerImpl::GetInstructionsMessage() const {
// The prompt for server cards should reference Google Payments, whereas the
// prompt for local cards should not.
#if defined(OS_IOS)
  int ids;
  if (reason_ == AutofillClient::UnmaskCardReason::kAutofill &&
      ShouldRequestExpirationDate()) {
    ids = card_.record_type() == autofill::CreditCard::LOCAL_CARD
              ? IDS_AUTOFILL_CARD_UNMASK_PROMPT_INSTRUCTIONS_EXPIRED_LOCAL_CARD
              : IDS_AUTOFILL_CARD_UNMASK_PROMPT_INSTRUCTIONS_EXPIRED;
  } else {
    ids = card_.record_type() == autofill::CreditCard::LOCAL_CARD
              ? IDS_AUTOFILL_CARD_UNMASK_PROMPT_INSTRUCTIONS_LOCAL_CARD
              : IDS_AUTOFILL_CARD_UNMASK_PROMPT_INSTRUCTIONS;
  }
  // The iOS UI shows the card details in the instructions text since they
  // don't fit in the title.
  return l10n_util::GetStringFUTF16(
      ids, card_.CardIdentifierStringForAutofillDisplay());
#else
  // For Google Pay Plex cards, show a specific message that include
  // instructions to find the CVC for their Plex card.
  if (card_.IsGoogleIssuedCard()) {
    return l10n_util::GetStringUTF16(
        IDS_AUTOFILL_CARD_UNMASK_PROMPT_INSTRUCTIONS_GOOGLE_ISSUED_CARD);
  }
  return l10n_util::GetStringUTF16(
      card_.record_type() == autofill::CreditCard::LOCAL_CARD
          ? IDS_AUTOFILL_CARD_UNMASK_PROMPT_INSTRUCTIONS_LOCAL_CARD
          : IDS_AUTOFILL_CARD_UNMASK_PROMPT_INSTRUCTIONS);
#endif
}

std::u16string CardUnmaskPromptControllerImpl::GetOkButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_CARD_UNMASK_CONFIRM_BUTTON);
}

int CardUnmaskPromptControllerImpl::GetCvcImageRid() const {
  return card_.network() == kAmericanExpressCard ? IDR_CREDIT_CARD_CVC_HINT_AMEX
                                                 : IDR_CREDIT_CARD_CVC_HINT;
}

bool CardUnmaskPromptControllerImpl::ShouldRequestExpirationDate() const {
  return card_.ShouldUpdateExpiration() ||
         new_card_link_clicked_;
}

bool CardUnmaskPromptControllerImpl::GetStoreLocallyStartState() const {
  return pref_service_->GetBoolean(
      prefs::kAutofillWalletImportStorageCheckboxState);
}

#if defined(OS_ANDROID)
int CardUnmaskPromptControllerImpl::GetGooglePayImageRid() const {
  return IDR_AUTOFILL_GOOGLE_PAY_WITH_DIVIDER;
}

bool CardUnmaskPromptControllerImpl::ShouldOfferWebauthn() const {
  return delegate_ && delegate_->ShouldOfferFidoAuth();
}

bool CardUnmaskPromptControllerImpl::GetWebauthnOfferStartState() const {
  return pref_service_->GetBoolean(
      prefs::kAutofillCreditCardFidoAuthOfferCheckboxState);
}

bool CardUnmaskPromptControllerImpl::IsCardLocal() const {
  return card_.record_type() == CreditCard::LOCAL_CARD;
}

#endif

bool CardUnmaskPromptControllerImpl::InputCvcIsValid(
    const std::u16string& input_text) const {
  std::u16string trimmed_text;
  base::TrimWhitespace(input_text, base::TRIM_ALL, &trimmed_text);
  return IsValidCreditCardSecurityCode(trimmed_text, card_.network());
}

bool CardUnmaskPromptControllerImpl::InputExpirationIsValid(
    const std::u16string& month,
    const std::u16string& year) const {
  if ((month.size() != 2U && month.size() != 1U) ||
      (year.size() != 4U && year.size() != 2U)) {
    return false;
  }

  int month_value = 0, year_value = 0;
  if (!base::StringToInt(month, &month_value) ||
      !base::StringToInt(year, &year_value)) {
    return false;
  }

  // Convert 2 digit year to 4 digit year.
  if (year_value < 100) {
    base::Time::Exploded now;
    AutofillClock::Now().LocalExplode(&now);
    year_value += (now.year / 100) * 100;
  }

  return IsValidCreditCardExpirationDate(year_value, month_value,
                                         AutofillClock::Now());
}

int CardUnmaskPromptControllerImpl::GetExpectedCvcLength() const {
  return GetCvcLengthForCardNetwork(card_.network());
}

base::TimeDelta CardUnmaskPromptControllerImpl::GetSuccessMessageDuration()
    const {
  return base::Milliseconds(
      card_.record_type() == CreditCard::LOCAL_CARD ||
              reason_ == AutofillClient::UnmaskCardReason::kPaymentRequest
          ? 0
          : 500);
}

AutofillClient::PaymentsRpcResult
CardUnmaskPromptControllerImpl::GetVerificationResult() const {
  return unmasking_result_;
}

bool CardUnmaskPromptControllerImpl::AllowsRetry(
    AutofillClient::PaymentsRpcResult result) {
  if (result == AutofillClient::PaymentsRpcResult::kNetworkError ||
      result == AutofillClient::PaymentsRpcResult::kPermanentFailure ||
      result ==
          AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure ||
      result ==
          AutofillClient::PaymentsRpcResult::kVcnRetrievalTryAgainFailure) {
    return false;
  }
  return true;
}

bool CardUnmaskPromptControllerImpl::ShouldDismissUnmaskPromptUponResult(
    AutofillClient::PaymentsRpcResult result) {
#if defined(OS_ANDROID)
  // For virtual card errors on Android, we'd dismiss the unmask prompt and
  // instead show a different error dialog.
  return result ==
             AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure ||
         result ==
             AutofillClient::PaymentsRpcResult::kVcnRetrievalTryAgainFailure;
#else
  return false;
#endif  // OS_ANDROID
}

void CardUnmaskPromptControllerImpl::LogOnCloseEvents() {
  AutofillMetrics::UnmaskPromptEvent close_reason_event = GetCloseReasonEvent();
  AutofillMetrics::LogUnmaskPromptEvent(close_reason_event,
                                        card_.HasNonEmptyValidNickname());
  AutofillMetrics::LogUnmaskPromptEventDuration(
      AutofillClock::Now() - shown_timestamp_, close_reason_event,
      card_.HasNonEmptyValidNickname());

  if (close_reason_event == AutofillMetrics::UNMASK_PROMPT_CLOSED_NO_ATTEMPTS)
    return;

  if (close_reason_event ==
      AutofillMetrics::UNMASK_PROMPT_CLOSED_ABANDON_UNMASKING) {
    AutofillMetrics::LogTimeBeforeAbandonUnmasking(
        AutofillClock::Now() - verify_timestamp_,
        card_.HasNonEmptyValidNickname());
  }
}

AutofillMetrics::UnmaskPromptEvent
CardUnmaskPromptControllerImpl::GetCloseReasonEvent() {
  if (unmasking_number_of_attempts_ == 0)
    return AutofillMetrics::UNMASK_PROMPT_CLOSED_NO_ATTEMPTS;

  // If NONE and we have a pending request, we have a pending GetRealPan
  // request.
  if (unmasking_result_ == AutofillClient::PaymentsRpcResult::kNone)
    return AutofillMetrics::UNMASK_PROMPT_CLOSED_ABANDON_UNMASKING;

  if (unmasking_result_ == AutofillClient::PaymentsRpcResult::kSuccess) {
    return unmasking_number_of_attempts_ == 1
               ? AutofillMetrics::UNMASK_PROMPT_UNMASKED_CARD_FIRST_ATTEMPT
               : AutofillMetrics::
                     UNMASK_PROMPT_UNMASKED_CARD_AFTER_FAILED_ATTEMPTS;
  }
  return AllowsRetry(unmasking_result_)
             ? AutofillMetrics::
                   UNMASK_PROMPT_CLOSED_FAILED_TO_UNMASK_RETRIABLE_FAILURE
             : AutofillMetrics::
                   UNMASK_PROMPT_CLOSED_FAILED_TO_UNMASK_NON_RETRIABLE_FAILURE;
}

}  // namespace autofill
