// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webauth/WebAuthentication.h"

#include <stdint.h>

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/LocalFrame.h"
#include "modules/webauth/RelyingPartyAccount.h"
#include "modules/webauth/ScopedCredential.h"
#include "modules/webauth/ScopedCredentialOptions.h"
#include "modules/webauth/ScopedCredentialParameters.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace {
const char kNoAuthenticatorError[] = "Authenticator unavailable.";
// Time to wait for an authenticator to successfully complete an operation.
const int kAdjustedTimeoutLowerInSeconds = 60;
const int kAdjustedTimeoutUpperInSeconds = 120;
}  // anonymous namespace

namespace mojo {
using webauth::mojom::blink::RelyingPartyAccount;
using webauth::mojom::blink::RelyingPartyAccountPtr;
using webauth::mojom::blink::AuthenticatorStatus;
using webauth::mojom::blink::ScopedCredentialDescriptor;
using webauth::mojom::blink::ScopedCredentialOptions;
using webauth::mojom::blink::ScopedCredentialOptionsPtr;
using webauth::mojom::blink::ScopedCredentialParameters;
using webauth::mojom::blink::ScopedCredentialParametersPtr;
using webauth::mojom::blink::ScopedCredentialType;
using webauth::mojom::blink::Transport;

// TODO(kpaulhamus): Make this a TypeConverter
Vector<uint8_t> ConvertBufferSource(const blink::BufferSource& buffer) {
  DCHECK(!buffer.isNull());
  Vector<uint8_t> vector;
  if (buffer.isArrayBuffer()) {
    vector.Append(static_cast<uint8_t*>(buffer.getAsArrayBuffer()->Data()),
                  buffer.getAsArrayBuffer()->ByteLength());
  } else {
    vector.Append(static_cast<uint8_t*>(
                      buffer.getAsArrayBufferView().View()->BaseAddress()),
                  buffer.getAsArrayBufferView().View()->byteLength());
  }
  return vector;
}

// TODO(kpaulhamus): Make this a TypeConverter
ScopedCredentialType ConvertScopedCredentialType(const String& cred_type) {
  if (cred_type == "ScopedCred")
    return ScopedCredentialType::SCOPEDCRED;
  NOTREACHED();
  return ScopedCredentialType::SCOPEDCRED;
}

// TODO(kpaulhamus): Make this a TypeConverter
Transport ConvertTransport(const String& transport) {
  if (transport == "usb")
    return Transport::USB;
  if (transport == "nfc")
    return Transport::NFC;
  if (transport == "ble")
    return Transport::BLE;
  NOTREACHED();
  return Transport::USB;
}

// TODO(kpaulhamus): Make this a TypeConverter
RelyingPartyAccountPtr ConvertRelyingPartyAccount(
    const blink::RelyingPartyAccount& account_information,
    blink::ScriptPromiseResolver* resolver) {
  auto mojo_account = RelyingPartyAccount::New();

  mojo_account->relying_party_display_name =
      account_information.rpDisplayName();
  mojo_account->display_name = account_information.displayName();
  mojo_account->id = account_information.id();
  mojo_account->name = account_information.name();
  mojo_account->image_url = account_information.imageURL();
  return mojo_account;
}

// TODO(kpaulhamus): Make this a TypeConverter
ScopedCredentialOptionsPtr ConvertScopedCredentialOptions(
    const blink::ScopedCredentialOptions options,
    blink::ScriptPromiseResolver* resolver) {
  auto mojo_options = ScopedCredentialOptions::New();
  if (options.hasRpId()) {
    // if rpID is missing, it will later be set to the origin of the page
    // in the secure browser process.
    mojo_options->relying_party_id = options.rpId();
  }

  // Step 4 of https://w3c.github.io/webauthn/#createCredential
  int predicted_timeout = kAdjustedTimeoutLowerInSeconds;
  if (options.hasTimeoutSeconds()) {
    predicted_timeout = static_cast<int>(options.timeoutSeconds());
  }

  mojo_options->adjusted_timeout = static_cast<double>(
      std::max(kAdjustedTimeoutLowerInSeconds,
               std::min(kAdjustedTimeoutUpperInSeconds, predicted_timeout)));

  if (options.hasExcludeList()) {
    // Adds the excludeList members (which are ScopedCredentialDescriptors)
    for (const auto& descriptor : options.excludeList()) {
      auto mojo_descriptor = ScopedCredentialDescriptor::New();
      mojo_descriptor->type = ConvertScopedCredentialType(descriptor.type());
      mojo_descriptor->id = ConvertBufferSource(descriptor.id());
      for (const auto& transport : descriptor.transports())
        mojo_descriptor->transports.push_back(ConvertTransport(transport));
      mojo_options->exclude_list.push_back(std::move(mojo_descriptor));
    }
  }
  // TODO(kpaulhamus): add AuthenticationExtensions;
  return mojo_options;
}

// TODO(kpaulhamus): Make this a TypeConverter
ScopedCredentialParametersPtr ConvertScopedCredentialParameter(
    const blink::ScopedCredentialParameters parameter,
    blink::ScriptPromiseResolver* resolver) {
  auto mojo_parameter = ScopedCredentialParameters::New();
  mojo_parameter->type = ConvertScopedCredentialType(parameter.type());
  // TODO(kpaulhamus): add AlgorithmIdentifier
  return mojo_parameter;
}

blink::DOMException* CreateExceptionFromStatus(AuthenticatorStatus status) {
  switch (status) {
    case AuthenticatorStatus::NOT_IMPLEMENTED:
      return blink::DOMException::Create(blink::kNotSupportedError,
                                         "Not implemented.");
    case AuthenticatorStatus::NOT_ALLOWED_ERROR:
      return blink::DOMException::Create(blink::kNotAllowedError,
                                         "Not allowed.");
    case AuthenticatorStatus::NOT_SUPPORTED_ERROR:
      return blink::DOMException::Create(
          blink::kNotSupportedError,
          "Parameters for this operation are not supported.");
    case AuthenticatorStatus::SECURITY_ERROR:
      return blink::DOMException::Create(blink::kSecurityError,
                                         "The operation was not allowed.");
    case AuthenticatorStatus::UNKNOWN_ERROR:
      return blink::DOMException::Create(blink::kUnknownError,
                                         "Request failed.");
    case AuthenticatorStatus::CANCELLED:
      return blink::DOMException::Create(blink::kNotAllowedError,
                                         "User canceled the operation.");
    case AuthenticatorStatus::SUCCESS:
      return nullptr;
    default:
      NOTREACHED();
      return nullptr;
  }
}
}  // namespace mojo

