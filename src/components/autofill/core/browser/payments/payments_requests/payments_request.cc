// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/payments/payments_client.h"

namespace autofill::payments {

PaymentsRequest::~PaymentsRequest() = default;

base::Value PaymentsRequest::BuildRiskDictionary(
    const std::string& encoded_risk_data) {
  base::Value risk_data(base::Value::Type::DICTIONARY);
#if BUILDFLAG(IS_IOS)
  // Browser fingerprinting is not available on iOS. Instead, we generate
  // RiskAdvisoryData.
  risk_data.SetKey("message_type", base::Value("RISK_ADVISORY_DATA"));
  risk_data.SetKey("encoding_type", base::Value("BASE_64_URL"));
#else
  risk_data.SetKey("message_type",
                   base::Value("BROWSER_NATIVE_FINGERPRINTING"));
  risk_data.SetKey("encoding_type", base::Value("BASE_64"));
#endif

  risk_data.SetKey("value", base::Value(encoded_risk_data));

  return risk_data;
}

base::Value PaymentsRequest::BuildCustomerContextDictionary(
    int64_t external_customer_id) {
  base::Value customer_context(base::Value::Type::DICTIONARY);
  customer_context.SetKey(
      "external_customer_id",
      base::Value(base::NumberToString(external_customer_id)));
  return customer_context;
}

void PaymentsRequest::SetActiveExperiments(
    const std::vector<const char*>& active_experiments,
    base::Value& request_dict) {
  if (active_experiments.empty())
    return;

  base::Value active_chrome_experiments(base::Value::Type::LIST);
  for (const char* it : active_experiments)
    active_chrome_experiments.Append(it);

  request_dict.SetKey("active_chrome_experiments",
                      std::move(active_chrome_experiments));
}

base::Value PaymentsRequest::BuildAddressDictionary(
    const AutofillProfile& profile,
    const std::string& app_locale,
    bool include_non_location_data) {
  base::Value postal_address(base::Value::Type::DICTIONARY);

  if (include_non_location_data) {
    SetStringIfNotEmpty(profile, NAME_FULL, app_locale,
                        PaymentsClient::kRecipientName, postal_address);
  }

  base::Value address_lines(base::Value::Type::LIST);
  AppendStringIfNotEmpty(profile, ADDRESS_HOME_LINE1, app_locale,
                         address_lines);
  AppendStringIfNotEmpty(profile, ADDRESS_HOME_LINE2, app_locale,
                         address_lines);
  AppendStringIfNotEmpty(profile, ADDRESS_HOME_LINE3, app_locale,
                         address_lines);
  if (!address_lines.GetListDeprecated().empty())
    postal_address.SetKey("address_line", std::move(address_lines));

  SetStringIfNotEmpty(profile, ADDRESS_HOME_CITY, app_locale, "locality_name",
                      postal_address);
  SetStringIfNotEmpty(profile, ADDRESS_HOME_STATE, app_locale,
                      "administrative_area_name", postal_address);
  SetStringIfNotEmpty(profile, ADDRESS_HOME_ZIP, app_locale,
                      "postal_code_number", postal_address);

  // Use GetRawInfo to get a country code instead of the country name:
  const std::u16string country_code = profile.GetRawInfo(ADDRESS_HOME_COUNTRY);
  if (!country_code.empty())
    postal_address.SetKey("country_name_code", base::Value(country_code));

  base::Value address(base::Value::Type::DICTIONARY);
  address.SetKey("postal_address", std::move(postal_address));

  if (include_non_location_data) {
    SetStringIfNotEmpty(profile, PHONE_HOME_WHOLE_NUMBER, app_locale,
                        PaymentsClient::kPhoneNumber, address);
  }

  return address;
}

base::Value PaymentsRequest::BuildCreditCardDictionary(
    const CreditCard& credit_card,
    const std::string& app_locale,
    const std::string& pan_field_name) {
  base::Value card(base::Value::Type::DICTIONARY);
  card.SetKey("unique_id", base::Value(credit_card.guid()));

  const std::u16string exp_month =
      credit_card.GetInfo(AutofillType(CREDIT_CARD_EXP_MONTH), app_locale);
  const std::u16string exp_year = credit_card.GetInfo(
      AutofillType(CREDIT_CARD_EXP_4_DIGIT_YEAR), app_locale);
  int value = 0;
  if (base::StringToInt(exp_month, &value))
    card.SetKey("expiration_month", base::Value(value));
  if (base::StringToInt(exp_year, &value))
    card.SetKey("expiration_year", base::Value(value));
  SetStringIfNotEmpty(credit_card, CREDIT_CARD_NAME_FULL, app_locale,
                      "cardholder_name", card);

  if (credit_card.HasNonEmptyValidNickname())
    card.SetKey("nickname", base::Value(credit_card.nickname()));

  card.SetKey("encrypted_pan", base::Value("__param:" + pan_field_name));
  return card;
}

void PaymentsRequest::AppendStringIfNotEmpty(const AutofillProfile& profile,
                                             const ServerFieldType& type,
                                             const std::string& app_locale,
                                             base::Value& list) {
  const std::u16string value = profile.GetInfo(type, app_locale);
  if (!value.empty())
    list.Append(value);
}

void PaymentsRequest::SetStringIfNotEmpty(const AutofillDataModel& profile,
                                          const ServerFieldType& type,
                                          const std::string& app_locale,
                                          const std::string& path,
                                          base::Value& dictionary) {
  const std::u16string value = profile.GetInfo(AutofillType(type), app_locale);
  if (!value.empty())
    dictionary.SetKey(path, base::Value(value));
}

}  // namespace autofill::payments
