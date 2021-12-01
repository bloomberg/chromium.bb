// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/select_challenge_option_request.h"

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/payments/payments_client.h"

namespace autofill {
namespace payments {

namespace {
const char kSelectChallengeOptionRequestPath[] =
    "payments/apis/chromepaymentsservice/selectchallengeoption";
}  // namespace

SelectChallengeOptionRequest::SelectChallengeOptionRequest(
    PaymentsClient::SelectChallengeOptionRequestDetails request_details,
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                            const std::string&)> callback)
    : request_details_(request_details), callback_(std::move(callback)) {}

SelectChallengeOptionRequest::~SelectChallengeOptionRequest() = default;

std::string SelectChallengeOptionRequest::GetRequestUrlPath() {
  return kSelectChallengeOptionRequestPath;
}

std::string SelectChallengeOptionRequest::GetRequestContentType() {
  return "application/json";
}

std::string SelectChallengeOptionRequest::GetRequestContent() {
  base::Value request_dict(base::Value::Type::DICTIONARY);
  base::Value context(base::Value::Type::DICTIONARY);
  context.SetKey("billable_service",
                 base::Value(kUnmaskCardBillableServiceNumber));
  if (request_details_.billing_customer_number != 0) {
    context.SetKey("customer_context",
                   BuildCustomerContextDictionary(
                       request_details_.billing_customer_number));
  }
  request_dict.SetKey("context", std::move(context));

  base::Value selected_idv_method(base::Value::Type::DICTIONARY);

  DCHECK_NE(request_details_.selected_challenge_option.type,
            CardUnmaskChallengeOptionType::kUnknownType);
  // Set if selected idv option is sms otp option.
  if (request_details_.selected_challenge_option.type ==
      CardUnmaskChallengeOptionType::kSmsOtp) {
    base::Value sms_challenge_option(base::Value::Type::DICTIONARY);
    // We only get and set the challenge id.
    if (!request_details_.selected_challenge_option.id.empty()) {
      sms_challenge_option.SetKey(
          "challenge_id",
          base::Value(request_details_.selected_challenge_option.id));
    }
    selected_idv_method.SetKey("sms_otp_challenge_option",
                               std::move(sms_challenge_option));
  }
  request_dict.SetKey("selected_idv_challenge_option",
                      std::move(selected_idv_method));

  if (!request_details_.context_token.empty()) {
    request_dict.SetKey("context_token",
                        base::Value(request_details_.context_token));
  }

  std::string request_content;
  base::JSONWriter::Write(request_dict, &request_content);
  VLOG(3) << "selectchallengeoption request body: " << request_content;
  return request_content;
}

void SelectChallengeOptionRequest::ParseResponse(const base::Value& response) {
  const std::string* updated_context_token =
      response.FindStringKey("context_token");
  updated_context_token_ =
      updated_context_token ? *updated_context_token : std::string();
}

bool SelectChallengeOptionRequest::IsResponseComplete() {
  return !updated_context_token_.empty();
}

void SelectChallengeOptionRequest::RespondToDelegate(
    AutofillClient::PaymentsRpcResult result) {
  std::move(callback_).Run(result, updated_context_token_);
}

}  // namespace payments
}  // namespace autofill