namespace blink {
WebAuthentication::WebAuthentication(LocalFrame& frame)
    : ContextLifecycleObserver(frame.GetDocument()) {}

WebAuthentication::~WebAuthentication() {
  // |authenticator_| may still be valid but there should be no more
  // outstanding requests because each holds a persistent handle to this object.
  DCHECK(authenticator_requests_.IsEmpty());
}

ScriptPromise WebAuthentication::makeCredential(
    ScriptState* script_state,
    const RelyingPartyAccount& account_information,
    const HeapVector<ScopedCredentialParameters> crypto_parameters,
    const BufferSource& attestation_challenge,
    ScopedCredentialOptions& options) {
  ScriptPromise promise = RejectIfNotSupported(script_state);
  if (!promise.IsEmpty())
    return promise;

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);

  Vector<uint8_t> buffer = mojo::ConvertBufferSource(attestation_challenge);
  auto opts = mojo::ConvertScopedCredentialOptions(options, resolver);
  Vector<webauth::mojom::blink::ScopedCredentialParametersPtr> parameters;
  for (const auto& parameter : crypto_parameters) {
    if (parameter.hasType()) {
      parameters.push_back(
          mojo::ConvertScopedCredentialParameter(parameter, resolver));
    }
  }
  auto account =
      mojo::ConvertRelyingPartyAccount(account_information, resolver);
  authenticator_requests_.insert(resolver);
  authenticator_->MakeCredential(
      std::move(account), std::move(parameters), buffer, std::move(opts),
      ConvertToBaseCallback(WTF::Bind(&WebAuthentication::OnMakeCredential,
                                      WrapPersistent(this),
                                      WrapPersistent(resolver))));
  return resolver->Promise();
}

ScriptPromise WebAuthentication::getAssertion(
    ScriptState* script_state,
    const BufferSource& assertion_challenge,
    const AuthenticationAssertionOptions& options) {
  NOTREACHED();
  return ScriptPromise();
}

void WebAuthentication::ContextDestroyed(ExecutionContext*) {
  Cleanup();
}

void WebAuthentication::OnMakeCredential(
    ScriptPromiseResolver* resolver,
    webauth::mojom::blink::AuthenticatorStatus status,
    webauth::mojom::blink::ScopedCredentialInfoPtr credential) {
  if (!MarkRequestComplete(resolver))
    return;

  DOMException* error = mojo::CreateExceptionFromStatus(status);
  if (error) {
    DCHECK(!credential);
    resolver->Reject(error);
    Cleanup();
    return;
  }

  if (credential->client_data.IsEmpty() || credential->attestation.IsEmpty()) {
    resolver->Reject(
        DOMException::Create(kNotFoundError, "No credential returned."));
    return;
  }

  DOMArrayBuffer* clientDataBuffer = DOMArrayBuffer::Create(
      static_cast<void*>(&credential->client_data.front()),
      credential->client_data.size());

  DOMArrayBuffer* attestationBuffer = DOMArrayBuffer::Create(
      static_cast<void*>(&credential->attestation.front()),
      credential->attestation.size());

  ScopedCredentialInfo* scopedCredential =
      ScopedCredentialInfo::Create(clientDataBuffer, attestationBuffer);
  resolver->Resolve(scopedCredential);
}

ScriptPromise WebAuthentication::RejectIfNotSupported(
    ScriptState* script_state) {
  if (!authenticator_) {
    if (!GetFrame()) {
      return ScriptPromise::RejectWithDOMException(
          script_state, DOMException::Create(kNotSupportedError));
    }
    GetFrame()->GetInterfaceProvider().GetInterface(
        mojo::MakeRequest(&authenticator_));

    authenticator_.set_connection_error_handler(ConvertToBaseCallback(
        WTF::Bind(&WebAuthentication::OnAuthenticatorConnectionError,
                  WrapWeakPersistent(this))));
  }
  return ScriptPromise();
}

void WebAuthentication::OnAuthenticatorConnectionError() {
  for (ScriptPromiseResolver* resolver : authenticator_requests_) {
    resolver->Reject(
        DOMException::Create(kNotFoundError, kNoAuthenticatorError));
  }
  Cleanup();
}

bool WebAuthentication::MarkRequestComplete(ScriptPromiseResolver* resolver) {
  auto request_entry = authenticator_requests_.find(resolver);
  if (request_entry == authenticator_requests_.end())
    return false;
  authenticator_requests_.erase(request_entry);
  return true;
}

DEFINE_TRACE(WebAuthentication) {
  visitor->Trace(authenticator_requests_);
  ContextLifecycleObserver::Trace(visitor);
}

// Clears the promise resolver, timer, and closes the Mojo connection.
void WebAuthentication::Cleanup() {
  authenticator_.reset();
  authenticator_requests_.clear();
}

}  // namespace blink
