// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/upload_card_request.h"

#include <string>

#include "base/feature_list.h"
#include "base/json/json_writer.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/autofill_payments_features.h"

namespace autofill::payments {

namespace {
const char kUploadCardRequestPath[] =
    "payments/apis-secure/chromepaymentsservice/savecard"
    "?s7e_suffix=chromewallet";
const char kUploadCardRequestFormat[] =
    "requestContentType=application/json; charset=utf-8&request=%s"
    "&s7e_1_pan=%s&s7e_13_cvc=%s";
const char kUploadCardRequestFormatWithoutCvc[] =
    "requestContentType=application/json; charset=utf-8&request=%s"
    "&s7e_1_pan=%s";
}  // namespace

UploadCardRequest::UploadCardRequest(
    const PaymentsClient::UploadRequestDetails& request_details,
    const bool full_sync_enabled,
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                            const PaymentsClient::UploadCardResponseDetails&)>
        callback)
    : request_details_(request_details),
      full_sync_enabled_(full_sync_enabled),
      callback_(std::move(callback)) {}

UploadCardRequest::~UploadCardRequest() = default;

std::string UploadCardRequest::GetRequestUrlPath() {
  return kUploadCardRequestPath;
}

std::string UploadCardRequest::GetRequestContentType() {
  return "application/x-www-form-urlencoded";
}

std::string UploadCardRequest::GetRequestContent() {
  base::Value request_dict(base::Value::Type::DICTIONARY);
  request_dict.SetKey("encrypted_pan", base::Value("__param:s7e_1_pan"));
  if (!request_details_.cvc.empty())
    request_dict.SetKey("encrypted_cvc", base::Value("__param:s7e_13_cvc"));
  request_dict.SetKey("risk_data_encoded",
                      BuildRiskDictionary(request_details_.risk_data));

  const std::string& app_locale = request_details_.app_locale;
  base::Value context(base::Value::Type::DICTIONARY);
  context.SetKey("language_code", base::Value(app_locale));
  context.SetKey("billable_service",
                 base::Value(kUploadCardBillableServiceNumber));
  if (request_details_.billing_customer_number != 0) {
    context.SetKey("customer_context",
                   BuildCustomerContextDictionary(
                       request_details_.billing_customer_number));
  }
  request_dict.SetKey("context", std::move(context));

  base::Value chrome_user_context(base::Value::Type::DICTIONARY);
  chrome_user_context.SetKey("full_sync_enabled",
                             base::Value(full_sync_enabled_));
  request_dict.SetKey("chrome_user_context", std::move(chrome_user_context));

  SetStringIfNotEmpty(request_details_.card, CREDIT_CARD_NAME_FULL, app_locale,
                      "cardholder_name", request_dict);

  base::Value addresses(base::Value::Type::LIST);
  for (const AutofillProfile& profile : request_details_.profiles) {
    addresses.Append(BuildAddressDictionary(profile, app_locale, true));
  }
  request_dict.SetKey("address", std::move(addresses));

  request_dict.SetKey("context_token",
                      base::Value(request_details_.context_token));

  int value = 0;
  const std::u16string exp_month = request_details_.card.GetInfo(
      AutofillType(CREDIT_CARD_EXP_MONTH), app_locale);
  const std::u16string exp_year = request_details_.card.GetInfo(
      AutofillType(CREDIT_CARD_EXP_4_DIGIT_YEAR), app_locale);
  if (base::StringToInt(exp_month, &value))
    request_dict.SetKey("expiration_month", base::Value(value));
  if (base::StringToInt(exp_year, &value))
    request_dict.SetKey("expiration_year", base::Value(value));

  if (request_details_.card.HasNonEmptyValidNickname()) {
    request_dict.SetKey("nickname",
                        base::Value(request_details_.card.nickname()));
  }

  SetActiveExperiments(request_details_.active_experiments, request_dict);

  const std::u16string pan = request_details_.card.GetInfo(
      AutofillType(CREDIT_CARD_NUMBER), app_locale);
  std::string json_request;
  base::JSONWriter::Write(request_dict, &json_request);
  std::string request_content;
  if (request_details_.cvc.empty()) {
    request_content = base::StringPrintf(
        kUploadCardRequestFormatWithoutCvc,
        base::EscapeUrlEncodedData(json_request, true).c_str(),
        base::EscapeUrlEncodedData(base::UTF16ToASCII(pan), true).c_str());
  } else {
    request_content = base::StringPrintf(
        kUploadCardRequestFormat,
        base::EscapeUrlEncodedData(json_request, true).c_str(),
        base::EscapeUrlEncodedData(base::UTF16ToASCII(pan), true).c_str(),
        base::EscapeUrlEncodedData(base::UTF16ToASCII(request_details_.cvc),
                                   true)
            .c_str());
  }
  VLOG(3) << "savecard request body: " << request_content;
  return request_content;
}

