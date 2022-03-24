// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/collect_user_data_action.h"

#include <algorithm>
#include <array>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/i18n/case_conversion.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/address_i18n.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/cud_condition.pb.h"
#include "components/autofill_assistant/browser/field_formatter.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/user_data_util.h"
#include "components/autofill_assistant/browser/website_login_manager_impl.h"
#include "components/strings/grit/components_strings.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "third_party/libaddressinput/chromium/addressinput_util.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill_assistant {
namespace {

static constexpr int kDefaultMaxNumberContactSummaryLines = 1;
static constexpr std::array<autofill_assistant::AutofillContactField, 2>
    kDefaultContactSummaryFields = {autofill_assistant::EMAIL_ADDRESS,
                                    autofill_assistant::NAME_FULL};
static constexpr int kDefaultMaxNumberContactFullLines = 2;
static constexpr std::array<autofill_assistant::AutofillContactField, 2>
    kDefaultContactFullFields = {autofill_assistant::NAME_FULL,
                                 autofill_assistant::EMAIL_ADDRESS};

bool IsReadOnlyAdditionalSection(
    const autofill_assistant::UserFormSectionProto& section) {
  switch (section.section_case()) {
    case autofill_assistant::UserFormSectionProto::kStaticTextSection:
      return true;
    case autofill_assistant::UserFormSectionProto::kTextInputSection:
    case autofill_assistant::UserFormSectionProto::kPopupListSection:
    case autofill_assistant::UserFormSectionProto::SECTION_NOT_SET:
      return false;
  }
}

bool OnlyLoginRequested(
    const CollectUserDataOptions& collect_user_data_options) {
  if (!collect_user_data_options.request_login_choice) {
    return false;
  }

  const bool has_non_readonly_input_sections =
      !base::ranges::all_of(
          collect_user_data_options.additional_prepended_sections,
          IsReadOnlyAdditionalSection) ||
      !base::ranges::all_of(
          collect_user_data_options.additional_appended_sections,
          IsReadOnlyAdditionalSection);
  return !has_non_readonly_input_sections &&
         !collect_user_data_options.request_payer_name &&
         !collect_user_data_options.request_payer_email &&
         !collect_user_data_options.request_payer_phone &&
         !collect_user_data_options.request_shipping &&
         !collect_user_data_options.request_payment_method &&
         !collect_user_data_options.request_date_time_range &&
         collect_user_data_options.accept_terms_and_conditions_text.empty() &&
         !collect_user_data_options.additional_model_identifier_to_check
              .has_value();
}

bool IsValidLoginChoice(
    const LoginChoice* login_choice,
    const CollectUserDataOptions& collect_user_data_options) {
  return !collect_user_data_options.request_login_choice ||
         login_choice != nullptr;
}

bool IsValidTermsChoice(
    TermsAndConditionsState terms_state,
    const CollectUserDataOptions& collect_user_data_options) {
  return collect_user_data_options.accept_terms_and_conditions_text.empty() ||
         terms_state != TermsAndConditionsState::NOT_SELECTED;
}

// Checks |proto| and writes an error message to |error| if fields are missing.
bool IsValidDateTimeRangeProto(
    const autofill_assistant::DateTimeRangeProto& proto,
    std::string* error) {
  std::vector<std::string> missing_fields;
  if (proto.start_date_label().empty())
    missing_fields.emplace_back("start_date_label");
  if (proto.start_time_label().empty())
    missing_fields.emplace_back("start_time_label");
  if (proto.end_date_label().empty())
    missing_fields.emplace_back("end_date_label");
  if (proto.end_time_label().empty())
    missing_fields.emplace_back("end_time_label");
  if (!proto.has_start_date())
    missing_fields.emplace_back("start_date");
  if (!proto.has_start_time_slot())
    missing_fields.emplace_back("start_time_slot");
  if (!proto.has_end_date())
    missing_fields.emplace_back("end_date");
  if (!proto.has_end_time_slot())
    missing_fields.emplace_back("end_time_slot");
  if (!proto.has_min_date())
    missing_fields.emplace_back("min_date");
  if (!proto.has_max_date())
    missing_fields.emplace_back("max_date");
  if (proto.time_slots().empty())
    missing_fields.emplace_back("time_slots");
  if (proto.date_not_set_error().empty())
    missing_fields.emplace_back("date_not_set_error");
  if (proto.time_not_set_error().empty())
    missing_fields.emplace_back("time_not_set_error");

  if (error != nullptr && !missing_fields.empty()) {
    error->assign("The following fields are missing or empty: ");
    error->append(base::JoinString(missing_fields, ", "));
  }

  return missing_fields.empty();
}

bool IsValidDateTimeRange(
    const absl::optional<autofill_assistant::DateProto>& start_date,
    const absl::optional<int> start_timeslot,
    const absl::optional<autofill_assistant::DateProto> end_date,
    const absl::optional<int> end_timeslot,
    const CollectUserDataOptions& collect_user_data_options) {
  if (!collect_user_data_options.request_date_time_range) {
    return true;
  }
  if (!start_date.has_value() || !start_timeslot.has_value() ||
      !end_date.has_value() || !end_timeslot.has_value()) {
    return false;
  }

  auto temp_start_date = start_date;
  auto temp_start_timeslot = start_timeslot;
  auto temp_end_date = end_date;
  auto temp_end_timeslot = end_timeslot;
  return !autofill_assistant::CollectUserDataAction::SanitizeDateTimeRange(
      &temp_start_date, &temp_start_timeslot, &temp_end_date,
      &temp_end_timeslot, collect_user_data_options, false);
}

bool IsValidUserFormSection(
    const autofill_assistant::UserFormSectionProto& proto) {
  if (proto.title().empty()) {
    VLOG(2) << "UserFormSectionProto: Empty title not allowed.";
    return false;
  }
  switch (proto.section_case()) {
    case autofill_assistant::UserFormSectionProto::kStaticTextSection:
      if (proto.static_text_section().text().empty()) {
        VLOG(2) << "StaticTextSectionProto: Empty text not allowed.";
        return false;
      }
      break;
    case autofill_assistant::UserFormSectionProto::kTextInputSection: {
      if (proto.text_input_section().input_fields().empty()) {
        VLOG(2) << "TextInputProto: At least one input must be specified.";
        return false;
      }
      std::set<std::string> memory_keys;
      for (const auto& input_field :
           proto.text_input_section().input_fields()) {
        if (input_field.client_memory_key().empty()) {
          VLOG(2) << "TextInputProto: Memory key must be specified.";
          return false;
        }
        if (!memory_keys.insert(input_field.client_memory_key()).second) {
          VLOG(2) << "TextInputProto: Duplicate memory keys ("
                  << input_field.client_memory_key() << ").";
          return false;
        }
      }
      break;
    }
    case autofill_assistant::UserFormSectionProto::kPopupListSection:
      if (proto.popup_list_section().item_names().empty()) {
        VLOG(2) << "PopupListProto: At least one item must be specified.";
        return false;
      }
      if (proto.popup_list_section().initial_selection().size() > 1 &&
          proto.popup_list_section().allow_multiselect() == false) {
        VLOG(2) << "PopupListProto: multiple initial selections for a single "
                   "selection popup.";
        return false;
      }
      for (int selection : proto.popup_list_section().initial_selection()) {
        if (selection >= proto.popup_list_section().item_names().size() ||
            selection < 0) {
          VLOG(2) << "PopupListProto: an initial selection is out of bounds.";
          return false;
        }
      }
      break;
    case autofill_assistant::UserFormSectionProto::SECTION_NOT_SET:
      VLOG(2) << "UserFormSectionProto: section oneof not set.";
      return false;
  }
  return true;
}

bool IsValidUserModel(const autofill_assistant::UserModel& user_model,
                      const autofill_assistant::CollectUserDataOptions&
                          collect_user_data_options) {
  if (!collect_user_data_options.additional_model_identifier_to_check
           .has_value()) {
    return true;
  }

  auto valid = user_model.GetValue(
      *collect_user_data_options.additional_model_identifier_to_check);
  if (!valid.has_value()) {
    VLOG(1) << "Error evaluating validity of user model: '"
            << *collect_user_data_options.additional_model_identifier_to_check
            << "' not found in user model.";
    return false;
  }

  if (valid->kind_case() != autofill_assistant::ValueProto::kBooleans ||
      valid->booleans().values().size() != 1) {
    VLOG(1) << "Error evaluating validity of user model: expected single "
               "boolean, but got "
            << *valid;
    return false;
  }

  return valid->booleans().values(0);
}

// Merges |model_a| and |model_b| into a new model.
// TODO(arbesser): deal with overlapping keys.
autofill_assistant::ModelProto MergeModelProtos(
    const autofill_assistant::ModelProto& model_a,
    const autofill_assistant::ModelProto& model_b) {
  autofill_assistant::ModelProto model_merged;
  for (const auto& value : model_a.values()) {
    *model_merged.add_values() = value;
  }
  for (const auto& value : model_b.values()) {
    *model_merged.add_values() = value;
  }
  return model_merged;
}

void FillProtoForAdditionalSection(
    const autofill_assistant::UserFormSectionProto& additional_section,
    const UserData& user_data,
    autofill_assistant::ProcessedActionProto* processed_action_proto) {
  switch (additional_section.section_case()) {
    case autofill_assistant::UserFormSectionProto::kTextInputSection:
      for (const auto& text_input :
           additional_section.text_input_section().input_fields()) {
        if (user_data.HasAdditionalValue(text_input.client_memory_key())) {
          processed_action_proto->mutable_collect_user_data_result()
              ->add_set_text_input_memory_keys(text_input.client_memory_key());
          if (additional_section.send_result_to_backend()) {
            auto* value =
                user_data.GetAdditionalValue(text_input.client_memory_key());
            autofill_assistant::ModelProto_ModelValue model_value;
            model_value.set_identifier(text_input.client_memory_key());
            *model_value.mutable_value() = *value;
            *processed_action_proto->mutable_collect_user_data_result()
                 ->add_additional_sections_values() = model_value;
          }
        }
      }
      break;
    case autofill_assistant::UserFormSectionProto::kPopupListSection:
      if (user_data.HasAdditionalValue(
              additional_section.popup_list_section().additional_value_key()) &&
          additional_section.send_result_to_backend()) {
        auto* value = user_data.GetAdditionalValue(
            additional_section.popup_list_section().additional_value_key());
        autofill_assistant::ModelProto_ModelValue model_value;
        model_value.set_identifier(
            additional_section.popup_list_section().additional_value_key());
        *model_value.mutable_value() = *value;
        *processed_action_proto->mutable_collect_user_data_result()
             ->add_additional_sections_values() = model_value;
      }
      break;
    case autofill_assistant::UserFormSectionProto::kStaticTextSection:
    case autofill_assistant::UserFormSectionProto::SECTION_NOT_SET:
      // Do nothing.
      break;
  }
}

bool IsAdditionalSectionComplete(
    const UserData& user_data,
    const autofill_assistant::UserFormSectionProto& section) {
  if (section.section_case() !=
          autofill_assistant::UserFormSectionProto::kPopupListSection ||
      !section.popup_list_section().selection_mandatory()) {
    return true;
  }
  auto* value = user_data.GetAdditionalValue(
      section.popup_list_section().additional_value_key());
  if (value != nullptr && !value->ints().values().empty()) {
    return true;
  }
  return false;
}

bool AreAdditionalSectionsComplete(
    const UserData& user_data,
    const CollectUserDataOptions& collect_user_data_options) {
  for (const auto& section :
       collect_user_data_options.additional_prepended_sections) {
    if (!IsAdditionalSectionComplete(user_data, section)) {
      return false;
    }
  }
  for (const auto& section :
       collect_user_data_options.additional_appended_sections) {
    if (!IsAdditionalSectionComplete(user_data, section)) {
      return false;
    }
  }
  return true;
}

void SetInitialUserDataForAdditionalSection(
    const autofill_assistant::UserFormSectionProto& additional_section,
    UserData* user_data) {
  switch (additional_section.section_case()) {
    case autofill_assistant::UserFormSectionProto::kTextInputSection: {
      for (const auto& text_input :
           additional_section.text_input_section().input_fields()) {
        autofill_assistant::ValueProto value;
        value.mutable_strings()->add_values(text_input.value());
        user_data->SetAdditionalValue(text_input.client_memory_key(), value);
      }
      break;
    }
    case autofill_assistant::UserFormSectionProto::kPopupListSection: {
      autofill_assistant::ValueProto value;
      for (const auto& selection :
           additional_section.popup_list_section().initial_selection()) {
        value.mutable_ints()->add_values(selection);
      }
      user_data->SetAdditionalValue(
          additional_section.popup_list_section().additional_value_key(),
          value);
      break;
    }
    case autofill_assistant::UserFormSectionProto::kStaticTextSection:
    case autofill_assistant::UserFormSectionProto::SECTION_NOT_SET:
      // Do nothing.
      break;
  }
}

bool RequiresPaymentMethod(
    const CollectUserDataOptions& collect_user_data_options) {
  return collect_user_data_options.request_payment_method;
}

bool RequiresContact(const CollectUserDataOptions& collect_user_data_options) {
  return collect_user_data_options.request_payer_name ||
         collect_user_data_options.request_payer_email ||
         collect_user_data_options.request_payer_phone;
}

bool RequiresShipping(const CollectUserDataOptions& collect_user_data_options) {
  return collect_user_data_options.request_shipping;
}

bool RequiresAddress(const CollectUserDataOptions& collect_user_data_options) {
  return RequiresShipping(collect_user_data_options) ||
         RequiresPaymentMethod(collect_user_data_options);
}

void AddProtoDataToAutofillDataModel(
    const google::protobuf::Map<int32_t, AutofillEntryProto>& data,
    const std::string& locale,
    autofill::AutofillDataModel* model) {
  for (const auto& it : data) {
    if (it.second.raw()) {
      model->SetRawInfo(static_cast<autofill::ServerFieldType>(it.first),
                        base::UTF8ToUTF16(it.second.value()));
    } else {
      model->SetInfo(static_cast<autofill::ServerFieldType>(it.first),
                     base::UTF8ToUTF16(it.second.value()), locale);
    }
  }
}

}  // namespace

