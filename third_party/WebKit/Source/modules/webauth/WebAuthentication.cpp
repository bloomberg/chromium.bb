// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webauth/WebAuthentication.h"

#include "bindings/core/v8/ArrayBufferOrArrayBufferView.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/LocalFrame.h"
#include "core/typed_arrays/DOMArrayBuffer.h"
#include "modules/webauth/AuthenticatorAttestationResponse.h"
#include "modules/webauth/MakeCredentialOptions.h"
#include "public/platform/InterfaceProvider.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace blink {
typedef ArrayBufferOrArrayBufferView BufferSource;
}  // namespace blink

namespace {
constexpr char kNoAuthenticatorError[] = "Authenticator unavailable.";
// Time to wait for an authenticator to successfully complete an operation.
constexpr WTF::TimeDelta kAdjustedTimeoutLower = WTF::TimeDelta::FromMinutes(1);
constexpr WTF::TimeDelta kAdjustedTimeoutUpper = WTF::TimeDelta::FromMinutes(2);
}  // anonymous namespace

namespace mojo {
using webauth::mojom::blink::AuthenticatorStatus;
using webauth::mojom::blink::MakeCredentialOptionsPtr;
using webauth::mojom::blink::PublicKeyCredentialEntityPtr;
using webauth::mojom::blink::PublicKeyCredentialParametersPtr;
using webauth::mojom::blink::PublicKeyCredentialType;
using webauth::mojom::blink::AuthenticatorTransport;

// TODO(kpaulhamus): Make this a TypeConverter
Vector<uint8_t> ConvertBufferSource(const blink::BufferSource& buffer) {
  DCHECK(!buffer.isNull());
  Vector<uint8_t> vector;
  if (buffer.isArrayBuffer()) {
    vector.Append(static_cast<uint8_t*>(buffer.getAsArrayBuffer()->Data()),
                  buffer.getAsArrayBuffer()->ByteLength());
  } else {
    DCHECK(buffer.isArrayBufferView());
    vector.Append(static_cast<uint8_t*>(
                      buffer.getAsArrayBufferView().View()->BaseAddress()),
                  buffer.getAsArrayBufferView().View()->byteLength());
  }
  return vector;
}

// TODO(kpaulhamus): Make this a TypeConverter
PublicKeyCredentialType ConvertPublicKeyCredentialType(const String& type) {
  if (type == "public-key")
    return PublicKeyCredentialType::PUBLIC_KEY;
  NOTREACHED();
  return PublicKeyCredentialType::PUBLIC_KEY;
}

// TODO(kpaulhamus): Make this a TypeConverter
AuthenticatorTransport ConvertTransport(const String& transport) {
  if (transport == "usb")
    return AuthenticatorTransport::USB;
  if (transport == "nfc")
    return AuthenticatorTransport::NFC;
  if (transport == "ble")
    return AuthenticatorTransport::BLE;
  NOTREACHED();
  return AuthenticatorTransport::USB;
}

// TODO(kpaulhamus): Make this a TypeConverter
PublicKeyCredentialEntityPtr ConvertPublicKeyCredentialUserEntity(
    const blink::PublicKeyCredentialUserEntity& user) {
  auto entity = webauth::mojom::blink::PublicKeyCredentialEntity::New();
  entity->id = user.id();
  entity->name = user.name();
  if (user.hasIcon()) {
    entity->icon = blink::KURL(blink::KURL(), user.icon());
  }
  entity->display_name = user.displayName();
  return entity;
}

// TODO(kpaulhamus): Make this a TypeConverter
PublicKeyCredentialEntityPtr ConvertPublicKeyCredentialEntity(
    const blink::PublicKeyCredentialEntity& rp) {
  auto entity = webauth::mojom::blink::PublicKeyCredentialEntity::New();
  entity->id = rp.id();
  entity->name = rp.name();
  if (rp.hasIcon()) {
    entity->icon = blink::KURL(blink::KURL(), rp.icon());
  }
  return entity;
}

// TODO(kpaulhamus): Make this a TypeConverter
PublicKeyCredentialParametersPtr ConvertPublicKeyCredentialParameters(
    const blink::PublicKeyCredentialParameters parameter) {
  auto mojo_parameter =
      webauth::mojom::blink::PublicKeyCredentialParameters::New();
  mojo_parameter->type = ConvertPublicKeyCredentialType(parameter.type());
  // TODO(kpaulhamus): add AlgorithmIdentifier
  return mojo_parameter;
}

// TODO(kpaulhamus): Make this a TypeConverter
MakeCredentialOptionsPtr ConvertMakeCredentialOptions(
    const blink::MakeCredentialOptions options) {
  auto mojo_options = webauth::mojom::blink::MakeCredentialOptions::New();
  mojo_options->relying_party = ConvertPublicKeyCredentialEntity(options.rp());
  mojo_options->user = ConvertPublicKeyCredentialUserEntity(options.user());
  mojo_options->challenge = ConvertBufferSource(options.challenge());

  Vector<webauth::mojom::blink::PublicKeyCredentialParametersPtr> parameters;
  for (const auto& parameter : options.parameters()) {
    parameters.push_back(ConvertPublicKeyCredentialParameters(parameter));
  }
  mojo_options->crypto_parameters = std::move(parameters);

  // Step 4 of https://w3c.github.io/webauthn/#createCredential
  WTF::TimeDelta adjusted_timeout;
  if (options.hasTimeout()) {
    adjusted_timeout = WTF::TimeDelta::FromMilliseconds(options.timeout());
  } else {
    adjusted_timeout = kAdjustedTimeoutLower;
  }

  mojo_options->adjusted_timeout = std::max(
      kAdjustedTimeoutLower, std::min(kAdjustedTimeoutUpper, adjusted_timeout));

  if (options.hasExcludeCredentials()) {
    // Adds the excludeCredentials members
    // (which are PublicKeyCredentialDescriptors)
    for (const auto& descriptor : options.excludeCredentials()) {
      auto mojo_descriptor =
          webauth::mojom::blink::PublicKeyCredentialDescriptor::New();
      mojo_descriptor->type = ConvertPublicKeyCredentialType(descriptor.type());
      mojo_descriptor->id = ConvertBufferSource(descriptor.id());
      for (const auto& transport : descriptor.transports())
        mojo_descriptor->transports.push_back(ConvertTransport(transport));
      mojo_options->exclude_credentials.push_back(std::move(mojo_descriptor));
    }
  }
  return mojo_options;
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
    : ContextLifecycleObserver(frame.GetDocument()) {
  frame.GetInterfaceProvider().GetInterface(mojo::MakeRequest(&authenticator_));
  authenticator_.set_connection_error_handler(ConvertToBaseCallback(
      WTF::Bind(&WebAuthentication::OnAuthenticatorConnectionError,
                WrapWeakPersistent(this))));
}

WebAuthentication::~WebAuthentication() {
  // |authenticator_| may still be valid but there should be no more
  // outstanding requests because each holds a persistent handle to this object.
  DCHECK(authenticator_requests_.IsEmpty());
}

ScriptPromise WebAuthentication::makeCredential(
    ScriptState* script_state,
    const MakeCredentialOptions& publicKey) {
  ScriptPromise promise = RejectIfNotSupported(script_state);
  if (!promise.IsEmpty())
    return promise;

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);

  auto options = mojo::ConvertMakeCredentialOptions(publicKey);
  authenticator_requests_.insert(resolver);
  authenticator_->MakeCredential(
      std::move(options), ConvertToBaseCallback(WTF::Bind(
                              &WebAuthentication::OnMakeCredential,
                              WrapPersistent(this), WrapPersistent(resolver))));
  return resolver->Promise();
}

ScriptPromise WebAuthentication::getAssertion(
    ScriptState* script_state,
    const PublicKeyCredentialRequestOptions& publicKey) {
  NOTREACHED();
  return ScriptPromise();
}

void WebAuthentication::ContextDestroyed(ExecutionContext*) {
  Cleanup();
}

void WebAuthentication::OnMakeCredential(
    ScriptPromiseResolver* resolver,
    webauth::mojom::blink::AuthenticatorStatus status,
    webauth::mojom::blink::PublicKeyCredentialInfoPtr credential) {
  if (!MarkRequestComplete(resolver))
    return;

  DOMException* error = mojo::CreateExceptionFromStatus(status);
  if (error) {
    DCHECK(!credential);
    resolver->Reject(error);
    Cleanup();
    return;
  }

  if (credential->client_data_json.IsEmpty() ||
      credential->response->attestation_object.IsEmpty()) {
    resolver->Reject(
        DOMException::Create(kNotFoundError, "No credentials returned."));
  }

  DOMArrayBuffer* client_data_buffer = DOMArrayBuffer::Create(
      static_cast<void*>(&credential->client_data_json.front()),
      credential->client_data_json.size());

  // Return AuthenticatorAttestationResponse
  DOMArrayBuffer* attestation_buffer = DOMArrayBuffer::Create(
      static_cast<void*>(&credential->response->attestation_object.front()),
      credential->response->attestation_object.size());

  AuthenticatorAttestationResponse* attestation_response =
      AuthenticatorAttestationResponse::Create(client_data_buffer,
                                               attestation_buffer);
  resolver->Resolve(attestation_response);
}

ScriptPromise WebAuthentication::RejectIfNotSupported(
    ScriptState* script_state) {
  LocalFrame* frame =
      ToDocument(ExecutionContext::From(script_state))->GetFrame();
  if (!authenticator_) {
    if (!frame) {
      return ScriptPromise::RejectWithDOMException(
          script_state, DOMException::Create(kNotSupportedError));
    }
    frame->GetInterfaceProvider().GetInterface(
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
