// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_FIDO_AUTHENTICATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_FIDO_AUTHENTICATOR_H_

#include <memory>

#include "base/strings/string16.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/fido_authentication_strike_database.h"
#include "components/autofill/core/browser/payments/full_card_request.h"
#include "components/autofill/core/browser/payments/internal_authenticator.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "device/fido/fido_constants.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"

namespace autofill {

using blink::mojom::AuthenticatorStatus;
using blink::mojom::GetAssertionAuthenticatorResponse;
using blink::mojom::GetAssertionAuthenticatorResponsePtr;
using blink::mojom::MakeCredentialAuthenticatorResponse;
using blink::mojom::MakeCredentialAuthenticatorResponsePtr;
using blink::mojom::PublicKeyCredentialCreationOptions;
using blink::mojom::PublicKeyCredentialCreationOptionsPtr;
using blink::mojom::PublicKeyCredentialRequestOptions;
using blink::mojom::PublicKeyCredentialRequestOptionsPtr;
using device::AttestationConveyancePreference;
using device::AuthenticatorAttachment;
using device::AuthenticatorSelectionCriteria;
using device::CredentialType;
using device::FidoTransportProtocol;
using device::PublicKeyCredentialDescriptor;
using device::UserVerificationRequirement;

// Enum denotes user's intention to opt in/out.
enum class UserOptInIntention {
  // Unspecified intention. No pref mismatch.
  kUnspecified = 0,
  // Only used for Android settings page. Local pref is opted in but Payments
  // considers the user not opted-in.
  kIntentToOptIn = 1,
  // User intends to opt out, happens when user opted out from settings page on
  // Android, or opt-out call failed on Desktop.
  kIntentToOptOut = 2,
};

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
    OPT_IN_FETCH_CHALLENGE_FLOW,
    // Opt-out flow.
    OPT_OUT_FLOW,
    // Authorization of a new card.
    FOLLOWUP_AFTER_CVC_AUTH_FLOW,
  };
  class Requester {
   public:
    virtual ~Requester() {}
    virtual void OnFIDOAuthenticationComplete(
        bool did_succeed,
        const CreditCard* card = nullptr,
        const base::string16& cvc = base::string16()) = 0;
    virtual void OnFidoAuthorizationComplete(bool did_succeed) = 0;
  };
  CreditCardFIDOAuthenticator(AutofillDriver* driver, AutofillClient* client);
  ~CreditCardFIDOAuthenticator() override;

  // Invokes Authentication flow. Responds to |accessor_| with full pan.
  void Authenticate(const CreditCard* card,
                    base::WeakPtr<Requester> requester,
                    base::TimeTicks form_parsed_timestamp,
                    base::Value request_options);

  // Invokes Registration flow. Sends credentials created from
  // |creation_options| along with the |card_authorization_token| to Payments in
  // order to enroll the user and authorize the corresponding card.
  void Register(std::string card_authorization_token = std::string(),
                base::Value creation_options = base::Value());

  // Invokes an Authorization flow. Sends signature created from
  // |request_options| along with the |card_authorization_token| to Payments in
  // order to authorize the corresponding card. Notifies |requester| once
  // Authorization is complete.
  void Authorize(base::WeakPtr<Requester> requester,
                 std::string card_authorization_token,
                 base::Value request_options);

  // Opts the user out.
  virtual void OptOut();

  // Invokes callback with true if user has a verifying platform authenticator.
  // e.g. Touch/Face ID, Windows Hello, Android fingerprint, etc., is available
  // and enabled. Otherwise invokes callback with false.
  virtual void IsUserVerifiable(base::OnceCallback<void(bool)> callback);

  // Returns true only if the user has opted-in to use WebAuthn for autofill.
  virtual bool IsUserOptedIn();

  // Return user's opt in/out intention based on unmask detail response and
  // local pref.
  UserOptInIntention GetUserOptInIntention(
      payments::PaymentsClient::UnmaskDetails& unmask_details);

  // Cancel the ongoing verification process. Used to reset states in this class
  // and in the FullCardRequest if any.
  void CancelVerification();

#if !defined(OS_ANDROID)
  // Invoked when a Webauthn offer dialog is about to be shown.
  void OnWebauthnOfferDialogRequested(std::string card_authorization_token);

  // Invoked when the WebAuthn offer dialog is accepted or declined/cancelled.
  void OnWebauthnOfferDialogUserResponse(bool did_accept);
#endif

  // Retrieves the strike database for offering FIDO authentication.
  FidoAuthenticationStrikeDatabase*
  GetOrCreateFidoAuthenticationStrikeDatabase();

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
  void OptChange(base::Value authenticator_response = base::Value());

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
  void OnDidGetOptChangeResult(
      AutofillClient::PaymentsRpcResult result,
      payments::PaymentsClient::OptChangeResponseDetails& response);

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

  // Logs the result of a WebAuthn prompt.
  void LogWebauthnResult(AuthenticatorStatus status);

  // Updates the user preference to the value of |user_is_opted_in_|.
  void UpdateUserPref();

  // Gets or creates Authenticator pointer to facilitate WebAuthn.
  InternalAuthenticator* authenticator();

  // Card being unmasked.
  const CreditCard* card_;

  // The current flow in progress.
  Flow current_flow_ = NONE_FLOW;

  // Token used for authorizing new cards. Helps tie CVC auth and FIDO calls
  // together in order to support FIDO-only unmasking on future attempts.
  std::string card_authorization_token_;

  // Meant for histograms recorded in FullCardRequest.
  base::TimeTicks form_parsed_timestamp_;

  // The associated autofill driver. Weak reference.
  AutofillDriver* const autofill_driver_;

  // The associated autofill client. Weak reference.
  AutofillClient* const autofill_client_;

  // Payments client to make requests to Google Payments.
  payments::PaymentsClient* const payments_client_;

  // Authenticator pointer to facilitate WebAuthn.
  InternalAuthenticator* authenticator_ = nullptr;

  // Responsible for getting the full card details, including the PAN and the
  // CVC.
  std::unique_ptr<payments::FullCardRequest> full_card_request_;

  // Weak pointer to object that is requesting authentication.
  base::WeakPtr<Requester> requester_;

  // Is set to true when user is opted-in, else false. This value will always
  // override the value in the pref store in the case of any discrepancies.
  bool user_is_opted_in_;

  // Strike database to ensure we limit the number of times we offer fido
  // authentication.
  std::unique_ptr<FidoAuthenticationStrikeDatabase>
      fido_authentication_strike_database_;

  // Signaled when callback for IsUserVerifiable() is invoked.
  base::WaitableEvent user_is_verifiable_callback_received_;

  base::WeakPtrFactory<CreditCardFIDOAuthenticator> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CreditCardFIDOAuthenticator);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_FIDO_AUTHENTICATOR_H_