CollectUserDataAction::LoginDetails::LoginDetails(
    bool _choose_automatically_if_no_stored_login,
    const std::string& _payload,
    const WebsiteLoginManager::Login& _login)
    : choose_automatically_if_no_stored_login(
          _choose_automatically_if_no_stored_login),
      payload(_payload),
      login(_login) {}

CollectUserDataAction::LoginDetails::LoginDetails(
    bool _choose_automatically_if_no_stored_login,
    const std::string& _payload)
    : choose_automatically_if_no_stored_login(
          _choose_automatically_if_no_stored_login),
      payload(_payload) {}

CollectUserDataAction::LoginDetails::~LoginDetails() = default;

CollectUserDataAction::CollectUserDataAction(ActionDelegate* delegate,
                                             const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_collect_user_data());
}

CollectUserDataAction::~CollectUserDataAction() {
  delegate_->GetPersonalDataManager()->RemoveObserver(this);

  MaybeLogMetrics();
}

void CollectUserDataAction::MaybeLogMetrics() {
  if (!shown_to_user_ || metrics_data_.metrics_logged)
    return;

  metrics_data_.metrics_logged = true;
  Metrics::RecordPaymentRequestPrefilledSuccess(
      metrics_data_.initially_prefilled, metrics_data_.action_successful);
  Metrics::RecordPaymentRequestAutofillChanged(
      metrics_data_.personal_data_changed, metrics_data_.action_successful);

  Metrics::RecordCollectUserDataSuccess(
      delegate_->GetUkmRecorder(), metrics_data_.source_id,
      metrics_data_.action_successful,
      action_stopwatch_.TotalActiveTime().InMilliseconds());
  if (RequiresContact(*collect_user_data_options_)) {
    Metrics::RecordContactMetrics(
        delegate_->GetUkmRecorder(), metrics_data_.source_id,
        metrics_data_.complete_contacts_initial_count,
        metrics_data_.incomplete_contacts_initial_count,
        metrics_data_.selected_contact_field_bitmask,
        metrics_data_.contact_selection_state);
  }

  if (RequiresPaymentMethod(*collect_user_data_options_)) {
    Metrics::RecordCreditCardMetrics(
        delegate_->GetUkmRecorder(), metrics_data_.source_id,
        metrics_data_.complete_credit_cards_initial_count,
        metrics_data_.incomplete_credit_cards_initial_count,
        metrics_data_.selected_credit_card_field_bitmask,
        metrics_data_.selected_billing_address_field_bitmask,
        metrics_data_.credit_card_selection_state);
  }

  if (RequiresShipping(*collect_user_data_options_)) {
    Metrics::RecordShippingMetrics(
        delegate_->GetUkmRecorder(), metrics_data_.source_id,
        metrics_data_.complete_shipping_addresses_initial_count,
        metrics_data_.incomplete_shipping_addresses_initial_count,
        metrics_data_.selected_shipping_address_field_bitmask,
        metrics_data_.shipping_selection_state);
  }
}

