// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/credentialmanager/CredentialsContainer.h"

#include <memory>
#include <utility>

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/Frame.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/page/FrameTree.h"
#include "core/typed_arrays/DOMArrayBuffer.h"
#include "modules/credentialmanager/AuthenticatorAttestationResponse.h"
#include "modules/credentialmanager/AuthenticatorResponse.h"
#include "modules/credentialmanager/Credential.h"
#include "modules/credentialmanager/CredentialCreationOptions.h"
#include "modules/credentialmanager/CredentialManagerProxy.h"
#include "modules/credentialmanager/CredentialManagerTypeConverters.h"
#include "modules/credentialmanager/CredentialRequestOptions.h"
#include "modules/credentialmanager/FederatedCredential.h"
#include "modules/credentialmanager/FederatedCredentialRequestOptions.h"
#include "modules/credentialmanager/MakePublicKeyCredentialOptions.h"
#include "modules/credentialmanager/PasswordCredential.h"
#include "modules/credentialmanager/PublicKeyCredential.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/Functional.h"
#include "third_party/WebKit/public/platform/modules/credentialmanager/credential_manager.mojom-blink.h"

namespace blink {

namespace {

using ::password_manager::mojom::blink::CredentialManagerError;
using ::password_manager::mojom::blink::CredentialInfo;
using ::password_manager::mojom::blink::CredentialInfoPtr;
using ::password_manager::mojom::blink::CredentialMediationRequirement;
using ::webauth::mojom::blink::AuthenticatorStatus;
using MojoMakePublicKeyCredentialOptions =
    ::webauth::mojom::blink::MakePublicKeyCredentialOptions;
using ::webauth::mojom::blink::PublicKeyCredentialInfoPtr;

enum class RequiredOriginType { kSecure, kSecureAndSameWithAncestors };

// Off-heap wrapper that holds a strong reference to a ScriptPromiseResolver.
class ScopedPromiseResolver {
  WTF_MAKE_NONCOPYABLE(ScopedPromiseResolver);

 public:
  explicit ScopedPromiseResolver(ScriptPromiseResolver* resolver)
      : resolver_(resolver) {}

  ~ScopedPromiseResolver() {
    if (resolver_)
      OnConnectionError();
  }

  // Releases the owned |resolver_|. This is to be called by the Mojo response
  // callback responsible for resolving the corresponding ScriptPromise.
  //
  // If this method is not called before |this| goes of scope, it is assumed
  // that a Mojo connection error has occurred, and the response callback was
  // never invoked. The Promise will be rejected with an appropriate exception.
  ScriptPromiseResolver* Release() { return resolver_.Release(); }

 private:
  void OnConnectionError() {
    // The only anticapted reason for a connection error is that the embedder
    // does not implement mojom::CredentialManager, so go out on a limb and try
    // to provide an actionable error message.
    resolver_->Reject(DOMException::Create(
        kNotSupportedError,
        "The user agent does not implement a password store."));
  }

