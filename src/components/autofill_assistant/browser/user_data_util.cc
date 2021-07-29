// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/user_data_util.h"

#include <map>
#include <numeric>

#include "base/callback.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/geo/address_i18n.h"
#include "components/autofill_assistant/browser/action_value.pb.h"
#include "components/autofill_assistant/browser/cud_condition.pb.h"
#include "components/autofill_assistant/browser/field_formatter.h"
#include "components/autofill_assistant/browser/url_utils.h"
#include "components/autofill_assistant/browser/website_login_manager.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/libaddressinput/chromium/addressinput_util.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill_assistant {
namespace {

constexpr char kDefaultLocale[] = "en-US";

template <typename T>
ClientStatus ExtractDataAndFormatAutofillValue(
    const T& autofill_value,
    const ValueExpression& value_expression,
    const UserData* user_data,
    bool quote_meta,
    std::string* out_value) {
  if (value_expression.chunk().empty()) {
    VLOG(1) << "|value_expression| is empty";
    return ClientStatus(INVALID_ACTION);
  }

  std::map<std::string, std::string> data;

  if (autofill_value.has_profile()) {
    const auto& profile = autofill_value.profile();
    if (profile.identifier().empty()) {
      VLOG(1) << "empty |profile.identifier|";
      return ClientStatus(INVALID_ACTION);
    }
    const autofill::AutofillProfile* address =
        user_data->selected_address(profile.identifier());
    if (address == nullptr) {
      VLOG(1) << "Requested unknown address '" << profile.identifier() << "'";
      return ClientStatus(PRECONDITION_FAILED);
    }

    auto address_map =
        field_formatter::CreateAutofillMappings(*address, kDefaultLocale);
    data.insert(address_map.begin(), address_map.end());
  }

  const autofill::CreditCard* card = user_data->selected_card();
  if (card != nullptr) {
    auto card_map =
        field_formatter::CreateAutofillMappings(*card, kDefaultLocale);
    data.insert(card_map.begin(), card_map.end());
  }

  return field_formatter::FormatExpression(value_expression, data, quote_meta,
                                           out_value);
}

void OnGetStoredPassword(
    base::OnceCallback<void(const ClientStatus&, const std::string&)> callback,
    bool success,
    std::string password) {
  if (!success) {
    std::move(callback).Run(ClientStatus(AUTOFILL_INFO_NOT_AVAILABLE),
                            std::string());
    return;
  }
  std::move(callback).Run(OkClientStatus(), password);
}

bool EvaluateCondition(const std::map<std::string, std::string>& data,
                       const RequiredDataPiece::Condition& condition) {
  auto it = data.find(base::NumberToString(condition.key()));
  if (it == data.end()) {
    return false;
  }
  auto value = it->second;
  switch (condition.condition_case()) {
    case RequiredDataPiece::Condition::kNotEmpty:
      return !value.empty();
    case RequiredDataPiece::Condition::kRegexp: {
      re2::RE2::Options options;
      options.set_case_sensitive(
          condition.regexp().text_filter().case_sensitive());
      re2::RE2 regexp(condition.regexp().text_filter().re2(), options);
      return RE2::PartialMatch(value, regexp);
    }
    case RequiredDataPiece::Condition::CONDITION_NOT_SET:
      return false;
  }
}

std::vector<std::string> GetValidationErrors(
    const std::map<std::string, std::string> data,
    const std::vector<RequiredDataPiece> required_data_pieces) {
  std::vector<std::string> errors;

  for (const auto& required_data_piece : required_data_pieces) {
    if (!EvaluateCondition(data, required_data_piece.condition())) {
      errors.push_back(required_data_piece.error_message());
    }
  }
  return errors;
}

// Helper function that compares instances of AutofillProfile by completeness
// in regards to the current options. Full profiles should be ordered before
// empty ones and fall back to compare the profile's last usage.
bool CompletenessCompareContacts(const CollectUserDataOptions& options,
                                 const autofill::AutofillProfile& a,
                                 const autofill::AutofillProfile& b) {
  int incomplete_fields_a =
      GetValidationErrors(
          field_formatter::CreateAutofillMappings(a, kDefaultLocale),
          options.required_contact_data_pieces)
          .size();
  int incomplete_fields_b =
      GetValidationErrors(
          field_formatter::CreateAutofillMappings(b, kDefaultLocale),
          options.required_contact_data_pieces)
          .size();
  if (incomplete_fields_a != incomplete_fields_b) {
    return incomplete_fields_a <= incomplete_fields_b;
  }

  return a.use_date() > b.use_date();
}

int GetAddressEditorCompletenessRating(
    const autofill::AutofillProfile& profile) {
  auto address_data = autofill::i18n::CreateAddressDataFromAutofillProfile(
      profile, kDefaultLocale);
  std::multimap<i18n::addressinput::AddressField,
                i18n::addressinput::AddressProblem>
      problems;
  autofill::addressinput::ValidateRequiredFields(
      *address_data, /* filter= */ nullptr, &problems);
  return problems.size();
}

int CompletenessCompareAddresses(
    const std::vector<RequiredDataPiece>& required_data_pieces,
    const autofill::AutofillProfile& a,
    const autofill::AutofillProfile& b) {
  // Compare by editor completeness first. This is done because the
  // AddressEditor only allows storing addresses it considers complete.
  int incomplete_fields_a = GetAddressEditorCompletenessRating(a);
  int incomplete_fields_b = GetAddressEditorCompletenessRating(b);
  if (incomplete_fields_a != incomplete_fields_b) {
    return incomplete_fields_b - incomplete_fields_a;
  }

  incomplete_fields_a =
      GetValidationErrors(
          field_formatter::CreateAutofillMappings(a, kDefaultLocale),
          required_data_pieces)
          .size();
  incomplete_fields_b =
      GetValidationErrors(
          field_formatter::CreateAutofillMappings(b, kDefaultLocale),
          required_data_pieces)
          .size();
  return incomplete_fields_b - incomplete_fields_a;
}

// Helper function that compares instances of AutofillProfile by completeness
// in regards to the current options. Full profiles should be ordered before
// empty ones and fall back to compare the profile's name in case of equality.
bool CompletenessCompareShippingAddresses(const CollectUserDataOptions& options,
                                          const autofill::AutofillProfile& a,
                                          const autofill::AutofillProfile& b) {
  int address_compare = CompletenessCompareAddresses(
      options.required_shipping_address_data_pieces, a, b);
  if (address_compare != 0) {
    return address_compare > 0;
  }

  return a.use_date() > b.use_date();
}

// Helper function that compares instances of PaymentInstrument by completeness
// in regards to the current options. Full payment instruments should be
// ordered before empty ones and fall back to compare the full name on the
// credit card in case of equality.
bool CompletenessComparePaymentInstruments(
    const CollectUserDataOptions& options,
    const PaymentInstrument& a,
    const PaymentInstrument& b) {
  DCHECK(a.card);
  DCHECK(b.card);
  int incomplete_fields_a =
      GetValidationErrors(
          field_formatter::CreateAutofillMappings(*a.card, kDefaultLocale),
          options.required_credit_card_data_pieces)
          .size();
  int incomplete_fields_b =
      GetValidationErrors(
          field_formatter::CreateAutofillMappings(*b.card, kDefaultLocale),
          options.required_credit_card_data_pieces)
          .size();
  if (incomplete_fields_a != incomplete_fields_b) {
    return incomplete_fields_a <= incomplete_fields_b;
  }

  bool a_has_valid_expiration = a.card->HasValidExpirationDate();
  bool b_has_valid_expiration = b.card->HasValidExpirationDate();
  if (a_has_valid_expiration != b_has_valid_expiration) {
    return !b_has_valid_expiration;
  }

  bool a_has_valid_number =
      (a.card->record_type() != autofill::CreditCard::MASKED_SERVER_CARD &&
       a.card->HasValidCardNumber()) ||
      (a.card->record_type() == autofill::CreditCard::MASKED_SERVER_CARD &&
       !a.card->GetRawInfo(autofill::CREDIT_CARD_NUMBER).empty());
  bool b_has_valid_number =
      (b.card->record_type() != autofill::CreditCard::MASKED_SERVER_CARD &&
       b.card->HasValidCardNumber()) ||
      (b.card->record_type() == autofill::CreditCard::MASKED_SERVER_CARD &&
       !b.card->GetRawInfo(autofill::CREDIT_CARD_NUMBER).empty());
  if (a_has_valid_number != b_has_valid_number) {
    return !b_has_valid_number;
  }

  bool a_has_address = a.billing_address != nullptr;
  bool b_has_address = b.billing_address != nullptr;
  if (a_has_address != b_has_address) {
    return !b_has_address;
  }
  if (a_has_address && b_has_address) {
    int address_compare = CompletenessCompareAddresses(
        options.required_billing_address_data_pieces, *a.billing_address,
        *b.billing_address);
    if (address_compare != 0) {
      return address_compare > 0;
    }
  }

  return a.card->use_date() > b.card->use_date();
}

}  // namespace
namespace user_data {

std::vector<std::string> GetContactValidationErrors(
    const autofill::AutofillProfile* profile,
    const CollectUserDataOptions& collect_user_data_options) {
  if (collect_user_data_options.required_contact_data_pieces.empty()) {
    return std::vector<std::string>();
  }

  return GetValidationErrors(
      profile
          ? field_formatter::CreateAutofillMappings(*profile, kDefaultLocale)
          : std::map<std::string, std::string>(),
      collect_user_data_options.required_contact_data_pieces);
}

std::vector<int> SortContactsByCompleteness(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<autofill::AutofillProfile>>& profiles) {
  std::vector<int> profile_indices(profiles.size());
  std::iota(std::begin(profile_indices), std::end(profile_indices), 0);
  std::stable_sort(profile_indices.begin(), profile_indices.end(),
                   [&collect_user_data_options, &profiles](int i, int j) {
                     return CompletenessCompareContacts(
                         collect_user_data_options, *profiles[i], *profiles[j]);
                   });
  return profile_indices;
}

int GetDefaultContactProfile(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<autofill::AutofillProfile>>& profiles) {
  if (profiles.empty()) {
    return -1;
  }
  auto sorted_indices =
      SortContactsByCompleteness(collect_user_data_options, profiles);
  if (!collect_user_data_options.default_email.empty()) {
    for (int index : sorted_indices) {
      if (base::UTF16ToUTF8(
              profiles[index]->GetRawInfo(autofill::EMAIL_ADDRESS)) ==
          collect_user_data_options.default_email) {
        return index;
      }
    }
  }
  return sorted_indices[0];
}

std::vector<std::string> GetShippingAddressValidationErrors(
    const autofill::AutofillProfile* profile,
    const CollectUserDataOptions& collect_user_data_options) {
  std::vector<std::string> errors;
  if (!collect_user_data_options.request_shipping) {
    return errors;
  }

  if (!collect_user_data_options.required_shipping_address_data_pieces
           .empty()) {
    errors = GetValidationErrors(
        profile
            ? field_formatter::CreateAutofillMappings(*profile, kDefaultLocale)
            : std::map<std::string, std::string>(),
        collect_user_data_options.required_shipping_address_data_pieces);
  }

  // Require address editor completeness if Assistant validation succeeds. If
  // Assistant validation fails, the editor has to be opened and requires
  // completeness to save the change, do not append the (potentially duplicate)
  // error in this case.
  if (errors.empty() && (profile == nullptr ||
                         GetAddressEditorCompletenessRating(*profile) != 0)) {
    errors.push_back(l10n_util::GetStringUTF8(
        IDS_AUTOFILL_ASSISTANT_PAYMENT_INFORMATION_MISSING));
  }
  return errors;
}

std::vector<int> SortShippingAddressesByCompleteness(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<autofill::AutofillProfile>>& profiles) {
  std::vector<int> profile_indices(profiles.size());
  std::iota(std::begin(profile_indices), std::end(profile_indices), 0);
  std::stable_sort(profile_indices.begin(), profile_indices.end(),
                   [&collect_user_data_options, &profiles](int i, int j) {
                     return CompletenessCompareShippingAddresses(
                         collect_user_data_options, *profiles[i], *profiles[j]);
                   });
  return profile_indices;
}

int GetDefaultShippingAddressProfile(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<autofill::AutofillProfile>>& profiles) {
  if (profiles.empty()) {
    return -1;
  }
  auto sorted_indices =
      SortShippingAddressesByCompleteness(collect_user_data_options, profiles);
  return sorted_indices[0];
}

std::vector<std::string> GetPaymentInstrumentValidationErrors(
    const autofill::CreditCard* credit_card,
    const autofill::AutofillProfile* billing_address,
    const CollectUserDataOptions& collect_user_data_options) {
  std::vector<std::string> errors;
  if (!collect_user_data_options.request_payment_method) {
    return errors;
  }

  if (!collect_user_data_options.required_credit_card_data_pieces.empty()) {
    const auto& card_errors = GetValidationErrors(
        credit_card ? field_formatter::CreateAutofillMappings(*credit_card,
                                                              kDefaultLocale)
                    : std::map<std::string, std::string>(),
        collect_user_data_options.required_credit_card_data_pieces);
    errors.insert(errors.end(), card_errors.begin(), card_errors.end());
  }
  if (credit_card && !credit_card->HasValidExpirationDate()) {
    errors.push_back(collect_user_data_options.credit_card_expired_text);
  }

  if (!collect_user_data_options.required_billing_address_data_pieces.empty()) {
    const auto& address_errors = GetValidationErrors(
        billing_address ? field_formatter::CreateAutofillMappings(
                              *billing_address, kDefaultLocale)
                        : std::map<std::string, std::string>(),
        collect_user_data_options.required_billing_address_data_pieces);
    errors.insert(errors.end(), address_errors.begin(), address_errors.end());
  }

  // Require card editor completeness if Assistant validation succeeds. If
  // Assistant validation fails, the editor has to be opened and requires
  // completeness to save the change, do not append the (potentially duplicate)
  // error in this case.
  if (errors.empty()) {
    if (credit_card &&
        credit_card->record_type() !=
            autofill::CreditCard::MASKED_SERVER_CARD &&
        !credit_card->HasValidCardNumber()) {
      // Can't check validity of masked server card numbers, because they are
      // incomplete until decrypted.
      errors.push_back(l10n_util::GetStringUTF8(
          IDS_AUTOFILL_ASSISTANT_PAYMENT_INFORMATION_MISSING));
    } else if (!credit_card || !billing_address ||
               credit_card->billing_address_id().empty() ||
               GetAddressEditorCompletenessRating(*billing_address) != 0) {
      errors.push_back(l10n_util::GetStringUTF8(
          IDS_AUTOFILL_ASSISTANT_PAYMENT_INFORMATION_MISSING));
    }
  }

  return errors;
}

std::vector<int> SortPaymentInstrumentsByCompleteness(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<PaymentInstrument>>&
        payment_instruments) {
  std::vector<int> payment_instrument_indices(payment_instruments.size());
  std::iota(std::begin(payment_instrument_indices),
            std::end(payment_instrument_indices), 0);
  std::stable_sort(
      payment_instrument_indices.begin(), payment_instrument_indices.end(),
      [&collect_user_data_options, &payment_instruments](int a, int b) {
        return CompletenessComparePaymentInstruments(collect_user_data_options,
                                                     *payment_instruments[a],
                                                     *payment_instruments[b]);
      });
  return payment_instrument_indices;
}

int GetDefaultPaymentInstrument(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<PaymentInstrument>>&
        payment_instruments) {
  if (payment_instruments.empty()) {
    return -1;
  }
  auto sorted_indices = SortPaymentInstrumentsByCompleteness(
      collect_user_data_options, payment_instruments);
  return sorted_indices[0];
}

std::unique_ptr<autofill::AutofillProfile> MakeUniqueFromProfile(
    const autofill::AutofillProfile& profile) {
  auto unique_profile = std::make_unique<autofill::AutofillProfile>(profile);
  // Temporary workaround so that fields like first/last name a properly
  // populated.
  unique_profile->FinalizeAfterImport();
  return unique_profile;
}

bool CompareContactDetails(
    const CollectUserDataOptions& collect_user_data_options,
    const autofill::AutofillProfile* a,
    const autofill::AutofillProfile* b) {
  std::vector<autofill::ServerFieldType> types;
  if (collect_user_data_options.request_payer_name) {
    types.emplace_back(autofill::NAME_FULL);
    types.emplace_back(autofill::NAME_FIRST);
    types.emplace_back(autofill::NAME_MIDDLE);
    types.emplace_back(autofill::NAME_LAST);
  }
  if (collect_user_data_options.request_payer_phone) {
    types.emplace_back(autofill::PHONE_HOME_WHOLE_NUMBER);
  }
  if (collect_user_data_options.request_payer_email) {
    types.emplace_back(autofill::EMAIL_ADDRESS);
  }
  if (types.empty()) {
    return a->guid() == b->guid();
  }

  for (auto type : types) {
    int comparison = a->GetRawInfo(type).compare(b->GetRawInfo(type));
    if (comparison != 0) {
      return false;
    }
  }

  return true;
}

ClientStatus GetFormattedAutofillValue(const AutofillValue& autofill_value,
                                       const UserData* user_data,
                                       std::string* out_value) {
  return ExtractDataAndFormatAutofillValue(
      autofill_value, autofill_value.value_expression(), user_data,
      /* quote_meta= */ false, out_value);
}

ClientStatus GetFormattedAutofillValue(
    const AutofillValueRegexp& autofill_value_regexp,
    const UserData* user_data,
    std::string* out_value) {
  return ExtractDataAndFormatAutofillValue(
      autofill_value_regexp,
      autofill_value_regexp.value_expression_re2().value_expression(),
      user_data,
      /* quote_meta= */ true, out_value);
}

void GetPasswordManagerValue(
    const PasswordManagerValue& password_manager_value,
    const ElementFinder::Result& target_element,
    const UserData* user_data,
    WebsiteLoginManager* website_login_manager,
    base::OnceCallback<void(const ClientStatus&, const std::string&)>
        callback) {
  if (!user_data->selected_login_) {
    std::move(callback).Run(ClientStatus(PRECONDITION_FAILED), std::string());
    return;
  }
  if (!target_element.container_frame_host ||
      !url_utils::IsSamePublicSuffixDomain(
          target_element.container_frame_host->GetLastCommittedURL(),
          user_data->selected_login_->origin)) {
    std::move(callback).Run(ClientStatus(PASSWORD_ORIGIN_MISMATCH),
                            std::string());
    return;
  }

  switch (password_manager_value.credential_type()) {
    case PasswordManagerValue::PASSWORD:
      website_login_manager->GetPasswordForLogin(
          *user_data->selected_login_,
          base::BindOnce(&OnGetStoredPassword, std::move(callback)));
      return;
    case PasswordManagerValue::USERNAME:
      std::move(callback).Run(OkClientStatus(),
                              user_data->selected_login_->username);
      return;
    case PasswordManagerValue::NOT_SET:
      std::move(callback).Run(ClientStatus(INVALID_ACTION), std::string());
      return;
  }
}

ClientStatus GetClientMemoryStringValue(const std::string& client_memory_key,
                                        const UserData* user_data,
                                        std::string* out_value) {
  if (client_memory_key.empty()) {
    return ClientStatus(INVALID_ACTION);
  }
  if (!user_data->has_additional_value(client_memory_key) ||
      user_data->additional_value(client_memory_key)
              ->strings()
              .values()
              .size() != 1) {
    VLOG(1) << "Requested key '" << client_memory_key
            << "' not available in client memory";
    return ClientStatus(PRECONDITION_FAILED);
  }
  out_value->assign(
      user_data->additional_value(client_memory_key)->strings().values(0));
  return OkClientStatus();
}

void ResolveTextValue(const TextValue& text_value,
                      const ElementFinder::Result& target_element,
                      const ActionDelegate* action_delegate,
                      base::OnceCallback<void(const ClientStatus&,
                                              const std::string&)> callback) {
  std::string value;
  ClientStatus status = OkClientStatus();
  switch (text_value.value_case()) {
    case TextValue::kText:
      value = text_value.text();
      break;
    case TextValue::kAutofillValue: {
      status = GetFormattedAutofillValue(
          text_value.autofill_value(), action_delegate->GetUserData(), &value);
      break;
    }
    case TextValue::kPasswordManagerValue: {
      GetPasswordManagerValue(text_value.password_manager_value(),
                              target_element, action_delegate->GetUserData(),
                              action_delegate->GetWebsiteLoginManager(),
                              std::move(callback));
      return;
    }
    case TextValue::kClientMemoryKey: {
      status =
          GetClientMemoryStringValue(text_value.client_memory_key(),
                                     action_delegate->GetUserData(), &value);
      break;
    }
    case TextValue::VALUE_NOT_SET:
      status = ClientStatus(INVALID_ACTION);
  }

  std::move(callback).Run(status, value);
}

}  // namespace user_data
}  // namespace autofill_assistant