bool CollectUserDataAction::ShouldInterruptOnPause() const {
  return true;
}

void CollectUserDataAction::InternalProcessAction(
    ProcessActionCallback callback) {
  callback_ = std::move(callback);
  if (!CreateOptionsFromProto()) {
    EndAction(ClientStatus(INVALID_ACTION));
    return;
  }

  // If Chrome password manager logins are requested, we need to asynchronously
  // obtain them before showing the UI.
  auto collect_user_data = proto_.collect_user_data();
  auto password_manager_option = base::ranges::find_if(
      collect_user_data.login_details().login_options(),
      [&](const LoginDetailsProto::LoginOptionProto& option) {
        return option.type_case() ==
               LoginDetailsProto::LoginOptionProto::kPasswordManager;
      });
  bool requests_pwm_logins =
      password_manager_option !=
      collect_user_data.login_details().login_options().end();

  collect_user_data_options_->confirm_callback =
      base::BindOnce(&CollectUserDataAction::OnGetUserData,
                     weak_ptr_factory_.GetWeakPtr(), collect_user_data);
  collect_user_data_options_->additional_actions_callback =
      base::BindOnce(&CollectUserDataAction::OnAdditionalActionTriggered,
                     weak_ptr_factory_.GetWeakPtr());
  collect_user_data_options_->terms_link_callback =
      base::BindOnce(&CollectUserDataAction::OnTermsAndConditionsLinkClicked,
                     weak_ptr_factory_.GetWeakPtr());
  collect_user_data_options_->selected_user_data_changed_callback =
      base::BindRepeating(&CollectUserDataAction::OnSelectionStateChanged,
                          weak_ptr_factory_.GetWeakPtr());
  collect_user_data_options_->reload_data_callback = base::BindOnce(
      &CollectUserDataAction::ReloadAction, weak_ptr_factory_.GetWeakPtr());
  if (requests_pwm_logins) {
    delegate_->GetWebsiteLoginManager()->GetLoginsForUrl(
        delegate_->GetWebContents()->GetLastCommittedURL(),
        base::BindOnce(&CollectUserDataAction::OnGetLogins,
                       weak_ptr_factory_.GetWeakPtr(),
                       *password_manager_option));
  } else {
    ShowToUser();
  }
  action_stopwatch_.StartWaitTime();
}

void CollectUserDataAction::EndAction(const ClientStatus& status) {
  metrics_data_.action_successful = status.ok();
  MaybeLogMetrics();
  if (metrics_data_.action_successful) {
    delegate_->SetLastSuccessfulUserDataOptions(
        std::move(collect_user_data_options_));
  }
  delegate_->CleanUpAfterPrompt();
  UpdateProcessedAction(status);
  std::move(callback_).Run(std::move(processed_action_proto_));
}

bool CollectUserDataAction::HasActionEnded() const {
  return !callback_;
}

void CollectUserDataAction::OnGetLogins(
    const LoginDetailsProto::LoginOptionProto& login_option,
    std::vector<WebsiteLoginManager::Login> logins) {
  for (const auto& login : logins) {
    auto identifier =
        base::NumberToString(collect_user_data_options_->login_choices.size());
    collect_user_data_options_->login_choices.emplace_back(
        identifier, login.username, login_option.sublabel(),
        login_option.sublabel_accessibility_hint(),
        login_option.preselection_priority(),
        login_option.has_info_popup()
            ? absl::make_optional(login_option.info_popup())
            : absl::nullopt,
        login_option.has_edit_button_content_description()
            ? absl::make_optional(
                  login_option.edit_button_content_description())
            : absl::nullopt);
    login_details_map_.emplace(
        identifier, std::make_unique<LoginDetails>(
                        login_option.choose_automatically_if_no_stored_login(),
                        login_option.payload(), login));
  }
  ShowToUser();
}

void CollectUserDataAction::ShowToUser() {
  // Set initial state.
  delegate_->WriteUserData(base::BindOnce(&CollectUserDataAction::OnShowToUser,
                                          weak_ptr_factory_.GetWeakPtr()));
}

void CollectUserDataAction::OnShowToUser(UserData* user_data,
                                         UserData::FieldChange* field_change) {
  // merge the new proto_ into the existing user_data. the proto_ always takes
  // precedence over the existing user_data.
  *field_change = UserData::FieldChange::ALL;
  auto collect_user_data = proto_.collect_user_data();
  // the backend should explicitly set the terms and conditions state on every
  // new action.
  switch (collect_user_data.terms_and_conditions_state()) {
    case CollectUserDataProto::NOT_SELECTED:
      user_data->terms_and_conditions_ = NOT_SELECTED;
      break;
    case CollectUserDataProto::ACCEPTED:
      user_data->terms_and_conditions_ = ACCEPTED;
      break;
    case CollectUserDataProto::REVIEW_REQUIRED:
      user_data->terms_and_conditions_ = REQUIRES_REVIEW;
      break;
  }
  for (const auto& additional_section :
       collect_user_data.additional_prepended_sections()) {
    SetInitialUserDataForAdditionalSection(additional_section, user_data);
  }
  for (const auto& additional_section :
       collect_user_data.additional_appended_sections()) {
    SetInitialUserDataForAdditionalSection(additional_section, user_data);
  }

  if (collect_user_data_options_->request_login_choice &&
      collect_user_data_options_->login_choices.empty()) {
    EndAction(ClientStatus(COLLECT_USER_DATA_ERROR));
    return;
  }

  bool has_password_manager_logins = base::ranges::any_of(
      login_details_map_,
      [&](const auto& pair) { return pair.second->login.has_value(); });

  auto automatic_choice_it =
      base::ranges::find_if(login_details_map_, [&](const auto& pair) {
        return pair.second->choose_automatically_if_no_stored_login;
      });

  // Special case: if the login choice can be made implicitly (there are no PWM
  // logins and there is a |choose_automatically_if_no_stored_login| choice),
  // the section will not be shown.
  if (automatic_choice_it != login_details_map_.end() &&
      !has_password_manager_logins) {
    delegate_->GetUserModel()->SetSelectedLoginChoiceByIdentifier(
        automatic_choice_it->first, *collect_user_data_options_, user_data);

    // If only the login section is requested and the choice has already been
    // made implicitly, the entire UI will not be shown and the action will
    // complete immediately.
    if (OnlyLoginRequested(*collect_user_data_options_)) {
      std::move(collect_user_data_options_->confirm_callback)
          .Run(user_data, nullptr);
      return;
    }
    if (collect_user_data_options_->login_choices.size() == 1) {
      // The login section does not offer a meaningful choice, but there is at
      // least one other section being shown. Hide the logins, show the rest.
      collect_user_data_options_->request_login_choice = false;
    }
  } else if (!collect_user_data_options_->login_choices.empty()) {
    base::ranges::stable_sort(collect_user_data_options_->login_choices,
                              LoginChoice::CompareByPriority);
    delegate_->GetUserModel()->SetSelectedLoginChoice(
        std::make_unique<LoginChoice>(
            collect_user_data_options_->login_choices[0]),
        user_data);
  }

  // Clear previously selected info, if requested.
  if (proto_.collect_user_data().clear_previous_credit_card_selection()) {
    delegate_->GetUserModel()->SetSelectedCreditCard(/* card= */ nullptr,
                                                     user_data);
  }
  if (proto_.collect_user_data().clear_previous_login_selection()) {
    user_data->selected_login_.reset();
  }
  for (const auto& profile_name :
       proto_.collect_user_data().clear_previous_profile_selection()) {
    delegate_->GetUserModel()->SetSelectedAutofillProfile(
        profile_name, /* profile= */ nullptr, user_data);
  }

  if (proto_.collect_user_data().has_user_data()) {
    UpdateUserDataFromProto(proto_.collect_user_data().user_data(), user_data);
  } else {
    delegate_->GetPersonalDataManager()->AddObserver(this);
    UpdatePersonalDataManagerProfiles(user_data);
    UpdatePersonalDataManagerCards(user_data);
  }
  UpdateDateTimeRangeStart(user_data);
  UpdateDateTimeRangeEnd(user_data);

  UpdateMetrics(user_data);

  if (collect_user_data.has_prompt()) {
    delegate_->SetStatusMessage(collect_user_data.prompt());
  }
  delegate_->Prompt(/* user_actions = */ nullptr,
                    /* disable_force_expand_sheet = */ false);
  delegate_->CollectUserData(collect_user_data_options_.get());
}