  Persistent<ScriptPromiseResolver> resolver_;
};

bool IsSameOriginWithAncestors(const Frame* frame) {
  DCHECK(frame);
  const Frame* current = frame;
  const SecurityOrigin* origin =
      frame->GetSecurityContext()->GetSecurityOrigin();
  while (current->Tree().Parent()) {
    current = current->Tree().Parent();
    if (!origin->CanAccess(current->GetSecurityContext()->GetSecurityOrigin()))
      return false;
  }
  return true;
}

bool CheckSecurityRequirementsBeforeRequest(
    ScriptPromiseResolver* resolver,
    RequiredOriginType required_origin_type) {
  // Credential Management is not exposed to Workers or Worklets, so the current
  // realm execution context must have a responsible browsing context.
  SECURITY_CHECK(resolver->GetFrame());

  String error_message;
  if (!resolver->GetExecutionContext()->IsSecureContext(error_message)) {
    resolver->Reject(DOMException::Create(kSecurityError, error_message));
    return false;
  }

  if (required_origin_type == RequiredOriginType::kSecureAndSameWithAncestors &&
      !IsSameOriginWithAncestors(resolver->GetFrame())) {
    resolver->Reject(DOMException::Create(
        kNotAllowedError,
        "`PasswordCredential` and `FederatedCredential` objects may only be "
        "stored/retrieved in a document which is same-origin with all of its "
        "ancestors."));
    return false;
  }

  return true;
}

void AssertSecurityRequirementsBeforeResponse(
    ScriptPromiseResolver* resolver,
    RequiredOriginType require_origin) {
  // The |resolver| will blanket ignore Reject/Resolve calls if the context is
  // gone -- nevertheless, call Reject() to be on the safe side.
  if (!resolver->GetExecutionContext()) {
    resolver->Reject();
    return;
  }

  SECURITY_CHECK(resolver->GetFrame());
  SECURITY_CHECK(resolver->GetExecutionContext()->IsSecureContext());
  SECURITY_CHECK(require_origin !=
                     RequiredOriginType::kSecureAndSameWithAncestors ||
                 IsSameOriginWithAncestors(resolver->GetFrame()));
}

// Checks if the icon URL of |credential| is an a-priori authenticated URL.
// https://w3c.github.io/webappsec-credential-management/#dom-credentialuserdata-iconurl
bool IsIconURLEmptyOrSecure(const Credential* credential) {
  if (!credential->IsPasswordCredential() &&
      !credential->IsFederatedCredential()) {
    DCHECK(credential->IsPublicKeyCredential());
    return true;
  }

  const KURL& url =
      credential->IsFederatedCredential()
          ? static_cast<const FederatedCredential*>(credential)->iconURL()
          : static_cast<const PasswordCredential*>(credential)->iconURL();
  if (url.IsEmpty())
    return true;

  // https://www.w3.org/TR/mixed-content/#a-priori-authenticated-url
  return url.IsAboutSrcdocURL() || url.IsAboutBlankURL() ||
         url.ProtocolIsData() ||
         SecurityOrigin::Create(url)->IsPotentiallyTrustworthy();
}

DOMException* CredentialManagerErrorToDOMException(
    CredentialManagerError reason) {
  switch (reason) {
    case CredentialManagerError::PENDING_REQUEST:
      return DOMException::Create(kInvalidStateError,
                                  "A request is already pending.");
    case CredentialManagerError::PASSWORD_STORE_UNAVAILABLE:
      return DOMException::Create(kNotSupportedError,
                                  "The password store is unavailable.");
    case CredentialManagerError::NOT_ALLOWED:
      return DOMException::Create(kNotAllowedError,
                                  "The operation is not allowed.");
    case CredentialManagerError::NOT_SUPPORTED:
      return DOMException::Create(
          kNotSupportedError,
          "Parameters for this operation are not supported.");
    case CredentialManagerError::UNKNOWN:
      return DOMException::Create(kNotReadableError,
                                  "An unknown error occurred while talking "
                                  "to the credential manager.");
    case CredentialManagerError::SUCCESS:
      NOTREACHED();
      break;
  }
  return nullptr;
}

void OnStoreComplete(std::unique_ptr<ScopedPromiseResolver> scoped_resolver,
                     RequiredOriginType required_origin_type) {
  auto* resolver = scoped_resolver->Release();
  AssertSecurityRequirementsBeforeResponse(resolver, required_origin_type);
  resolver->Resolve();
}

void OnPreventSilentAccessComplete(
    std::unique_ptr<ScopedPromiseResolver> scoped_resolver) {
  auto* resolver = scoped_resolver->Release();
  const auto required_origin_type = RequiredOriginType::kSecure;
  AssertSecurityRequirementsBeforeResponse(resolver, required_origin_type);

  resolver->Resolve();
}

void OnGetComplete(std::unique_ptr<ScopedPromiseResolver> scoped_resolver,
                   RequiredOriginType required_origin_type,
                   CredentialManagerError error,
                   CredentialInfoPtr credential_info) {
  auto* resolver = scoped_resolver->Release();

  AssertSecurityRequirementsBeforeResponse(resolver, required_origin_type);
  if (error == CredentialManagerError::SUCCESS) {
    DCHECK(credential_info);
    UseCounter::Count(resolver->GetExecutionContext(),
                      WebFeature::kCredentialManagerGetReturnedCredential);
    resolver->Resolve(mojo::ConvertTo<Credential*>(std::move(credential_info)));
  } else {
    DCHECK(!credential_info);
    resolver->Reject(CredentialManagerErrorToDOMException(error));
  }
}

DOMArrayBuffer* VectorToDOMArrayBuffer(const Vector<uint8_t> buffer) {
  return DOMArrayBuffer::Create(static_cast<const void*>(buffer.data()),
                                buffer.size());
}

void OnMakePublicKeyCredentialComplete(
    std::unique_ptr<ScopedPromiseResolver> scoped_resolver,
    AuthenticatorStatus status,
    PublicKeyCredentialInfoPtr credential) {
  auto* resolver = scoped_resolver->Release();
  const auto required_origin_type = RequiredOriginType::kSecure;

  AssertSecurityRequirementsBeforeResponse(resolver, required_origin_type);
  if (status == AuthenticatorStatus::SUCCESS) {
    DCHECK(credential);
    DCHECK(!credential->client_data_json.IsEmpty());
    DCHECK(!credential->response->attestation_object.IsEmpty());
    DOMArrayBuffer* client_data_buffer =
        VectorToDOMArrayBuffer(std::move(credential->client_data_json));
    DOMArrayBuffer* attestation_buffer = VectorToDOMArrayBuffer(
        std::move(credential->response->attestation_object));
    DOMArrayBuffer* raw_id =
        VectorToDOMArrayBuffer(std::move(credential->raw_id));
    AuthenticatorAttestationResponse* authenticator_response =
        AuthenticatorAttestationResponse::Create(client_data_buffer,
                                                 attestation_buffer);
    resolver->Resolve(PublicKeyCredential::Create(credential->id, raw_id,
                                                  authenticator_response));
  } else {
    DCHECK(!credential);
    resolver->Reject(CredentialManagerErrorToDOMException(
        mojo::ConvertTo<CredentialManagerError>(status)));
  }
}

}  // namespace

CredentialsContainer* CredentialsContainer::Create() {
  return new CredentialsContainer();
}

CredentialsContainer::CredentialsContainer() {}

ScriptPromise CredentialsContainer::get(
    ScriptState* script_state,
    const CredentialRequestOptions& options) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  auto required_origin_type =
      options.hasFederated() || options.hasPassword()
          ? RequiredOriginType::kSecureAndSameWithAncestors
          : RequiredOriginType::kSecure;
  if (!CheckSecurityRequirementsBeforeRequest(resolver, required_origin_type))
    return promise;

