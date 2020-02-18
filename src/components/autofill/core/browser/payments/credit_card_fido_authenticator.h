// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_FIDO_AUTHENTICATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_FIDO_AUTHENTICATOR_H_

#include <memory>

#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/full_card_request.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "device/fido/fido_constants.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/webauthn/internal_authenticator.mojom.h"

namespace autofill {

using blink::mojom::AuthenticatorStatus;
using blink::mojom::GetAssertionAuthenticatorResponse;
using blink::mojom::GetAssertionAuthenticatorResponsePtr;
using blink::mojom::InternalAuthenticator;
using blink::mojom::MakeCredentialAuthenticatorResponse;
using blink::mojom::MakeCredentialAuthenticatorResponsePtr;
using blink::mojom::PublicKeyCredentialCreationOptions;
using blink::mojom::PublicKeyCredentialCreationOptionsPtr;
using blink::mojom::PublicKeyCredentialRequestOptions;
using blink::mojom::PublicKeyCredentialRequestOptionsPtr;
using device::CredentialType;
using device::FidoTransportProtocol;
using device::PublicKeyCredentialDescriptor;
using device::UserVerificationRequirement;

// Authenticates credit card unmasking through FIDO authentication, using the
// WebAuthn specification, standardized by the FIDO alliance. The Webauthn
// specification defines an API to cryptographically bind a server and client,
// and verify that binding. More information can be found here:
// - https://www.w3.org/TR/webauthn-1/
// - https://fidoalliance.org/fido2/
class CreditCardFIDOAuthenticator
    : public payments::FullCardRequest::ResultDelegate {
 public:
  // Useful for splitting metrics to correct sub-histograms and knowing which
  // Payments RPC's to send.
  enum Flow {
    // No flow is in progress.
    NONE_FLOW,
    // Authentication flow.
    AUTHENTICATION_FLOW,
    // Registration flow, including a challenge to sign.
    OPT_IN_WITH_CHALLENGE_FLOW,
    // Opt-in attempt flow, no challenge to sign.
    OPT_IN_WITHOUT_CHALLENGE_FLOW,
    // Opt-out flow.
    OPT_OUT_FLOW,
  };
  class Requester {
   public:
    virtual ~Requester() {}
    virtual void OnFIDOAuthenticationComplete(
        bool did_succeed,
        const CreditCard* card = nullptr) = 0;
  };
  CreditCardFIDOAuthenticator(AutofillDriver* driver, AutofillClient* client);
  ~CreditCardFIDOAuthenticator() override;

  // Offer the option to use WebAuthn for authenticating future card unmasking.
  void ShowWebauthnOfferDialog();

  // Authentication
  void Authenticate(const CreditCard* card,
                    base::WeakPtr<Requester> requester,
                    base::TimeTicks form_parsed_timestamp,
                    base::Value request_options);

  // Registration
  void Register(base::Value creation_options = base::Value());

  // Opts the user out.
  void OptOut();

  // Invokes callback with true if user has a verifying platform authenticator.
  // e.g. Touch/Face ID, Windows Hello, Android fingerprint, etc., is available
  // and enabled. Otherwise invokes callback with false.
  virtual void IsUserVerifiable(base::OnceCallback<void(bool)> callback);

  // The synchronous version of IsUserVerifiable. Used on settings page load.
  bool IsUserVerifiable();

  // Returns true only if the user has opted-in to use WebAuthn for autofill.
  virtual bool IsUserOptedIn();

  // Ensures that local user opt-in pref is in-sync with payments server.
  void SyncUserOptIn(AutofillClient::UnmaskDetails& unmask_details);

  // Returns the current flow.
  Flow current_flow() { return current_flow_; }

 private:
  friend class AutofillManagerTest;
  friend class CreditCardAccessManagerTest;
  friend class CreditCardFIDOAuthenticatorTest;
  friend class TestCreditCardFIDOAuthenticator;
  FRIEND_TEST_ALL_PREFIXES(CreditCardFIDOAuthenticatorTest,
                           ParseRequestOptions);
  FRIEND_TEST_ALL_PREFIXES(CreditCardFIDOAuthenticatorTest,
                           ParseAssertionResponse);
  FRIEND_TEST_ALL_PREFIXES(CreditCardFIDOAuthenticatorTest,
                           ParseCreationOptions);
  FRIEND_TEST_ALL_PREFIXES(CreditCardFIDOAuthenticatorTest,
                           ParseAttestationResponse);

  // Invokes the WebAuthn prompt to request user verification to sign the
  // challenge in |request_options|.
  virtual void GetAssertion(
      PublicKeyCredentialRequestOptionsPtr request_options);

  // Invokes the WebAuthn prompt to request user verification to sign the
  // challenge in |creation_options| and create a key-pair.
  virtual void MakeCredential(
      PublicKeyCredentialCreationOptionsPtr creation_options);

  // Makes a request to payments to either opt-in or opt-out the user.
  void OptChange(bool opt_in, base::Value attestation_response = base::Value());

  // The callback invoked from the WebAuthn prompt including the
  // |assertion_response|, which will be sent to Google Payments to retrieve
  // card details.
  void OnDidGetAssertion(
      AuthenticatorStatus status,
      GetAssertionAuthenticatorResponsePtr assertion_response);

  // The callback invoked from the WebAuthn prompt including the
  // |attestation_response|, which will be sent to Google Payments to enroll the
  // credential for this user.
  void OnDidMakeCredential(
      AuthenticatorStatus status,
      MakeCredentialAuthenticatorResponsePtr attestation_response);

  // Sets prefstore to enable credit card authentication if rpc was successful.
  void OnDidGetOptChangeResult(AutofillClient::PaymentsRpcResult result,
                               bool user_is_opted_in,
                               base::Value creation_options = base::Value());

  // The callback invoked from the WebAuthn offer dialog when it is accepted or
  // declined/cancelled.
  void OnWebauthnOfferDialogUserResponse(bool did_accept);

  // payments::FullCardRequest::ResultDelegate:
  void OnFullCardRequestSucceeded(
      const payments::FullCardRequest& full_card_request,
      const CreditCard& card,
      const base::string16& cvc) override;
  void OnFullCardRequestFailed() override;

  // Converts |request_options| from JSON to mojom pointer.
  PublicKeyCredentialRequestOptionsPtr ParseRequestOptions(
      const base::Value& request_options);

  // Converts |creation_options| from JSON to mojom pointer.
  PublicKeyCredentialCreationOptionsPtr ParseCreationOptions(
      const base::Value& creation_options);

  // Helper function to parse |key_info| sub-dictionary found in
  // |request_options| and |creation_options|.
  PublicKeyCredentialDescriptor ParseCredentialDescriptor(
      const base::Value& key_info);

  // Converts |assertion_response| from mojom pointer to JSON.
  base::Value ParseAssertionResponse(
      GetAssertionAuthenticatorResponsePtr assertion_response);

  // Converts |attestation_response| from mojom pointer to JSON.
  base::Value ParseAttestationResponse(
      MakeCredentialAuthenticatorResponsePtr attestation_response);

  // Returns true if |request_options| contains a challenge and has a non-empty
  // list of keys that each have a Credential ID.
  bool IsValidRequestOptions(const base::Value& request_options);

  // Returns true if |request_options| contains a challenge.
  bool IsValidCreationOptions(const base::Value& creation_options);

  // Sets the value for |user_is_verifiable_|.
  void SetUserIsVerifiable(bool user_is_verifiable);

  // Card being unmasked.
  const CreditCard* card_;

  // The current flow in progress.
  Flow current_flow_ = NONE_FLOW;

  // Meant for histograms recorded in FullCardRequest.
  base::TimeTicks form_parsed_timestamp_;

  // The associated autofill driver. Weak reference.
  AutofillDriver* const autofill_driver_;

  // The associated autofill client. Weak reference.
  AutofillClient* const autofill_client_;

  // Payments client to make requests to Google Payments.
  payments::PaymentsClient* const payments_client_;

  // Authenticator pointer to facilitate WebAuthn.
  mojo::Remote<InternalAuthenticator> authenticator_;

  // Responsible for getting the full card details, including the PAN and the
  // CVC.
  std::unique_ptr<payments::FullCardRequest> full_card_request_;

  // Weak pointer to object that is requesting authentication.
  base::WeakPtr<Requester> requester_;

  // Set when callback for IsUserVerifiable() is invoked with passed value.
  base::Optional<bool> user_is_verifiable_ = base::nullopt;

  // Signaled when callback for IsUserVerifiable() is invoked.
  base::WaitableEvent user_is_verifiable_callback_received_;

  base::WeakPtrFactory<CreditCardFIDOAuthenticator> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CreditCardFIDOAuthenticator);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_FIDO_AUTHENTICATOR_H_