void CollectUserDataAction::UpdateMetrics(UserData* user_data) {
  DCHECK(user_data);
  if (!shown_to_user_) {
    shown_to_user_ = true;
    if (user_data->previous_user_data_metrics_) {
      // Restore metrics data from a previous run that was interrupted by
      // reloading the user data.
      metrics_data_ = *user_data->previous_user_data_metrics_;
    } else {
      metrics_data_.source_id =
          ukm::GetSourceIdForWebContentsDocument(delegate_->GetWebContents());
      FillInitialDataStateForMetrics(user_data->available_contacts_,
                                     user_data->available_addresses_,
                                     user_data->available_payment_instruments_);
      FillInitiallySelectedDataStateForMetrics(user_data);
    }
  }
  user_data->previous_user_data_metrics_.reset();
}

void CollectUserDataAction::OnGetUserData(
    const CollectUserDataProto& collect_user_data,
    UserData* user_data,
    const UserModel* user_model) {
  if (HasActionEnded()) {
    return;
  }
  action_stopwatch_.StartActiveTime();
  delegate_->GetPersonalDataManager()->RemoveObserver(this);

  WriteProcessedAction(user_data, user_model);
  if (collect_user_data_options_->should_store_data_changes) {
    UpdateProfileAndCardUse(user_data);
  }
  DCHECK(
      IsUserDataComplete(*user_data, *user_model, *collect_user_data_options_));
  EndAction(ClientStatus(ACTION_APPLIED));
}

void CollectUserDataAction::OnAdditionalActionTriggered(
    int index,
    UserData* user_data,
    const UserModel* user_model) {
  if (HasActionEnded()) {
    return;
  }
  action_stopwatch_.StartActiveTime();
  delegate_->GetPersonalDataManager()->RemoveObserver(this);

  processed_action_proto_->mutable_collect_user_data_result()
      ->set_additional_action_index(index);
  WriteProcessedAction(user_data, user_model);
  EndAction(ClientStatus(ACTION_APPLIED));
}

void CollectUserDataAction::OnTermsAndConditionsLinkClicked(
    int link,
    UserData* user_data,
    const UserModel* user_model) {
  if (HasActionEnded()) {
    return;
  }
  action_stopwatch_.StartActiveTime();
  delegate_->GetPersonalDataManager()->RemoveObserver(this);

  processed_action_proto_->mutable_collect_user_data_result()->set_terms_link(
      link);
  WriteProcessedAction(user_data, user_model);
  EndAction(ClientStatus(ACTION_APPLIED));
}

void CollectUserDataAction::ReloadAction(UserData* user_data) {
  if (HasActionEnded()) {
    return;
  }
  action_stopwatch_.StartActiveTime();
  delegate_->GetPersonalDataManager()->RemoveObserver(this);

  metrics_data_.personal_data_changed = true;
  user_data->previous_user_data_metrics_ = metrics_data_;
  // We do not wish to log this yet.
  metrics_data_.metrics_logged = true;
  EndAction(ClientStatus(RESEND_USER_DATA));
}

void CollectUserDataAction::OnSelectionStateChanged(
    UserDataEventField field,
    UserDataEventType event_type) {
  switch (field) {
    case CONTACT_EVENT:
      metrics_data_.contact_selection_state = user_data::GetNewSelectionState(
          metrics_data_.contact_selection_state, event_type);
      break;
    case CREDIT_CARD_EVENT:
      metrics_data_.credit_card_selection_state =
          user_data::GetNewSelectionState(
              metrics_data_.credit_card_selection_state, event_type);
      break;
    case SHIPPING_EVENT:
      metrics_data_.shipping_selection_state = user_data::GetNewSelectionState(
          metrics_data_.shipping_selection_state, event_type);
      break;
  }
}

