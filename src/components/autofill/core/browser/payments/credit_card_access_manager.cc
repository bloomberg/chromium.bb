// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/credit_card_access_manager.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/guid.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/metrics/credit_card_form_event_logger.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

CreditCardAccessManager::CreditCardAccessManager(
    AutofillManager* autofill_manager)
    : CreditCardAccessManager(
          autofill_manager->client(),
          autofill_manager->client()->GetPersonalDataManager()) {}

CreditCardAccessManager::CreditCardAccessManager(
    AutofillClient* client,
    PersonalDataManager* personal_data_manager,
    CreditCardFormEventLogger* form_event_logger)
    : client_(client),
      payments_client_(client_->GetPaymentsClient()),
      personal_data_manager_(personal_data_manager),
      form_event_logger_(form_event_logger),
      weak_ptr_factory_(this) {}

CreditCardAccessManager::~CreditCardAccessManager() {}

void CreditCardAccessManager::UpdateCreditCardFormEventLogger() {
  std::vector<CreditCard*> credit_cards = GetCreditCards();

  if (form_event_logger_) {
    size_t server_record_type_count = 0;
    size_t local_record_type_count = 0;
    for (CreditCard* credit_card : credit_cards) {
      if (credit_card->record_type() == CreditCard::LOCAL_CARD)
        local_record_type_count++;
      else
        server_record_type_count++;
    }
    form_event_logger_->set_server_record_type_count(server_record_type_count);
    form_event_logger_->set_local_record_type_count(local_record_type_count);
    form_event_logger_->set_is_context_secure(client_->IsContextSecure());
  }
}

std::vector<CreditCard*> CreditCardAccessManager::GetCreditCards() {
  return personal_data_manager_->GetCreditCards();
}

std::vector<CreditCard*> CreditCardAccessManager::GetCreditCardsToSuggest() {
  const std::vector<CreditCard*> cards_to_suggest =
      personal_data_manager_->GetCreditCardsToSuggest(
          client_->AreServerCardsSupported());

  for (const CreditCard* credit_card : cards_to_suggest) {
    if (form_event_logger_ && !credit_card->bank_name().empty()) {
      form_event_logger_->SetBankNameAvailable();
      break;
    }
  }

  return cards_to_suggest;
}

bool CreditCardAccessManager::ShouldDisplayGPayLogo() {
  for (const CreditCard* credit_card : GetCreditCardsToSuggest()) {
    if (credit_card->record_type() == CreditCard::LOCAL_CARD)
      return false;
  }
  return true;
}

bool CreditCardAccessManager::DeleteCard(const CreditCard* card) {
  // Server cards cannot be deleted from within Chrome.
  bool allowed_to_delete = IsLocalCard(card);

  if (allowed_to_delete)
    personal_data_manager_->DeleteLocalCreditCards({*card});

  return allowed_to_delete;
}

bool CreditCardAccessManager::GetDeletionConfirmationText(
    const CreditCard* card,
    base::string16* title,
    base::string16* body) {
  if (!IsLocalCard(card))
    return false;

  if (title)
    title->assign(card->NetworkOrBankNameAndLastFourDigits());
  if (body) {
    body->assign(l10n_util::GetStringUTF16(
        IDS_AUTOFILL_DELETE_CREDIT_CARD_SUGGESTION_CONFIRMATION_BODY));
  }

  return true;
}

bool CreditCardAccessManager::ShouldClearPreviewedForm() {
  return !is_authentication_in_progress_;
}

CreditCard* CreditCardAccessManager::GetCreditCard(std::string guid) {
  if (base::IsValidGUID(guid)) {
    return personal_data_manager_->GetCreditCardByGUID(guid);
  }
  return nullptr;
}

void CreditCardAccessManager::FetchCreditCard(
    const CreditCard* card,
    base::WeakPtr<Accessor> accessor,
    const base::TimeTicks& timestamp) {
  if (!card || card->record_type() != CreditCard::MASKED_SERVER_CARD) {
    accessor->OnCreditCardFetched(/*did_succeed=*/card != nullptr, card);
    return;
  }

  accessor_ = accessor;
  is_authentication_in_progress_ = true;
  credit_card_cvc_authenticator()->Authenticate(
      card, weak_ptr_factory_.GetWeakPtr(), personal_data_manager_, timestamp);
}

CreditCardCVCAuthenticator*
CreditCardAccessManager::credit_card_cvc_authenticator() {
  if (!cvc_authenticator_)
    cvc_authenticator_.reset(new CreditCardCVCAuthenticator(client_));
  return cvc_authenticator_.get();
}

void CreditCardAccessManager::OnCVCAuthenticationComplete(
    bool did_succeed,
    const CreditCard* card,
    const base::string16& cvc) {
  is_authentication_in_progress_ = false;
  accessor_->OnCreditCardFetched(did_succeed, card, cvc);
}

bool CreditCardAccessManager::IsLocalCard(const CreditCard* card) {
  return card && card->record_type() == CreditCard::LOCAL_CARD;
}

}  // namespace autofill