  Vector<KURL> providers;
  if (options.hasFederated() && options.federated().hasProviders()) {
    for (const auto& string : options.federated().providers()) {
      KURL url = KURL(NullURL(), string);
      if (url.IsValid())
        providers.push_back(std::move(url));
    }
  }

  CredentialMediationRequirement requirement;
  if (options.mediation() == "silent") {
    UseCounter::Count(ExecutionContext::From(script_state),
                      WebFeature::kCredentialManagerGetMediationSilent);
    requirement = CredentialMediationRequirement::kSilent;
  } else if (options.mediation() == "optional") {
    UseCounter::Count(ExecutionContext::From(script_state),
                      WebFeature::kCredentialManagerGetMediationOptional);
    requirement = CredentialMediationRequirement::kOptional;
  } else {
    DCHECK_EQ("required", options.mediation());
    UseCounter::Count(ExecutionContext::From(script_state),
                      WebFeature::kCredentialManagerGetMediationRequired);
    requirement = CredentialMediationRequirement::kRequired;
  }

  auto* credential_manager =
      CredentialManagerProxy::From(script_state)->CredentialManager();
  credential_manager->Get(
      requirement, options.password(), std::move(providers),
      WTF::Bind(&OnGetComplete,
                WTF::Passed(std::make_unique<ScopedPromiseResolver>(resolver)),
                required_origin_type));