bool CollectUserDataAction::CreateOptionsFromProto() {
  DCHECK(collect_user_data_options_ == nullptr);
  collect_user_data_options_ = std::make_unique<CollectUserDataOptions>();
  auto collect_user_data = proto_.collect_user_data();

  if (collect_user_data.has_contact_details()) {
    auto contact_details = collect_user_data.contact_details();
    collect_user_data_options_->request_payer_email =
        contact_details.request_payer_email();
    collect_user_data_options_->request_payer_name =
        contact_details.request_payer_name();
    collect_user_data_options_->request_payer_phone =
        contact_details.request_payer_phone();
    collect_user_data_options_->required_contact_data_pieces =
        std::vector<RequiredDataPiece>(
            contact_details.required_data_piece().begin(),
            contact_details.required_data_piece().end());
    // TODO(b/146405276): Remove legacy support for |summary_fields| and
    // |full_fields|.
    if (contact_details.summary_fields().empty()) {
      collect_user_data_options_->contact_summary_max_lines =
          kDefaultMaxNumberContactSummaryLines;
      collect_user_data_options_->contact_summary_fields.assign(
          kDefaultContactSummaryFields.begin(),
          kDefaultContactSummaryFields.end());
    } else {
      for (const auto& field : contact_details.summary_fields()) {
        collect_user_data_options_->contact_summary_fields.emplace_back(
            (AutofillContactField)field);
      }
      if (contact_details.max_number_summary_lines() <= 0) {
        VLOG(1) << "max_number_summary_lines must be > 0";
        return false;
      }
      collect_user_data_options_->contact_summary_max_lines =
          contact_details.max_number_summary_lines();
    }
    if (contact_details.full_fields().empty()) {
      collect_user_data_options_->contact_full_max_lines =
          kDefaultMaxNumberContactFullLines;
      collect_user_data_options_->contact_full_fields.assign(
          kDefaultContactFullFields.begin(), kDefaultContactFullFields.end());
    } else {
      for (const auto& field : contact_details.full_fields()) {
        collect_user_data_options_->contact_full_fields.emplace_back(
            (AutofillContactField)field);
      }
      if (contact_details.max_number_full_lines() <= 0) {
        VLOG(1) << "max_number_full_lines must be > 0";
        return false;
      }
      collect_user_data_options_->contact_full_max_lines =
          contact_details.max_number_full_lines();
    }
    if (RequiresContact(*collect_user_data_options_)) {
      if (!contact_details.has_contact_details_name()) {
        VLOG(1) << "Contact details name missing";
        return false;
      }
    }

    collect_user_data_options_->contact_details_name =
        contact_details.contact_details_name();

    collect_user_data_options_->contact_details_section_title.assign(
        contact_details.contact_details_section_title().empty()
            ? l10n_util::GetStringUTF8(IDS_PAYMENTS_CONTACT_DETAILS_LABEL)
            : contact_details.contact_details_section_title());
  }

  for (const auto& network :
       collect_user_data.supported_basic_card_networks()) {
    if (!autofill::data_util::IsValidBasicCardIssuerNetwork(network)) {
      VLOG(1) << "Invalid basic card network: " << network;
      return false;
    }
  }
  std::copy(collect_user_data.supported_basic_card_networks().begin(),
            collect_user_data.supported_basic_card_networks().end(),
            std::back_inserter(
                collect_user_data_options_->supported_basic_card_networks));
  collect_user_data_options_->request_payment_method =
      collect_user_data.request_payment_method();
  collect_user_data_options_->billing_address_name =
      collect_user_data.billing_address_name();
  if (collect_user_data_options_->request_payment_method &&
      collect_user_data_options_->billing_address_name.empty()) {
    VLOG(1) << "Required payment method without address name";
    return false;
  }
  collect_user_data_options_->credit_card_expired_text =
      collect_user_data.credit_card_expired_text();
  // TODO(b/146195295): Remove fallback and enforce non-empty backend string.
  if (collect_user_data_options_->credit_card_expired_text.empty()) {
    collect_user_data_options_->credit_card_expired_text =
        l10n_util::GetStringUTF8(
            IDS_PAYMENTS_VALIDATION_INVALID_CREDIT_CARD_EXPIRED);
  }
  if (collect_user_data_options_->request_payment_method) {
    collect_user_data_options_->required_credit_card_data_pieces =
        std::vector<RequiredDataPiece>(
            collect_user_data.required_credit_card_data_piece().begin(),
            collect_user_data.required_credit_card_data_piece().end());
    collect_user_data_options_->required_billing_address_data_pieces =
        std::vector<RequiredDataPiece>(
            collect_user_data.required_billing_address_data_piece().begin(),
            collect_user_data.required_billing_address_data_piece().end());
  }

  collect_user_data_options_->shipping_address_name =
      collect_user_data.shipping_address_name();
  collect_user_data_options_->request_shipping =
      !collect_user_data.shipping_address_name().empty();
  if (collect_user_data_options_->request_shipping) {
    collect_user_data_options_->required_shipping_address_data_pieces =
        std::vector<RequiredDataPiece>(
            collect_user_data.required_shipping_address_data_piece().begin(),
            collect_user_data.required_shipping_address_data_piece().end());
  }

  collect_user_data_options_->should_store_data_changes =
      !delegate_->GetWebContents()->GetBrowserContext()->IsOffTheRecord() &&
      !collect_user_data.has_user_data();
  collect_user_data_options_->can_edit_contacts =
      !collect_user_data.has_user_data();

  collect_user_data_options_->request_login_choice =
      collect_user_data.has_login_details();
  collect_user_data_options_->login_section_title.assign(
      collect_user_data.login_details().section_title());
  collect_user_data_options_->shipping_address_section_title.assign(
      collect_user_data.shipping_address_section_title().empty()
          ? l10n_util::GetStringUTF8(IDS_PAYMENTS_SHIPPING_ADDRESS_LABEL)
          : collect_user_data.shipping_address_section_title());

  // Transform login options to concrete login choices.
  for (const auto& login_option :
       collect_user_data.login_details().login_options()) {
    switch (login_option.type_case()) {
      case LoginDetailsProto::LoginOptionProto::kCustom: {
        const std::string identifier = base::NumberToString(
            collect_user_data_options_->login_choices.size());
        LoginChoice choice = {
            identifier,
            login_option.custom().label(),
            login_option.sublabel(),
            login_option.has_sublabel_accessibility_hint()
                ? absl::make_optional(
                      login_option.sublabel_accessibility_hint())
                : absl::nullopt,
            login_option.has_preselection_priority()
                ? login_option.preselection_priority()
                : -1,
            login_option.has_info_popup()
                ? absl::make_optional(login_option.info_popup())
                : absl::nullopt,
            login_option.has_edit_button_content_description()
                ? absl::make_optional(
                      login_option.edit_button_content_description())
                : absl::nullopt};
        collect_user_data_options_->login_choices.emplace_back(
            std::move(choice));
        login_details_map_.emplace(
            identifier,
            std::make_unique<LoginDetails>(
                login_option.choose_automatically_if_no_stored_login(),
                login_option.payload()));
        break;
      }
      case LoginDetailsProto::LoginOptionProto::kPasswordManager: {
        // Will be retrieved later.
        break;
      }
      case LoginDetailsProto::LoginOptionProto::TYPE_NOT_SET: {
        // Login option specified, but type not set (should never happen).
        VLOG(1) << "LoginDetailsProto::LoginOptionProto::TYPE_NOT_SET";
        return false;
      }
    }
  }

  if (collect_user_data.has_date_time_range()) {
    std::string error_message;
    if (!IsValidDateTimeRangeProto(collect_user_data.date_time_range(),
                                   &error_message)) {
      VLOG(1) << "Invalid action: " << error_message;
      return false;
    }
    if (collect_user_data.date_time_range().start_time_slot() < 0 ||
        collect_user_data.date_time_range().end_time_slot() < 0 ||
        collect_user_data.date_time_range().start_time_slot() >=
            collect_user_data.date_time_range().time_slots().size() ||
        collect_user_data.date_time_range().end_time_slot() >=
            collect_user_data.date_time_range().time_slots().size()) {
      VLOG(1) << "Invalid action: time slot index out of range";
      return false;
    }
    collect_user_data_options_->request_date_time_range = true;
    collect_user_data_options_->date_time_range =
        collect_user_data.date_time_range();
  }

  for (const auto& section :
       collect_user_data.additional_prepended_sections()) {
    if (!IsValidUserFormSection(section)) {
      VLOG(1)
          << "Invalid UserFormSectionProto in additional_prepended_sections";
      return false;
    }
    collect_user_data_options_->additional_prepended_sections.emplace_back(
        section);
  }
  for (const auto& section : collect_user_data.additional_appended_sections()) {
    if (!IsValidUserFormSection(section)) {
      VLOG(1) << "Invalid UserFormSectionProto in additional_appended_sections";
      return false;
    }
    collect_user_data_options_->additional_appended_sections.emplace_back(
        section);
  }

  if (collect_user_data.has_info_section_text() &&
      collect_user_data.info_section_text().empty()) {
    VLOG(1) << "Info text set but empty.";
    return false;
  }

  if (collect_user_data.has_generic_user_interface_prepended()) {
    collect_user_data_options_->generic_user_interface_prepended =
        collect_user_data.generic_user_interface_prepended();
  }
  if (collect_user_data.has_generic_user_interface_appended()) {
    collect_user_data_options_->generic_user_interface_appended =
        collect_user_data.generic_user_interface_appended();
  }
  if (collect_user_data.has_additional_model_identifier_to_check()) {
    collect_user_data_options_->additional_model_identifier_to_check =
        collect_user_data.additional_model_identifier_to_check();
  }

  auto* confirm_chip =
      collect_user_data_options_->confirm_action.mutable_chip();
  if (collect_user_data.has_confirm_chip()) {
    *confirm_chip = collect_user_data.confirm_chip();
  } else {
    confirm_chip->set_text(
        l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_PAYMENT_INFO_CONFIRM));
    confirm_chip->set_type(HIGHLIGHTED_ACTION);
  }

  for (auto action : collect_user_data.additional_actions()) {
    collect_user_data_options_->additional_actions.push_back(action);
  }

  if (collect_user_data.request_terms_and_conditions()) {
    collect_user_data_options_->show_terms_as_checkbox =
        collect_user_data.show_terms_as_checkbox();

    if (collect_user_data.accept_terms_and_conditions_text().empty()) {
      VLOG(1) << "Required terms and conditions without text";
      return false;
    }
    collect_user_data_options_->accept_terms_and_conditions_text =
        collect_user_data.accept_terms_and_conditions_text();

    if (!collect_user_data.show_terms_as_checkbox() &&
        collect_user_data.terms_require_review_text().empty()) {
      VLOG(1) << "Required terms review without text";
      return false;
    }
    collect_user_data_options_->terms_require_review_text =
        collect_user_data.terms_require_review_text();
  }

  if (collect_user_data.has_info_section_text()) {
    collect_user_data_options_->info_section_text =
        collect_user_data.info_section_text();
    collect_user_data_options_->info_section_text_center =
        collect_user_data.info_section_text_center();
  }

  collect_user_data_options_->privacy_notice_text =
      collect_user_data.privacy_notice_text();

  collect_user_data_options_->default_email =
      delegate_->GetEmailAddressForAccessTokenAccount();

  return true;
}

void CollectUserDataAction::FillInitialDataStateForMetrics(
    const std::vector<std::unique_ptr<Contact>>& contacts,
    const std::vector<std::unique_ptr<Address>>& addresses,
    const std::vector<std::unique_ptr<PaymentInstrument>>&
        payment_instruments) {
  DCHECK(collect_user_data_options_ != nullptr);
  metrics_data_.initially_prefilled = true;

  if (RequiresContact(*collect_user_data_options_)) {
    int complete_count =
        base::ranges::count_if(contacts, [this](const auto& contact) {
          return user_data::GetContactValidationErrors(
                     contact->profile.get(), *collect_user_data_options_)
              .empty();
        });
    metrics_data_.complete_contacts_initial_count = complete_count;
    metrics_data_.incomplete_contacts_initial_count =
        contacts.size() - complete_count;

    if (complete_count == 0) {
      metrics_data_.initially_prefilled = false;
    }
  }

  if (RequiresShipping(*collect_user_data_options_)) {
    int complete_count =
        base::ranges::count_if(addresses, [this](const auto& address) {
          return user_data::GetShippingAddressValidationErrors(
                     address->profile.get(), *collect_user_data_options_)
              .empty();
        });
    metrics_data_.complete_shipping_addresses_initial_count = complete_count;
    metrics_data_.incomplete_shipping_addresses_initial_count =
        addresses.size() - complete_count;

    if (complete_count == 0) {
      metrics_data_.initially_prefilled = false;
    }
  }

  if (RequiresPaymentMethod(*collect_user_data_options_)) {
    int complete_count = base::ranges::count_if(
        payment_instruments, [this](const auto& payment_instrument) {
          return user_data::GetPaymentInstrumentValidationErrors(
                     payment_instrument->card.get(),
                     payment_instrument->billing_address.get(),
                     *collect_user_data_options_)
              .empty();
        });
    metrics_data_.complete_credit_cards_initial_count = complete_count;
    metrics_data_.incomplete_credit_cards_initial_count =
        payment_instruments.size() - complete_count;

    if (complete_count == 0) {
      metrics_data_.initially_prefilled = false;
    }
  }
}

