// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/get_upload_details_request.h"

#include <string>

#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

namespace autofill::payments {

namespace {
const char kGetUploadDetailsRequestPath[] =
    "payments/apis/chromepaymentsservice/getdetailsforsavecard";
}  // namespace

GetUploadDetailsRequest::GetUploadDetailsRequest(
    const std::vector<AutofillProfile>& addresses,
    const int detected_values,
    const std::vector<const char*>& active_experiments,
    const bool full_sync_enabled,
    const std::string& app_locale,
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                            const std::u16string&,
                            std::unique_ptr<base::Value>,
                            std::vector<std::pair<int, int>>)> callback,
    const int billable_service_number,
    const int64_t billing_customer_number,
    PaymentsClient::UploadCardSource upload_card_source)
    : addresses_(addresses),
      detected_values_(detected_values),
      active_experiments_(active_experiments),
      full_sync_enabled_(full_sync_enabled),
      app_locale_(app_locale),
      callback_(std::move(callback)),
      billable_service_number_(billable_service_number),
      upload_card_source_(upload_card_source),
      billing_customer_number_(billing_customer_number) {}

GetUploadDetailsRequest::~GetUploadDetailsRequest() = default;

std::string GetUploadDetailsRequest::GetRequestUrlPath() {
  return kGetUploadDetailsRequestPath;
}

std::string GetUploadDetailsRequest::GetRequestContentType() {
  return "application/json";
}

std::string GetUploadDetailsRequest::GetRequestContent() {
  base::Value request_dict(base::Value::Type::DICTIONARY);
  base::Value context(base::Value::Type::DICTIONARY);
  context.SetKey("language_code", base::Value(app_locale_));
  context.SetKey("billable_service", base::Value(billable_service_number_));
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSendingBcnInGetUploadDetails) &&
      billing_customer_number_ != 0) {
    context.SetKey("customer_context",
                   BuildCustomerContextDictionary(billing_customer_number_));
  }
  request_dict.SetKey("context", std::move(context));

  base::Value chrome_user_context(base::Value::Type::DICTIONARY);
  chrome_user_context.SetKey("full_sync_enabled",
                             base::Value(full_sync_enabled_));
  request_dict.SetKey("chrome_user_context", std::move(chrome_user_context));

  base::Value addresses(base::Value::Type::LIST);
  for (const AutofillProfile& profile : addresses_) {
    // These addresses are used by Payments to (1) accurately determine the
    // user's country in order to show the correct legal documents and (2) to
    // verify that the addresses are valid for their purposes so that we don't
    // offer save in a case where it would definitely fail (e.g. P.O. boxes if
    // min address is not possible). The final parameter directs
    // BuildAddressDictionary to omit names and phone numbers, which aren't
    // useful for these purposes.
    addresses.Append(BuildAddressDictionary(profile, app_locale_, false));
  }
  request_dict.SetKey("address", std::move(addresses));

  // It's possible we may not have found name/address/CVC in the checkout
  // flow. The detected_values_ bitmask tells Payments what *was* found, and
  // Payments will decide if the provided data is enough to offer upload save.
  request_dict.SetKey("detected_values", base::Value(detected_values_));

  SetActiveExperiments(active_experiments_, request_dict);

  switch (upload_card_source_) {
    case PaymentsClient::UploadCardSource::UNKNOWN_UPLOAD_CARD_SOURCE:
      request_dict.SetKey("upload_card_source",
                          base::Value("UNKNOWN_UPLOAD_CARD_SOURCE"));
      break;
    case PaymentsClient::UploadCardSource::UPSTREAM_CHECKOUT_FLOW:
      request_dict.SetKey("upload_card_source",
                          base::Value("UPSTREAM_CHECKOUT_FLOW"));
      break;
    case PaymentsClient::UploadCardSource::UPSTREAM_SETTINGS_PAGE:
      request_dict.SetKey("upload_card_source",
                          base::Value("UPSTREAM_SETTINGS_PAGE"));
      break;
    case PaymentsClient::UploadCardSource::UPSTREAM_CARD_OCR:
      request_dict.SetKey("upload_card_source",
                          base::Value("UPSTREAM_CARD_OCR"));
      break;
    case PaymentsClient::UploadCardSource::LOCAL_CARD_MIGRATION_CHECKOUT_FLOW:
      request_dict.SetKey("upload_card_source",
                          base::Value("LOCAL_CARD_MIGRATION_CHECKOUT_FLOW"));
      break;
    case PaymentsClient::UploadCardSource::LOCAL_CARD_MIGRATION_SETTINGS_PAGE:
      request_dict.SetKey("upload_card_source",
                          base::Value("LOCAL_CARD_MIGRATION_SETTINGS_PAGE"));
      break;
    default:
      NOTREACHED();
  }

  std::string request_content;
  base::JSONWriter::Write(request_dict, &request_content);
  VLOG(3) << "getdetailsforsavecard request body: " << request_content;
  return request_content;
}

void GetUploadDetailsRequest::ParseResponse(const base::Value& response) {
  const auto* context_token = response.FindStringKey("context_token");
  context_token_ =
      context_token ? base::UTF8ToUTF16(*context_token) : std::u16string();

  const base::Value* dictionary_value =
      response.FindKeyOfType("legal_message", base::Value::Type::DICTIONARY);
  if (dictionary_value)
    legal_message_ = std::make_unique<base::Value>(dictionary_value->Clone());

  const auto* supported_card_bin_ranges_string =
      response.FindStringKey("supported_card_bin_ranges_string");
  supported_card_bin_ranges_ = ParseSupportedCardBinRangesString(
      supported_card_bin_ranges_string ? *supported_card_bin_ranges_string
                                       : base::EmptyString());
}

bool GetUploadDetailsRequest::IsResponseComplete() {
  return !context_token_.empty() && legal_message_;
}

void GetUploadDetailsRequest::RespondToDelegate(
    AutofillClient::PaymentsRpcResult result) {
  std::move(callback_).Run(result, context_token_, std::move(legal_message_),
                           supported_card_bin_ranges_);
}

std::vector<std::pair<int, int>>
GetUploadDetailsRequest::ParseSupportedCardBinRangesString(
    const std::string& supported_card_bin_ranges_string) {
  std::vector<std::pair<int, int>> supported_card_bin_ranges;
  std::vector<std::string> range_strings =
      base::SplitString(supported_card_bin_ranges_string, ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  for (std::string& range_string : range_strings) {
    std::vector<std::string> range = base::SplitString(
        range_string, "-", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    DCHECK(range.size() <= 2);
    int start;
    base::StringToInt(range[0], &start);
    if (range.size() == 1) {
      supported_card_bin_ranges.emplace_back(start, start);
    } else {
      int end;
      base::StringToInt(range[1], &end);
      DCHECK_LE(start, end);
      supported_card_bin_ranges.emplace_back(start, end);
    }
  }
  return supported_card_bin_ranges;
}

}  // namespace autofill::payments
