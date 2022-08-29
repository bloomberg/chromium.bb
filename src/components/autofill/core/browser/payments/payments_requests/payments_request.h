// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_PAYMENTS_REQUEST_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_PAYMENTS_REQUEST_H_

#include <string>

#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/common/autofill_payments_features.h"

namespace base {
class Value;
}

namespace autofill::payments {

// Shared class for the various Payments request types.
class PaymentsRequest {
 public:
  virtual ~PaymentsRequest();

  // Returns the URL path for this type of request.
  virtual std::string GetRequestUrlPath() = 0;

  // Returns the content type that should be used in the HTTP request.
  virtual std::string GetRequestContentType() = 0;

  // Returns the content that should be provided in the HTTP request.
  virtual std::string GetRequestContent() = 0;

  // Parses the required elements of the HTTP response.
  virtual void ParseResponse(const base::Value& response) = 0;

  // Returns true if all of the required elements were successfully retrieved by
  // a call to ParseResponse.
  virtual bool IsResponseComplete() = 0;

  // Invokes the appropriate callback in the delegate based on what type of
  // request this is.
  virtual void RespondToDelegate(AutofillClient::PaymentsRpcResult result) = 0;

 protected:
  // Shared helper function to build the risk data sent in the request.
  base::Value BuildRiskDictionary(const std::string& encoded_risk_data);

  // Shared helper function to build the customer context sent in the request.
  base::Value BuildCustomerContextDictionary(int64_t external_customer_id);

  // Shared helper function that populates the list of active experiments that
  // affect either the data sent in payments RPCs or whether the RPCs are sent
  // or not.
  void SetActiveExperiments(const std::vector<const char*>& active_experiments,
                            base::Value& request_dict);

  // Shared helper functoin that returns a dictionary with the structure
  // expected by Payments RPCs, containing each of the fields in |profile|,
  // formatted according to |app_locale|. If |include_non_location_data| is
  // false, the name and phone number in |profile| are not included.
  base::Value BuildAddressDictionary(const AutofillProfile& profile,
                                     const std::string& app_locale,
                                     bool include_non_location_data);

  // Shared helper function that returns a dictionary of the credit card with
  // the structure expected by Payments RPCs, containing expiration month,
  // expiration year and cardholder name (if any) fields in |credit_card|,
  // formatted according to |app_locale|. |pan_field_name| is the field name for
  // the encrypted pan. We use each credit card's guid as the unique id.
  base::Value BuildCreditCardDictionary(const CreditCard& credit_card,
                                        const std::string& app_locale,
                                        const std::string& pan_field_name);

  // Shared helper functions for string operations.
  void AppendStringIfNotEmpty(const AutofillProfile& profile,
                              const ServerFieldType& type,
                              const std::string& app_locale,
                              base::Value& list);
  void SetStringIfNotEmpty(const AutofillDataModel& profile,
                           const ServerFieldType& type,
                           const std::string& app_locale,
                           const std::string& path,
                           base::Value& dictionary);
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_PAYMENTS_REQUEST_H_