void CollectUserDataAction::FillInitiallySelectedDataStateForMetrics(
    UserData* user_data) {
  DCHECK(collect_user_data_options_);
  DCHECK(user_data);

  if (RequiresContact(*collect_user_data_options_)) {
    metrics_data_.selected_contact_field_bitmask =
        user_data::GetFieldBitArrayForAddress(user_data->selected_address(
            collect_user_data_options_->contact_details_name));
  }

  if (RequiresShipping(*collect_user_data_options_)) {
    metrics_data_.selected_shipping_address_field_bitmask =
        user_data::GetFieldBitArrayForAddress(user_data->selected_address(
            collect_user_data_options_->shipping_address_name));
  }

  if (RequiresPaymentMethod(*collect_user_data_options_)) {
    metrics_data_.selected_credit_card_field_bitmask =
        user_data::GetFieldBitArrayForCreditCard(user_data->selected_card());
    metrics_data_.selected_billing_address_field_bitmask =
        user_data::GetFieldBitArrayForAddress(user_data->selected_address(
            collect_user_data_options_->billing_address_name));
  }
}

// TODO(b/148448649): Move to dedicated helper namespace.
// static
bool CollectUserDataAction::IsUserDataComplete(
    const UserData& user_data,
    const UserModel& user_model,
    const CollectUserDataOptions& options) {
  auto* selected_profile =
      user_data.selected_address(options.contact_details_name);
  auto* billing_address =
      user_data.selected_address(options.billing_address_name);
  auto* shipping_address =
      user_data.selected_address(options.shipping_address_name);
  return user_data::GetContactValidationErrors(selected_profile, options)
             .empty() &&
         user_data::GetShippingAddressValidationErrors(shipping_address,
                                                       options)
             .empty() &&
         user_data::GetPaymentInstrumentValidationErrors(
             user_data.selected_card(), billing_address, options)
             .empty() &&
         IsValidLoginChoice(user_data.selected_login_choice(), options) &&
         IsValidTermsChoice(user_data.terms_and_conditions_, options) &&
         IsValidDateTimeRange(user_data.date_time_range_start_date_,
                              user_data.date_time_range_start_timeslot_,
                              user_data.date_time_range_end_date_,
                              user_data.date_time_range_end_timeslot_,
                              options) &&
         AreAdditionalSectionsComplete(user_data, options) &&
         IsValidUserModel(user_model, options);
}

// TODO(b/148448649): Move to dedicated helper namespace.
// static
int CollectUserDataAction::CompareDates(const DateProto& first,
                                        const DateProto& second) {
  auto first_tuple = std::make_tuple(first.year(), first.month(), first.day());
  auto second_tuple =
      std::make_tuple(second.year(), second.month(), second.day());
  if (first_tuple < second_tuple) {
    return -1;
  } else if (second_tuple < first_tuple) {
    return 1;
  }
  return 0;
}

// TODO(b/148448649): Move to dedicated helper namespace.
// static
bool CollectUserDataAction::SanitizeDateTimeRange(
    absl::optional<DateProto>* start_date,
    absl::optional<int>* start_timeslot,
    absl::optional<DateProto>* end_date,
    absl::optional<int>* end_timeslot,
    const CollectUserDataOptions& collect_user_data_options,
    bool change_start) {
  if (!collect_user_data_options.request_date_time_range) {
    return false;
  }
  DCHECK(start_date);
  DCHECK(start_timeslot);
  DCHECK(end_date);
  DCHECK(end_timeslot);
  if (!start_date->has_value() || !end_date->has_value()) {
    return false;
  }

  auto date_comparison = CompareDates(**start_date, **end_date);
  if (date_comparison < 0) {
    return false;
  }

  // Start date > end date, reset date.
  if (date_comparison > 0) {
    if (change_start) {
      start_date->reset();
    } else {
      end_date->reset();
    }
    return true;
  }

  if (!start_timeslot->has_value() || !end_timeslot->has_value()) {
    return false;
  }

  DCHECK(**start_timeslot >= 0 &&
         **start_timeslot <
             collect_user_data_options.date_time_range.time_slots().size());
  DCHECK(**end_timeslot >= 0 &&
         **end_timeslot <
             collect_user_data_options.date_time_range.time_slots().size());
  auto start_time =
      collect_user_data_options.date_time_range.time_slots(**start_timeslot);
  auto end_time =
      collect_user_data_options.date_time_range.time_slots(**end_timeslot);
  auto time_comparison =
      start_time.comparison_value() - end_time.comparison_value();
  if (time_comparison < 0) {
    return false;
  }

  // Start date == end date and start time >= end time, reset time.
  if (time_comparison >= 0) {
    if (change_start) {
      start_timeslot->reset();
    } else {
      end_timeslot->reset();
    }
    return true;
  }

  NOTREACHED();
  return false;
}

void CollectUserDataAction::WriteProcessedAction(UserData* user_data,
                                                 const UserModel* user_model) {
  if (proto().collect_user_data().request_payment_method() &&
      user_data->selected_card()) {
    std::string card_issuer_network =
        autofill::data_util::GetPaymentRequestData(
            user_data->selected_card()->network())
            .basic_card_issuer_network;
    processed_action_proto_->mutable_collect_user_data_result()
        ->set_card_issuer_network(card_issuer_network);
  }

  if (proto().collect_user_data().has_contact_details()) {
    auto contact_details_proto = proto().collect_user_data().contact_details();
    auto* selected_profile = user_data->selected_address(
        contact_details_proto.contact_details_name());

    if (selected_profile != nullptr) {
      if (contact_details_proto.request_payer_name()) {
        Metrics::RecordPaymentRequestFirstNameOnly(
            selected_profile->GetRawInfo(autofill::NAME_LAST).empty());
      }

      if (contact_details_proto.request_payer_email()) {
        processed_action_proto_->mutable_collect_user_data_result()
            ->set_payer_email(base::UTF16ToUTF8(
                selected_profile->GetRawInfo(autofill::EMAIL_ADDRESS)));
      }
    }
  }

  if (proto().collect_user_data().has_login_details() &&
      user_data->selected_login_choice() != nullptr) {
    auto login_details =
        login_details_map_.find(user_data->selected_login_choice()->identifier);
    if (login_details != login_details_map_.end()) {
      if (login_details->second->login.has_value()) {
        user_data->selected_login_ = *login_details->second->login;
        if (login_details->second->login->username.empty()) {
          processed_action_proto_->mutable_collect_user_data_result()
              ->set_login_missing_username(true);
        }
      }

      processed_action_proto_->mutable_collect_user_data_result()
          ->set_login_payload(login_details->second->payload);
    }
  }

  if (proto().collect_user_data().has_date_time_range()) {
    if (user_data->date_time_range_start_date_.has_value()) {
      *processed_action_proto_->mutable_collect_user_data_result()
           ->mutable_date_range_start_date() =
          *user_data->date_time_range_start_date_;
    }
    if (user_data->date_time_range_start_timeslot_.has_value()) {
      processed_action_proto_->mutable_collect_user_data_result()
          ->set_date_range_start_timeslot(
              *user_data->date_time_range_start_timeslot_);
    }
    if (user_data->date_time_range_end_date_.has_value()) {
      *processed_action_proto_->mutable_collect_user_data_result()
           ->mutable_date_range_end_date() =
          *user_data->date_time_range_end_date_;
    }
    if (user_data->date_time_range_end_timeslot_.has_value()) {
      processed_action_proto_->mutable_collect_user_data_result()
          ->set_date_range_end_timeslot(
              *user_data->date_time_range_end_timeslot_);
    }
  }
  for (const auto& section :
       proto().collect_user_data().additional_prepended_sections()) {
    FillProtoForAdditionalSection(section, *user_data,
                                  processed_action_proto_.get());
  }
  for (const auto& section :
       proto().collect_user_data().additional_appended_sections()) {
    FillProtoForAdditionalSection(section, *user_data,
                                  processed_action_proto_.get());
  }

  processed_action_proto_->mutable_collect_user_data_result()
      ->set_is_terms_and_conditions_accepted(user_data->terms_and_conditions_ ==
                                             TermsAndConditionsState::ACCEPTED);
  if (user_model != nullptr &&
      (proto().collect_user_data().has_generic_user_interface_prepended() ||
       proto().collect_user_data().has_generic_user_interface_appended())) {
    // Build the union of both models (this assumes that there are no
    // overlapping model keys).
    *processed_action_proto_->mutable_collect_user_data_result()
         ->mutable_model() = MergeModelProtos(
        proto().collect_user_data().generic_user_interface_prepended().model(),
        proto().collect_user_data().generic_user_interface_appended().model());
    user_model->UpdateProto(
        processed_action_proto_->mutable_collect_user_data_result()
            ->mutable_model());
  }
  processed_action_proto_->mutable_collect_user_data_result()
      ->set_shown_to_user(shown_to_user_);
}