  return promise;
}

ScriptPromise CredentialsContainer::store(ScriptState* script_state,
                                          Credential* credential) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  auto required_origin_type =
      credential->IsFederatedCredential() || credential->IsPasswordCredential()
          ? RequiredOriginType::kSecureAndSameWithAncestors
          : RequiredOriginType::kSecure;
  if (!CheckSecurityRequirementsBeforeRequest(resolver, required_origin_type))
    return promise;

  if (!(credential->IsFederatedCredential() ||
        credential->IsPasswordCredential())) {
    resolver->Reject(DOMException::Create(
        kNotSupportedError,
        "Store operation not permitted for PublicKey credentials."));
  }

  if (!IsIconURLEmptyOrSecure(credential)) {
    resolver->Reject(DOMException::Create(kSecurityError,
                                          "'iconURL' should be a secure URL"));
    return promise;
  }

  auto* credential_manager =
      CredentialManagerProxy::From(script_state)->CredentialManager();
  credential_manager->Store(
      CredentialInfo::From(credential),
      WTF::Bind(&OnStoreComplete,
                WTF::Passed(std::make_unique<ScopedPromiseResolver>(resolver)),
                required_origin_type));

  return promise;
}

ScriptPromise CredentialsContainer::create(
    ScriptState* script_state,
    const CredentialCreationOptions& options,
    ExceptionState& exception_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  const auto required_origin_type = RequiredOriginType::kSecure;
  if (!CheckSecurityRequirementsBeforeRequest(resolver, required_origin_type))
    return promise;

  if ((options.hasPassword() + options.hasFederated() +
       options.hasPublicKey()) != 1) {
    resolver->Reject(DOMException::Create(
        kNotSupportedError,
        "Only exactly one of 'password', 'federated', and 'publicKey' "
        "credential types are currently supported."));
    return promise;
  }

  if (options.hasPassword()) {
    resolver->Resolve(
        options.password().IsPasswordCredentialData()
            ? PasswordCredential::Create(
                  options.password().GetAsPasswordCredentialData(),
                  exception_state)
            : PasswordCredential::Create(
                  options.password().GetAsHTMLFormElement(), exception_state));
  } else if (options.hasFederated()) {
    resolver->Resolve(
        FederatedCredential::Create(options.federated(), exception_state));
  } else {
    DCHECK(options.hasPublicKey());
    auto mojo_options =
        MojoMakePublicKeyCredentialOptions::From(options.publicKey());
    if (mojo_options) {
      auto* authenticator =
          CredentialManagerProxy::From(script_state)->Authenticator();
      authenticator->MakeCredential(
          std::move(mojo_options),
          WTF::Bind(
              &OnMakePublicKeyCredentialComplete,
              WTF::Passed(std::make_unique<ScopedPromiseResolver>(resolver))));
    } else {
      resolver->Reject(DOMException::Create(
          kNotSupportedError,
          "Required parameters missing in `options.publicKey`."));
    }
  }

  return promise;
}

ScriptPromise CredentialsContainer::preventSilentAccess(
    ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  const auto required_origin_type = RequiredOriginType::kSecure;
  if (!CheckSecurityRequirementsBeforeRequest(resolver, required_origin_type))
    return promise;

  auto* credential_manager =
      CredentialManagerProxy::From(script_state)->CredentialManager();
  credential_manager->PreventSilentAccess(WTF::Bind(
      &OnPreventSilentAccessComplete,
      WTF::Passed(std::make_unique<ScopedPromiseResolver>(resolver))));

  return promise;
}

}  // namespace blink
