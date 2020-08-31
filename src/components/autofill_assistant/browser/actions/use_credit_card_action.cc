// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/use_credit_card_action.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/optional.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/actions/fallback_handler/fallback_data.h"
#include "components/autofill_assistant/browser/actions/fallback_handler/required_field.h"
#include "components/autofill_assistant/browser/actions/fallback_handler/required_fields_fallback_handler.h"
#include "components/autofill_assistant/browser/client_status.h"

namespace autofill_assistant {

UseCreditCardAction::UseCreditCardAction(ActionDelegate* delegate,
                                         const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto.has_use_card());
  prompt_ = proto.use_card().prompt();
  std::vector<RequiredField> required_fields;
  for (const auto& required_field_proto : proto_.use_card().required_fields()) {
    if (required_field_proto.value_expression().empty()) {
      DVLOG(3) << "No fallback filling information provided, skipping field";
      continue;
    }

    required_fields.emplace_back();
    required_fields.back().FromProto(required_field_proto);
  }

  required_fields_fallback_handler_ =
      std::make_unique<RequiredFieldsFallbackHandler>(required_fields,
                                                      delegate);
  selector_ = Selector(proto.use_card().form_field_element());
  selector_.MustBeVisible();
}

UseCreditCardAction::~UseCreditCardAction() = default;

void UseCreditCardAction::InternalProcessAction(
    ProcessActionCallback action_callback) {
  process_action_callback_ = std::move(action_callback);

  if (selector_.empty() && !proto_.use_card().skip_autofill()) {
    VLOG(1) << "UseCreditCard failed: |selector| empty";
    EndAction(ClientStatus(INVALID_ACTION));
    return;
  }
  if (proto_.use_card().skip_autofill() &&
      proto_.use_card().required_fields().empty()) {
    VLOG(1) << "UseCreditCard failed: |skip_autofill| without required fields";
    EndAction(ClientStatus(INVALID_ACTION));
    return;
  }

  // Ensure data already selected in a previous action.
  auto* user_data = delegate_->GetUserData();
  if (user_data->selected_card_.get() == nullptr) {
    EndAction(ClientStatus(PRECONDITION_FAILED));
    return;
  }

  FillFormWithData();
}

void UseCreditCardAction::EndAction(
    const ClientStatus& final_status,
    const base::Optional<ClientStatus>& optional_details_status) {
  UpdateProcessedAction(final_status);
  if (optional_details_status.has_value() && !optional_details_status->ok()) {
    processed_action_proto_->mutable_status_details()->MergeFrom(
        optional_details_status->details());
  }
  std::move(process_action_callback_).Run(std::move(processed_action_proto_));
}

void UseCreditCardAction::FillFormWithData() {
  if (proto_.use_card().skip_autofill()) {
    delegate_->GetFullCard(base::BindOnce(&UseCreditCardAction::OnGetFullCard,
                                          weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  DCHECK(!selector_.empty());
  delegate_->ShortWaitForElement(
      selector_, base::BindOnce(&UseCreditCardAction::OnWaitForElement,
                                weak_ptr_factory_.GetWeakPtr()));
}

void UseCreditCardAction::OnWaitForElement(const ClientStatus& element_status) {
  if (!element_status.ok()) {
    EndAction(element_status);
    return;
  }

  delegate_->GetFullCard(base::BindOnce(&UseCreditCardAction::OnGetFullCard,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void UseCreditCardAction::OnGetFullCard(
    std::unique_ptr<autofill::CreditCard> card,
    const base::string16& cvc) {
  if (!card) {
    EndAction(ClientStatus(GET_FULL_CARD_FAILED));
    return;
  }

  auto fallback_data = CreateFallbackData(cvc, *card);

  if (proto_.use_card().skip_autofill()) {
    required_fields_fallback_handler_->CheckAndFallbackRequiredFields(
        OkClientStatus(), std::move(fallback_data),
        base::BindOnce(&UseCreditCardAction::EndAction,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  delegate_->FillCardForm(
      std::move(card), cvc, selector_,
      base::BindOnce(&UseCreditCardAction::OnFormFilled,
                     weak_ptr_factory_.GetWeakPtr(), std::move(fallback_data)));
}

void UseCreditCardAction::OnFormFilled(
    std::unique_ptr<FallbackData> fallback_data,
    const ClientStatus& status) {
  required_fields_fallback_handler_->CheckAndFallbackRequiredFields(
      status, std::move(fallback_data),
      base::BindOnce(&UseCreditCardAction::EndAction,
                     weak_ptr_factory_.GetWeakPtr()));
}

std::unique_ptr<FallbackData> UseCreditCardAction::CreateFallbackData(
    const base::string16& cvc,
    const autofill::CreditCard& card) {
  auto fallback_data = std::make_unique<FallbackData>();

  fallback_data->field_values.emplace(
      static_cast<int>(
          UseCreditCardProto::RequiredField::CREDIT_CARD_VERIFICATION_CODE),
      base::UTF16ToUTF8(cvc));
  fallback_data->field_values.emplace(
      (int)UseCreditCardProto::RequiredField::CREDIT_CARD_RAW_NUMBER,
      base::UTF16ToUTF8(card.GetRawInfo(autofill::CREDIT_CARD_NUMBER)));
  fallback_data->field_values.emplace(
      static_cast<int>(UseCreditCardProto::RequiredField::CREDIT_CARD_NETWORK),
      autofill::data_util::GetPaymentRequestData(card.network())
          .basic_card_issuer_network);

  fallback_data->AddFormGroup(card);
  return fallback_data;
}
}  // namespace autofill_assistant