void CollectUserDataAction::UpdateProfileAndCardUse(UserData* user_data) {
  base::flat_map<std::string, const autofill::AutofillProfile*> profiles_used;
  if (proto().collect_user_data().has_contact_details()) {
    auto contact_details_proto = proto().collect_user_data().contact_details();
    auto* selected_contact_profile = user_data->selected_address(
        contact_details_proto.contact_details_name());
    if (selected_contact_profile != nullptr) {
      profiles_used.emplace(selected_contact_profile->guid(),
                            selected_contact_profile);
    }
  }
  if (!proto().collect_user_data().shipping_address_name().empty()) {
    auto* selected_shipping_address = user_data->selected_address(
        proto().collect_user_data().shipping_address_name());
    if (selected_shipping_address != nullptr) {
      profiles_used.emplace(selected_shipping_address->guid(),
                            selected_shipping_address);
    }
  }
  if (!proto().collect_user_data().billing_address_name().empty()) {
    auto* selected_billing_address = user_data->selected_address(
        proto().collect_user_data().billing_address_name());
    if (selected_billing_address != nullptr) {
      profiles_used.emplace(selected_billing_address->guid(),
                            selected_billing_address);
    }
  }
  for (const auto& it : profiles_used) {
    delegate_->GetPersonalDataManager()->RecordUseOf(it.second);
  }
  if (proto().collect_user_data().request_payment_method()) {
    auto* selected_card = user_data->selected_card();
    if (selected_card != nullptr) {
      delegate_->GetPersonalDataManager()->RecordUseOf(selected_card);
    }
  }
}

void CollectUserDataAction::UpdateUserDataFromProto(
    const CollectUserDataProto::UserDataProto& proto_data,
    UserData* user_data) {
  DCHECK(user_data != nullptr);

  if (RequiresContact(*collect_user_data_options_)) {
    user_data->available_contacts_.clear();
    for (const auto& profile_data : proto_data.available_contacts()) {
      auto profile = std::make_unique<autofill::AutofillProfile>();
      AddProtoDataToAutofillDataModel(profile_data.values(),
                                      proto_data.locale(), profile.get());
      profile->FinalizeAfterImport();
      if (!user_data::GetContactValidationErrors(profile.get(),
                                                 *collect_user_data_options_)
               .empty()) {
        continue;
      }
      auto contact = std::make_unique<Contact>(std::move(profile));
      if (profile_data.has_identifier()) {
        contact->identifier = profile_data.identifier();
      }
      user_data->available_contacts_.emplace_back(std::move(contact));
    }
    if (proto_data.has_selected_contact_identifier()) {
      const auto& it = base::ranges::find_if(
          user_data->available_contacts_, [&](const auto& contact) {
            return proto_data.selected_contact_identifier() ==
                   contact->identifier.value_or(std::string());
          });
      if (it == user_data->available_contacts_.end()) {
        NOTREACHED();
        EndAction(ClientStatus(INVALID_ACTION));
        return;
      }
      const auto& contact_to_select = *it;
      delegate_->GetUserModel()->SetSelectedAutofillProfile(
          collect_user_data_options_->contact_details_name,
          user_data::MakeUniqueFromProfile(*contact_to_select->profile),
          user_data);
    } else {
      UpdateSelectedContact(user_data);
    }
  }

  if (RequiresAddress(*collect_user_data_options_)) {
    user_data->available_addresses_.clear();
    for (const auto& profile_data : proto_data.available_addresses()) {
      auto profile = std::make_unique<autofill::AutofillProfile>();
      AddProtoDataToAutofillDataModel(profile_data.values(),
                                      proto_data.locale(), profile.get());
      profile->FinalizeAfterImport();
      auto address = std::make_unique<Address>(std::move(profile));
      if (profile_data.has_identifier()) {
        address->identifier = profile_data.identifier();
      }
      user_data->available_addresses_.emplace_back(std::move(address));
    }
    if (proto_data.has_selected_shipping_address_identifier()) {
      const auto& it = base::ranges::find_if(
          user_data->available_addresses_, [&](const auto& address) {
            return address->identifier &&
                   proto_data.selected_shipping_address_identifier() ==
                       *address->identifier;
          });
      if (it == user_data->available_addresses_.end()) {
        NOTREACHED();
        EndAction(ClientStatus(INVALID_ACTION));
        return;
      }
      const auto& address_to_select = *it;
      delegate_->GetUserModel()->SetSelectedAutofillProfile(
          collect_user_data_options_->shipping_address_name,
          user_data::MakeUniqueFromProfile(*address_to_select->profile),
          user_data);
    } else {
      UpdateSelectedShippingAddress(user_data);
    }
  }

  if (RequiresPaymentMethod(*collect_user_data_options_)) {
    user_data->available_payment_instruments_.clear();
    for (const auto& payment_data :
         proto_data.available_payment_instruments()) {
      auto credit_card = std::make_unique<autofill::CreditCard>();
      credit_card->set_record_type(autofill::CreditCard::MASKED_SERVER_CARD);
      AddProtoDataToAutofillDataModel(payment_data.card_values(),
                                      proto_data.locale(), credit_card.get());
      if (!payment_data.network().empty()) {
        credit_card->SetNetworkForMaskedCard(payment_data.network());
      }
      // Note: If the incoming card did not set a network GetPaymentRequestData
      // will fall back to "generic".
      if (!collect_user_data_options_->supported_basic_card_networks.empty() &&
          std::find(
              collect_user_data_options_->supported_basic_card_networks.begin(),
              collect_user_data_options_->supported_basic_card_networks.end(),
              autofill::data_util::GetPaymentRequestData(credit_card->network())
                  .basic_card_issuer_network) ==
              collect_user_data_options_->supported_basic_card_networks.end()) {
        continue;
      }

      auto payment_instrument = std::make_unique<PaymentInstrument>();
      payment_instrument->card = std::move(credit_card);

      if (!payment_data.address_values().empty()) {
        auto profile = std::make_unique<autofill::AutofillProfile>();
        AddProtoDataToAutofillDataModel(payment_data.address_values(),
                                        proto_data.locale(), profile.get());
        profile->FinalizeAfterImport();
        payment_instrument->billing_address = std::move(profile);
      }

      if (payment_data.has_identifier()) {
        payment_instrument->identifier = payment_data.identifier();
      }

      user_data->available_payment_instruments_.emplace_back(
          std::move(payment_instrument));
    }
    if (proto_data.has_selected_payment_instrument_identifier()) {
      const auto& it = base::ranges::find_if(
          user_data->available_payment_instruments_,
          [&](const auto& instrument) {
            return instrument->identifier &&
                   proto_data.selected_payment_instrument_identifier() ==
                       *instrument->identifier;
          });
      if (it == user_data->available_payment_instruments_.end()) {
        NOTREACHED();
        EndAction(ClientStatus(INVALID_ACTION));
        return;
      }
      const auto& instrument_to_select = *it;
      delegate_->GetUserModel()->SetSelectedCreditCard(
          std::make_unique<autofill::CreditCard>(*instrument_to_select->card),
          user_data);
      if (instrument_to_select->billing_address) {
        delegate_->GetUserModel()->SetSelectedAutofillProfile(
            collect_user_data_options_->billing_address_name,
            user_data::MakeUniqueFromProfile(
                *instrument_to_select->billing_address),
            user_data);
      }
    } else {
      UpdateSelectedCreditCard(user_data);
    }
  }
}

void CollectUserDataAction::UpdatePersonalDataManagerProfiles(
    UserData* user_data,
    UserData::FieldChange* field_change) {
  bool requires_contact = RequiresContact(*collect_user_data_options_);
  bool requires_address = RequiresAddress(*collect_user_data_options_);
  if (!requires_contact && !requires_address) {
    return;
  }
  DCHECK(user_data != nullptr);

  user_data->available_contacts_.clear();
  user_data->available_addresses_.clear();
  for (const auto* profile :
       delegate_->GetPersonalDataManager()->GetProfilesToSuggest()) {
    if (requires_contact) {
      user_data->available_contacts_.emplace_back(std::make_unique<Contact>(
          user_data::MakeUniqueFromProfile(*profile)));
    }
    if (requires_address) {
      user_data->available_addresses_.emplace_back(std::make_unique<Address>(
          user_data::MakeUniqueFromProfile(*profile)));
    }
  }
  UpdateSelectedContact(user_data);
  UpdateSelectedShippingAddress(user_data);

  if (field_change != nullptr) {
    *field_change = UserData::FieldChange::AVAILABLE_PROFILES;
  }
}

