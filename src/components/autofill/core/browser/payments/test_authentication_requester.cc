// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/test_authentication_requester.h"

#include "base/strings/string16.h"
#include "components/autofill/core/browser/data_model/credit_card.h"

namespace autofill {

TestAuthenticationRequester::TestAuthenticationRequester()
    : weak_ptr_factory_(this) {}

TestAuthenticationRequester::~TestAuthenticationRequester() {}

base::WeakPtr<TestAuthenticationRequester>
TestAuthenticationRequester::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void TestAuthenticationRequester::OnCVCAuthenticationComplete(
    bool did_succeed,
    const CreditCard* card,
    const base::string16& cvc) {
  if (did_succeed) {
    number_ = card->number();
    return Success();
  }
  return Failure();
}

}  // namespace autofill
