// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_AUTHENTICATION_REQUESTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_AUTHENTICATION_REQUESTER_H_

#include <memory>

#include "base/strings/string16.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/credit_card_cvc_authenticator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

// Test class for requesting authentication from CreditCardCVCAuthenticator.
class TestAuthenticationRequester
    : public CreditCardCVCAuthenticator::Requester {
 public:
  TestAuthenticationRequester();
  ~TestAuthenticationRequester() override;

  void OnCVCAuthenticationComplete(
      bool did_succeed,
      const CreditCard* card = nullptr,
      const base::string16& cvc = base::string16()) override;

  base::WeakPtr<TestAuthenticationRequester> GetWeakPtr();

  base::string16 number() { return number_; }

  MOCK_METHOD0(Success, void());
  MOCK_METHOD0(Failure, void());

 private:
  // The card number returned from OnCVCAuthenticationComplete.
  base::string16 number_;

  base::WeakPtrFactory<TestAuthenticationRequester> weak_ptr_factory_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_AUTHENTICATION_REQUESTER_H_
