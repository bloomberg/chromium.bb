// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/test_credit_card_fido_authenticator.h"

#include <string>
#include <utility>

#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"

namespace autofill {

TestCreditCardFIDOAuthenticator::TestCreditCardFIDOAuthenticator(
    AutofillDriver* driver,
    AutofillClient* client)
    : CreditCardFIDOAuthenticator(driver, client) {}

TestCreditCardFIDOAuthenticator::~TestCreditCardFIDOAuthenticator() = default;

void TestCreditCardFIDOAuthenticator::Authenticate(
    const CreditCard* card,
    base::WeakPtr<Requester> requester,
    base::Value request_options,
    absl::optional<std::string> context_token) {
  authenticate_invoked_ = true;
  card_ = *card;
  context_token_ = context_token;
  CreditCardFIDOAuthenticator::Authenticate(
      card, requester, std::move(request_options), context_token);
}

void TestCreditCardFIDOAuthenticator::GetAssertion(
    PublicKeyCredentialRequestOptionsPtr request_options) {
  request_options_ = request_options->Clone();
  CreditCardFIDOAuthenticator::GetAssertion(std::move(request_options));
}

void TestCreditCardFIDOAuthenticator::MakeCredential(
    PublicKeyCredentialCreationOptionsPtr creation_options) {
  creation_options_ = creation_options->Clone();
  CreditCardFIDOAuthenticator::MakeCredential(std::move(creation_options));
}

void TestCreditCardFIDOAuthenticator::OptOut() {
  opt_out_called_ = true;
  CreditCardFIDOAuthenticator::OptOut();
}

// static
void TestCreditCardFIDOAuthenticator::GetAssertion(
    CreditCardFIDOAuthenticator* fido_authenticator,
    bool did_succeed) {
  if (did_succeed) {
    GetAssertionAuthenticatorResponsePtr response =
        GetAssertionAuthenticatorResponse::New();
    response->info = blink::mojom::CommonCredentialInfo::New();
    fido_authenticator->OnDidGetAssertion(AuthenticatorStatus::SUCCESS,
                                          std::move(response));
  } else {
    fido_authenticator->OnDidGetAssertion(
        AuthenticatorStatus::NOT_ALLOWED_ERROR, nullptr);
  }
}

// static
void TestCreditCardFIDOAuthenticator::MakeCredential(
    CreditCardFIDOAuthenticator* fido_authenticator,
    bool did_succeed) {
  if (did_succeed) {
    MakeCredentialAuthenticatorResponsePtr response =
        MakeCredentialAuthenticatorResponse::New();
    response->info = blink::mojom::CommonCredentialInfo::New();
    fido_authenticator->OnDidMakeCredential(AuthenticatorStatus::SUCCESS,
                                            std::move(response));
  } else {
    fido_authenticator->OnDidMakeCredential(
        AuthenticatorStatus::NOT_ALLOWED_ERROR, nullptr);
  }
}

std::vector<uint8_t> TestCreditCardFIDOAuthenticator::GetCredentialId() {
  DCHECK(!request_options_->allow_credentials.empty());
  return request_options_->allow_credentials.front().id;
}

std::vector<uint8_t> TestCreditCardFIDOAuthenticator::GetChallenge() {
  if (request_options_) {
    return request_options_->challenge;
  } else {
    DCHECK(creation_options_);
    return creation_options_->challenge;
  }
}

std::string TestCreditCardFIDOAuthenticator::GetRelyingPartyId() {
  if (request_options_) {
    return request_options_->relying_party_id;
  } else {
    DCHECK(creation_options_);
    return creation_options_->relying_party.id;
  }
}

void TestCreditCardFIDOAuthenticator::IsUserVerifiable(
    base::OnceCallback<void(bool)> callback) {
  return std::move(callback).Run(is_user_verifiable_);
}

bool TestCreditCardFIDOAuthenticator::IsUserOptedIn() {
  if (is_user_opted_in_.has_value())
    return is_user_opted_in_.value();

  return CreditCardFIDOAuthenticator::IsUserOptedIn();
}

void TestCreditCardFIDOAuthenticator::Reset() {
  is_user_verifiable_ = false;
  is_user_opted_in_ = absl::nullopt;
  opt_out_called_ = false;
  authenticate_invoked_ = false;
  card_ = CreditCard();
}

}  // namespace autofill