void CollectUserDataAction::UpdatePersonalDataManagerCards(
    UserData* user_data,
    UserData::FieldChange* field_change) {
  if (!RequiresPaymentMethod(*collect_user_data_options_)) {
    return;
  }
  DCHECK(user_data != nullptr);

  user_data->available_payment_instruments_.clear();
  for (const auto* card :
       delegate_->GetPersonalDataManager()->GetCreditCardsToSuggest(
           /* include_server_cards= */ true)) {
    if (!collect_user_data_options_->supported_basic_card_networks.empty() &&
        std::find(
            collect_user_data_options_->supported_basic_card_networks.begin(),
            collect_user_data_options_->supported_basic_card_networks.end(),
            autofill::data_util::GetPaymentRequestData(card->network())
                .basic_card_issuer_network) ==
            collect_user_data_options_->supported_basic_card_networks.end()) {
      continue;
    }

    auto payment_instrument = std::make_unique<PaymentInstrument>();
    payment_instrument->card = std::make_unique<autofill::CreditCard>(*card);

    if (!card->billing_address_id().empty()) {
      auto* billing_address =
          delegate_->GetPersonalDataManager()->GetProfileByGUID(
              card->billing_address_id());
      if (billing_address != nullptr) {
        payment_instrument->billing_address =
            user_data::MakeUniqueFromProfile(*billing_address);
      }
    }
    user_data->available_payment_instruments_.emplace_back(
        std::move(payment_instrument));
  }
  UpdateSelectedCreditCard(user_data);

  if (field_change != nullptr) {
    *field_change = UserData::FieldChange::AVAILABLE_PAYMENT_INSTRUMENTS;
  }
}

void CollectUserDataAction::UpdateSelectedContact(UserData* user_data) {
  DCHECK(user_data != nullptr);

  bool found_contact = false;
  auto* selected_contact = user_data->selected_address(
      collect_user_data_options_->contact_details_name);
  if (selected_contact != nullptr) {
    found_contact = base::ranges::any_of(
        user_data->available_contacts_,
        [&selected_contact, this](const std::unique_ptr<Contact>& contact) {
          return user_data::CompareContactDetails(*collect_user_data_options_,
                                                  contact->profile.get(),
                                                  selected_contact);
        });
  }

  if (!found_contact && selected_contact != nullptr) {
    delegate_->GetUserModel()->SetSelectedAutofillProfile(
        collect_user_data_options_->contact_details_name,
        /* profile= */ nullptr, user_data);
  }

  if (!user_data->has_selected_address(
          collect_user_data_options_->contact_details_name) &&
      RequiresContact(*collect_user_data_options_)) {
    int default_selection = user_data::GetDefaultContact(
        *collect_user_data_options_, user_data->available_contacts_);
    if (default_selection != -1) {
      delegate_->GetUserModel()->SetSelectedAutofillProfile(
          collect_user_data_options_->contact_details_name,
          user_data::MakeUniqueFromProfile(
              *user_data->available_contacts_[default_selection]->profile),
          user_data);
    }
  }
}

void CollectUserDataAction::UpdateSelectedShippingAddress(UserData* user_data) {
  DCHECK(user_data != nullptr);

  bool found_shipping_address = false;
  auto* selected_shipping_address = user_data->selected_address(
      collect_user_data_options_->shipping_address_name);
  if (selected_shipping_address != nullptr) {
    found_shipping_address = base::ranges::any_of(
        user_data->available_addresses_,
        [&selected_shipping_address](const std::unique_ptr<Address>& address) {
          return address->profile->Compare(*selected_shipping_address) == 0;
        });
  }

  if (!found_shipping_address && selected_shipping_address != nullptr) {
    delegate_->GetUserModel()->SetSelectedAutofillProfile(
        collect_user_data_options_->shipping_address_name,
        /* profile= */ nullptr, user_data);
  }

  if (!user_data->has_selected_address(
          collect_user_data_options_->shipping_address_name) &&
      RequiresShipping(*collect_user_data_options_)) {
    int default_selection = user_data::GetDefaultShippingAddress(
        *collect_user_data_options_, user_data->available_addresses_);
    if (default_selection != -1) {
      delegate_->GetUserModel()->SetSelectedAutofillProfile(
          collect_user_data_options_->shipping_address_name,
          user_data::MakeUniqueFromProfile(
              *(user_data->available_addresses_[default_selection]->profile)),
          user_data);
    }
  }
}

void CollectUserDataAction::UpdateSelectedCreditCard(UserData* user_data) {
  DCHECK(user_data != nullptr);

  bool found_card = false;
  for (const std::unique_ptr<PaymentInstrument>& payment_instrument :
       user_data->available_payment_instruments_) {
    if (user_data->selected_card() != nullptr &&
        payment_instrument->card->Compare(*user_data->selected_card()) == 0) {
      found_card = true;
    }
  }

  if (!found_card) {
    delegate_->GetUserModel()->SetSelectedCreditCard(/* card= */ nullptr,
                                                     user_data);
    delegate_->GetUserModel()->SetSelectedAutofillProfile(
        collect_user_data_options_->billing_address_name,
        /* profile= */ nullptr, user_data);
  }
  if (user_data->selected_card() == nullptr &&
      collect_user_data_options_->request_payment_method) {
    int default_selection = user_data::GetDefaultPaymentInstrument(
        *collect_user_data_options_, user_data->available_payment_instruments_);
    if (default_selection != -1) {
      const auto& default_payment_instrument =
          user_data->available_payment_instruments_[default_selection];
      delegate_->GetUserModel()->SetSelectedCreditCard(
          std::make_unique<autofill::CreditCard>(
              *default_payment_instrument->card),
          user_data);
      if (default_payment_instrument->billing_address != nullptr) {
        delegate_->GetUserModel()->SetSelectedAutofillProfile(
            collect_user_data_options_->billing_address_name,
            user_data::MakeUniqueFromProfile(
                *default_payment_instrument->billing_address),
            user_data);
      }
    }
  }
}

void CollectUserDataAction::OnPersonalDataChanged() {
  if (HasActionEnded()) {
    return;
  }

  metrics_data_.personal_data_changed = true;
  delegate_->WriteUserData(
      base::BindOnce(&CollectUserDataAction::UpdatePersonalDataManagerProfiles,
                     weak_ptr_factory_.GetWeakPtr()));
  delegate_->WriteUserData(
      base::BindOnce(&CollectUserDataAction::UpdatePersonalDataManagerCards,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CollectUserDataAction::UpdateDateTimeRangeStart(
    UserData* user_data,
    UserData::FieldChange* field_change) {
  DCHECK(user_data != nullptr);
  DCHECK(collect_user_data_options_ != nullptr);

  UserData::FieldChange changed = UserData::FieldChange::NONE;
  if (!user_data->date_time_range_start_date_.has_value()) {
    user_data->date_time_range_start_date_ =
        collect_user_data_options_->date_time_range.start_date();
    changed = UserData::FieldChange::DATE_TIME_RANGE_START;
  }
  if (!user_data->date_time_range_start_timeslot_.has_value()) {
    user_data->date_time_range_start_timeslot_ =
        collect_user_data_options_->date_time_range.start_time_slot();
    changed = UserData::FieldChange::DATE_TIME_RANGE_START;
  }

  if (field_change != nullptr && changed != UserData::FieldChange::NONE) {
    *field_change = changed;
  }
}

void CollectUserDataAction::UpdateDateTimeRangeEnd(
    UserData* user_data,
    UserData::FieldChange* field_change) {
  DCHECK(user_data != nullptr);
  DCHECK(collect_user_data_options_ != nullptr);

  UserData::FieldChange changed = UserData::FieldChange::NONE;
  if (!user_data->date_time_range_end_date_.has_value()) {
    user_data->date_time_range_end_date_ =
        collect_user_data_options_->date_time_range.end_date();
    changed = UserData::FieldChange::DATE_TIME_RANGE_END;
  }
  if (!user_data->date_time_range_end_timeslot_.has_value()) {
    user_data->date_time_range_end_timeslot_ =
        collect_user_data_options_->date_time_range.end_time_slot();
    changed = UserData::FieldChange::DATE_TIME_RANGE_END;
  }

  if (field_change != nullptr && changed != UserData::FieldChange::NONE) {
    *field_change = changed;
  }
}

}  // namespace autofill_assistant