void UploadCardRequest::ParseResponse(const base::Value& response) {
  const std::string* credit_card_id = response.FindStringKey("credit_card_id");
  upload_card_response_details_.server_id =
      credit_card_id ? *credit_card_id : std::string();

  const std::string* response_instrument_id =
      response.FindStringKey("instrument_id");
  if (response_instrument_id) {
    int64_t instrument_id;
    if (base::StringToInt64(base::StringPiece(*response_instrument_id),
                            &instrument_id)) {
      upload_card_response_details_.instrument_id = instrument_id;
    }
  }

  const std::string* card_art_url = response.FindStringKey("card_art_url");
  upload_card_response_details_.card_art_url =
      card_art_url ? GURL(*card_art_url) : GURL();

  const auto* virtual_card_metadata = response.FindKeyOfType(
      "virtual_card_metadata", base::Value::Type::DICTIONARY);
  if (virtual_card_metadata) {
    const std::string* virtual_card_enrollment_status =
        virtual_card_metadata->FindStringKey("status");
    if (virtual_card_enrollment_status) {
      if (*virtual_card_enrollment_status == "ENROLLED") {
        upload_card_response_details_.virtual_card_enrollment_state =
            CreditCard::VirtualCardEnrollmentState::ENROLLED;
      } else if (*virtual_card_enrollment_status == "ENROLLMENT_ELIGIBLE") {
        upload_card_response_details_.virtual_card_enrollment_state =
            CreditCard::VirtualCardEnrollmentState::UNENROLLED_AND_ELIGIBLE;
      } else {
        upload_card_response_details_.virtual_card_enrollment_state =
            CreditCard::VirtualCardEnrollmentState::UNENROLLED_AND_NOT_ELIGIBLE;
      }
    }

    if (base::FeatureList::IsEnabled(
            features::
                kAutofillEnableGetDetailsForEnrollParsingInUploadCardResponse) &&
        !base::FeatureList::IsEnabled(
            features::kAutofillEnableToolbarStatusChip) &&
        !base::FeatureList::IsEnabled(
            features::kAutofillCreditCardUploadFeedback) &&
        upload_card_response_details_.virtual_card_enrollment_state ==
            CreditCard::VirtualCardEnrollmentState::UNENROLLED_AND_ELIGIBLE) {
      const auto* virtual_card_enrollment_data =
          virtual_card_metadata->FindKeyOfType("virtual_card_enrollment_data",
                                               base::Value::Type::DICTIONARY);
      if (virtual_card_enrollment_data) {
        PaymentsClient::GetDetailsForEnrollmentResponseDetails
            get_details_for_enrollment_response_details;
        const base::Value* google_legal_message =
            virtual_card_enrollment_data->FindKeyOfType(
                "google_legal_message", base::Value::Type::DICTIONARY);
        if (google_legal_message) {
          LegalMessageLine::Parse(
              *google_legal_message,
              &get_details_for_enrollment_response_details.google_legal_message,
              /*escape_apostrophes=*/true);
        }

        const base::Value* external_legal_message =
            virtual_card_enrollment_data->FindKeyOfType(
                "external_legal_message", base::Value::Type::DICTIONARY);
        if (external_legal_message) {
          LegalMessageLine::Parse(
              *external_legal_message,
              &get_details_for_enrollment_response_details.issuer_legal_message,
              /*escape_apostrophes=*/true);
        }

        const auto* context_token =
            virtual_card_enrollment_data->FindStringKey("context_token");
        get_details_for_enrollment_response_details.vcn_context_token =
            context_token ? *context_token : std::string();

        upload_card_response_details_
            .get_details_for_enrollment_response_details =
            get_details_for_enrollment_response_details;
      }
    }
  }
}

bool UploadCardRequest::IsResponseComplete() {
  return true;
}

void UploadCardRequest::RespondToDelegate(
    AutofillClient::PaymentsRpcResult result) {
  std::move(callback_).Run(result, upload_card_response_details_);
}

}  // namespace autofill::payments
