// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/use_address_action.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/actions/fallback_handler/fallback_data.h"
#include "components/autofill_assistant/browser/actions/fallback_handler/required_field.h"
#include "components/autofill_assistant/browser/actions/fallback_handler/required_fields_fallback_handler.h"
#include "components/autofill_assistant/browser/client_status.h"

namespace autofill_assistant {

UseAddressAction::UseAddressAction(ActionDelegate* delegate,
                                   const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto.has_use_address());
  prompt_ = proto.use_address().prompt();
  name_ = proto.use_address().name();
  std::vector<RequiredField> required_fields;
  for (const auto& required_field_proto :
       proto_.use_address().required_fields()) {
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
  selector_ = Selector(proto.use_address().form_field_element());
  selector_.MustBeVisible();
}

UseAddressAction::~UseAddressAction() = default;

void UseAddressAction::InternalProcessAction(
    ProcessActionCallback action_callback) {
  process_action_callback_ = std::move(action_callback);

  if (selector_.empty() && !proto_.use_address().skip_autofill()) {
    VLOG(1) << "UseAddress failed: |selector| empty";
    EndAction(ClientStatus(INVALID_ACTION));
    return;
  }
  if (proto_.use_address().skip_autofill() &&
      proto_.use_address().required_fields().empty()) {
    VLOG(1) << "UseAddress failed: |skip_autofill| without required fields";
    EndAction(ClientStatus(INVALID_ACTION));
    return;
  }

  // Ensure data already selected in a previous action.
  auto* user_data = delegate_->GetUserData();
  if (!user_data->has_selected_address(name_)) {
    auto* error_info = processed_action_proto_->mutable_status_details()
                           ->mutable_autofill_error_info();
    error_info->set_address_key_requested(name_);
    error_info->set_client_memory_address_key_names(
        user_data->GetAllAddressKeyNames());
    error_info->set_address_pointee_was_null(
        !user_data->has_selected_address(name_) ||
        !user_data->selected_address(name_));
    EndAction(ClientStatus(PRECONDITION_FAILED));
    return;
  }

  FillFormWithData();
}

void UseAddressAction::EndAction(
    const ClientStatus& final_status,
    const base::Optional<ClientStatus>& optional_details_status) {
  UpdateProcessedAction(final_status);
  if (optional_details_status.has_value() && !optional_details_status->ok()) {
    processed_action_proto_->mutable_status_details()->MergeFrom(
        optional_details_status->details());
  }
  std::move(process_action_callback_).Run(std::move(processed_action_proto_));
}

void UseAddressAction::FillFormWithData() {
  if (proto_.use_address().skip_autofill()) {
    VLOG(3) << "Retrieving address from client memory under '" << name_ << "'.";
    const autofill::AutofillProfile* profile =
        delegate_->GetUserData()->selected_address(name_);
    DCHECK(profile);
    auto fallback_data = CreateFallbackData(*profile);

    required_fields_fallback_handler_->CheckAndFallbackRequiredFields(
        OkClientStatus(), std::move(fallback_data),
        base::BindOnce(&UseAddressAction::EndAction,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  DCHECK(!selector_.empty());
  delegate_->ShortWaitForElement(
      selector_, base::BindOnce(&UseAddressAction::OnWaitForElement,
                                weak_ptr_factory_.GetWeakPtr()));
}

void UseAddressAction::OnWaitForElement(const ClientStatus& element_status) {
  if (!element_status.ok()) {
    EndAction(ClientStatus(element_status.proto_status()));
    return;
  }

  VLOG(3) << "Retrieving address from client memory under '" << name_ << "'.";
  const autofill::AutofillProfile* profile =
      delegate_->GetUserData()->selected_address(name_);
  DCHECK(profile);
  auto fallback_data = CreateFallbackData(*profile);

  delegate_->FillAddressForm(
      profile, selector_,
      base::BindOnce(&UseAddressAction::OnFormFilled,
                     weak_ptr_factory_.GetWeakPtr(), std::move(fallback_data)));
}

void UseAddressAction::OnFormFilled(std::unique_ptr<FallbackData> fallback_data,
                                    const ClientStatus& status) {
  required_fields_fallback_handler_->CheckAndFallbackRequiredFields(
      status, std::move(fallback_data),
      base::BindOnce(&UseAddressAction::EndAction,
                     weak_ptr_factory_.GetWeakPtr()));
}

std::unique_ptr<FallbackData> UseAddressAction::CreateFallbackData(
    const autofill::AutofillProfile& profile) {
  auto fallback_data = std::make_unique<FallbackData>();

  fallback_data->AddFormGroup(profile);
  return fallback_data;
}

}  // namespace autofill_assistant
