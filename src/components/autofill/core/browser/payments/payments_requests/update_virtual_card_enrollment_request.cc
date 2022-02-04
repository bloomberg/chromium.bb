// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/update_virtual_card_enrollment_request.h"

#include <string>

#include "base/json/json_writer.h"
#include "base/values.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"

namespace autofill {
namespace payments {

namespace {
const char kUpdateVirtualCardEnrollmentRequestPath[] =
    "payments/apis/virtualcardservice/enroll";
}  // namespace

UpdateVirtualCardEnrollmentRequest::UpdateVirtualCardEnrollmentRequest(
    const PaymentsClient::UpdateVirtualCardEnrollmentRequestDetails&
        request_details,
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult)> callback)
    : request_details_(request_details), callback_(std::move(callback)) {}

UpdateVirtualCardEnrollmentRequest::~UpdateVirtualCardEnrollmentRequest() =
    default;

std::string UpdateVirtualCardEnrollmentRequest::GetRequestUrlPath() {
  return kUpdateVirtualCardEnrollmentRequestPath;
}

std::string UpdateVirtualCardEnrollmentRequest::GetRequestContentType() {
  return "application/json";
}

std::string UpdateVirtualCardEnrollmentRequest::GetRequestContent() {
  base::Value request_dict(base::Value::Type::DICTIONARY);

  switch (request_details_.virtual_card_enrollment_request_type) {
    case VirtualCardEnrollmentRequestType::kEnroll:
      BuildEnrollRequestDictionary(&request_dict);
      break;
    case VirtualCardEnrollmentRequestType::kUnenroll:
      BuildUnenrollRequestDictionary(&request_dict);
      break;
    case VirtualCardEnrollmentRequestType::kNone:
      NOTREACHED();
      break;
  }

  std::string request_content;
  base::JSONWriter::Write(request_dict, &request_content);
  VLOG(3) << "UpdateVirtualCardEnrollmentRequest Body: " << request_content;
  return request_content;
}

void UpdateVirtualCardEnrollmentRequest::ParseResponse(
    const base::Value& response) {
  // Only enroll requests have a response to parse, unenroll request responses
  // are empty except for possible errors which are parsed in PaymentsClient.
  if (request_details_.virtual_card_enrollment_request_type ==
      VirtualCardEnrollmentRequestType::kEnroll) {
    auto* enroll_result =
        response.FindKeyOfType("enroll_result", base::Value::Type::STRING);
    if (enroll_result) {
      enroll_result_ = enroll_result->GetString();
    }
  }
}

bool UpdateVirtualCardEnrollmentRequest::IsResponseComplete() {
  switch (request_details_.virtual_card_enrollment_request_type) {
    case VirtualCardEnrollmentRequestType::kEnroll:
      // If it is an enroll request, we know the response is complete if the
      // response has an enroll result that is ENROLL_SUCCESS, as that is the
      // only field in an enroll response other than the possible error.
      return enroll_result_.has_value() && enroll_result_ == "ENROLL_SUCCESS";
    case VirtualCardEnrollmentRequestType::kUnenroll:
      // Unenroll responses are empty except for having an error. In
      // PaymentsClient, if the response has an error it will be handled before
      // we check IsResponseComplete(), so if we ever reach this branch we know
      // the response completed successfully as there is no error. Thus, we
      // always return true.
      return true;
    case VirtualCardEnrollmentRequestType::kNone:
      NOTREACHED();
      return false;
  }
}

void UpdateVirtualCardEnrollmentRequest::RespondToDelegate(
    AutofillClient::PaymentsRpcResult result) {
  std::move(callback_).Run(result);
}

void UpdateVirtualCardEnrollmentRequest::BuildEnrollRequestDictionary(
    base::Value* request_dict) {
  DCHECK(request_details_.virtual_card_enrollment_request_type ==
         VirtualCardEnrollmentRequestType::kEnroll);

  // If it is an enroll request, we should always have a context token from the
  // previous GetDetailsForEnroll request and we should not have an instrument
  // id set in |request_details_|.
  DCHECK(request_details_.vcn_context_token.has_value() &&
         !request_details_.instrument_id.has_value());

  // Builds the context and channel_type for this enroll request.
  base::Value context(base::Value::Type::DICTIONARY);
  switch (request_details_.virtual_card_enrollment_source) {
    case VirtualCardEnrollmentSource::kUpstream:
      context.SetKey("billable_service",
                     base::Value(kUploadCardBillableServiceNumber));
      request_dict->SetKey("channel_type", base::Value("CHROME_UPSTREAM"));
      break;
    case VirtualCardEnrollmentSource::kDownstream:
      // Downstream enroll is treated the same as settings page enroll because
      // chrome client should already have a card synced from the server.
      // Fall-through.
    case VirtualCardEnrollmentSource::kSettingsPage:
      context.SetKey("billable_service",
                     base::Value(kUnmaskCardBillableServiceNumber));
      request_dict->SetKey("channel_type", base::Value("CHROME_DOWNSTREAM"));
      break;
    case VirtualCardEnrollmentSource::kNone:
      NOTREACHED();
      break;
  }
  if (request_details_.billing_customer_number != 0) {
    context.SetKey("customer_context",
                   BuildCustomerContextDictionary(
                       request_details_.billing_customer_number));
  }
  request_dict->SetKey("context", std::move(context));

  // Sets the virtual_card_enrollment_flow field in this enroll request which
  // lets the server know whether the enrollment is happening with ToS or not.
  // Chrome client requests will always be ENROLL_WITH_TOS. This field is
  // necessary because virtual card enroll through other platforms enrolls
  // without ToS, for example Web Push Provisioning.
  request_dict->SetKey("virtual_card_enrollment_flow",
                       base::Value("ENROLL_WITH_TOS"));

  // Sets the context_token field in this enroll request which is used by the
  // server to link this enroll request to the previous
  // GetDetailsForEnrollRequest, as well as to retrieve the specific credit card
  // to enroll.
  request_dict->SetKey("context_token",
                       base::Value(request_details_.vcn_context_token.value()));
}

void UpdateVirtualCardEnrollmentRequest::BuildUnenrollRequestDictionary(
    base::Value* request_dict) {
  DCHECK(request_details_.virtual_card_enrollment_request_type ==
         VirtualCardEnrollmentRequestType::kUnenroll);

  // If it is an unenroll request, we should always have an instrument id and we
  // should not have a context token set in |request_details_|.
  DCHECK(request_details_.instrument_id.has_value() &&
         !request_details_.vcn_context_token.has_value());

  // Builds the context for this unenroll request if a billing customer number
  // is present.
  if (request_details_.billing_customer_number != 0) {
    base::Value context(base::Value::Type::DICTIONARY);
    context.SetKey("customer_context",
                   BuildCustomerContextDictionary(
                       request_details_.billing_customer_number));
    request_dict->SetKey("context", std::move(context));
  }

  // Sets the instrument_id field in this unenroll request which is used by
  // the server to get the appropriate credit card to unenroll.
  request_dict->SetKey("instrument_id",
                       base::Value(base::NumberToString(
                           request_details_.instrument_id.value())));
}

}  // namespace payments
}  // namespace autofill
