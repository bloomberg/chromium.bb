// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/credentialmanager/CredentialsContainer.h"

#include <memory>
#include <utility>
#include "bindings/core/v8/Dictionary.h"
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
#include "modules/credentialmanager/AuthenticatorAttestationResponse.h"
#include "modules/credentialmanager/AuthenticatorResponse.h"
#include "modules/credentialmanager/Credential.h"
#include "modules/credentialmanager/CredentialCreationOptions.h"
#include "modules/credentialmanager/CredentialManagerClient.h"
#include "modules/credentialmanager/CredentialRequestOptions.h"
#include "modules/credentialmanager/FederatedCredential.h"
#include "modules/credentialmanager/FederatedCredentialRequestOptions.h"
#include "modules/credentialmanager/MakePublicKeyCredentialOptions.h"
#include "modules/credentialmanager/PasswordCredential.h"
#include "modules/credentialmanager/PublicKeyCredential.h"
#include "platform/credentialmanager/PlatformFederatedCredential.h"
#include "platform/credentialmanager/PlatformPasswordCredential.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCredential.h"
#include "public/platform/WebCredentialManagerClient.h"
#include "public/platform/WebCredentialMediationRequirement.h"
#include "public/platform/WebFederatedCredential.h"
#include "public/platform/WebPasswordCredential.h"

namespace blink {
namespace {

bool IsSameOriginWithAncestors(Frame* frame) {
  if (!frame)
    return true;

  Frame* current = frame;
  SecurityOrigin* origin = frame->GetSecurityContext()->GetSecurityOrigin();
  while (current->Tree().Parent()) {
    current = current->Tree().Parent();
    if (!origin->CanAccess(current->GetSecurityContext()->GetSecurityOrigin()))
      return false;
  }
  return true;
}

void RejectDueToCredentialManagerError(ScriptPromiseResolver* resolver,
                                       WebCredentialManagerError reason) {
  switch (reason) {
    case kWebCredentialManagerDisabledError:
      resolver->Reject(DOMException::Create(
          kInvalidStateError, "The credential manager is disabled."));
      break;
    case kWebCredentialManagerPendingRequestError:
      resolver->Reject(DOMException::Create(kInvalidStateError,
                                            "A 'get()' request is pending."));
      break;
    case kWebCredentialManagerNotAllowedError:
      resolver->Reject(DOMException::Create(kNotAllowedError,
                                            "The operation is not allowed."));
      break;
    case kWebCredentialManagerNotSupportedError:
      resolver->Reject(DOMException::Create(
          kNotSupportedError,
          "Parameters for this operation are not supported."));
      break;
    case kWebCredentialManagerSecurityError:
      resolver->Reject(DOMException::Create(kSecurityError,
                                            "The operation is insecure and "
                                            "is not allowed."));
      break;
    case kWebCredentialManagerCancelledError:
      resolver->Reject(DOMException::Create(
          kNotAllowedError, "The user cancelled the operation."));
      break;
    case kWebCredentialManagerNotImplementedError:
      resolver->Reject(DOMException::Create(
          kNotAllowedError, "The operation is not implemented."));
      break;
    case kWebCredentialManagerUnknownError:
    default:
      resolver->Reject(DOMException::Create(kNotReadableError,
                                            "An unknown error occurred while "
                                            "talking to the credential "
                                            "manager."));
      break;
  }
}

bool CheckBoilerplate(ScriptPromiseResolver* resolver) {
  String error_message;
  if (!ExecutionContext::From(resolver->GetScriptState())
           ->IsSecureContext(error_message)) {
    resolver->Reject(DOMException::Create(kSecurityError, error_message));
    return false;
  }

  CredentialManagerClient* client = CredentialManagerClient::From(
      ExecutionContext::From(resolver->GetScriptState()));
  if (!client) {
    resolver->Reject(DOMException::Create(
        kInvalidStateError,
        "Could not establish connection to the credential manager."));
    return false;
  }

  return true;
}

bool IsIconURLInsecure(const Credential* credential) {
  PlatformCredential* platform_credential = credential->GetPlatformCredential();
  auto is_insecure = [](const KURL& url) {
    return !url.IsEmpty() && !url.ProtocolIs("https");
  };
  if (platform_credential->IsFederated()) {
    return is_insecure(
        static_cast<PlatformFederatedCredential*>(platform_credential)
            ->IconURL());
  }
  if (platform_credential->IsPassword()) {
    return is_insecure(
        static_cast<PlatformPasswordCredential*>(platform_credential)
            ->IconURL());
  }
  return false;
}

}  // namespace

class NotificationCallbacks
    : public WebCredentialManagerClient::NotificationCallbacks {
  WTF_MAKE_NONCOPYABLE(NotificationCallbacks);

 public:
  enum class SameOriginRequirement { kMustBeSameOrigin, kCanBeCrossOrigin };

  explicit NotificationCallbacks(ScriptPromiseResolver* resolver,
                                 SameOriginRequirement same_origin_requirement)
      : resolver_(resolver),
        same_origin_requirement_(same_origin_requirement) {}
  ~NotificationCallbacks() override {}

  void OnSuccess() override {
    Frame* frame =
        ToDocument(ExecutionContext::From(resolver_->GetScriptState()))
            ->GetFrame();
    SECURITY_CHECK(!frame ||
                   same_origin_requirement_ ==
                       SameOriginRequirement::kCanBeCrossOrigin ||
                   IsSameOriginWithAncestors(frame));

    resolver_->Resolve();
  }

  void OnError(WebCredentialManagerError reason) override {
    RejectDueToCredentialManagerError(resolver_, reason);
  }

 private:
  const Persistent<ScriptPromiseResolver> resolver_;
  const SameOriginRequirement same_origin_requirement_;
};

class RequestCallbacks : public WebCredentialManagerClient::RequestCallbacks {
  WTF_MAKE_NONCOPYABLE(RequestCallbacks);

 public:
  explicit RequestCallbacks(ScriptPromiseResolver* resolver)
      : resolver_(resolver) {}
  ~RequestCallbacks() override {}

  void OnSuccess(std::unique_ptr<WebCredential> web_credential) override {
    ExecutionContext* context =
        ExecutionContext::From(resolver_->GetScriptState());
    if (!context)
      return;
    Frame* frame = ToDocument(context)->GetFrame();
    SECURITY_CHECK(!frame || IsSameOriginWithAncestors(frame));

    std::unique_ptr<WebCredential> credential =
        WTF::WrapUnique(web_credential.release());
    if (!frame) {
      resolver_->Resolve();
      return;
    }

    if (!credential) {
      resolver_->Resolve(v8::Null(resolver_->GetScriptState()->GetIsolate()));
      return;
    }

    DCHECK(credential->IsPasswordCredential() ||
           credential->IsFederatedCredential());
    UseCounter::Count(ExecutionContext::From(resolver_->GetScriptState()),
                      WebFeature::kCredentialManagerGetReturnedCredential);
    if (credential->IsPasswordCredential())
      resolver_->Resolve(PasswordCredential::Create(
          static_cast<WebPasswordCredential*>(credential.get())));
    else
      resolver_->Resolve(FederatedCredential::Create(
          static_cast<WebFederatedCredential*>(credential.get())));
  }

  void OnError(WebCredentialManagerError reason) override {
    RejectDueToCredentialManagerError(resolver_, reason);
  }

 private:
  const Persistent<ScriptPromiseResolver> resolver_;
};

class PublicKeyCallbacks : public WebAuthenticationClient::PublicKeyCallbacks {
  WTF_MAKE_NONCOPYABLE(PublicKeyCallbacks);

 public:
  explicit PublicKeyCallbacks(ScriptPromiseResolver* resolver)
      : resolver_(resolver) {}
  ~PublicKeyCallbacks() override {
    OnError(blink::kWebCredentialManagerUnknownError);
  }

  void OnSuccess(
      webauth::mojom::blink::PublicKeyCredentialInfoPtr credential) override {
    ExecutionContext* context =
        ExecutionContext::From(resolver_->GetScriptState());
    if (!context)
      return;
    Frame* frame = ToDocument(context)->GetFrame();
    SECURITY_CHECK(!frame || frame == frame->Tree().Top());

    if (!frame) {
      resolver_->Resolve();
      return;
    }

    if (!credential || credential->client_data_json.IsEmpty() ||
        credential->response->attestation_object.IsEmpty()) {
      resolver_->Resolve(v8::Null(resolver_->GetScriptState()->GetIsolate()));
      return;
    }

    DOMArrayBuffer* client_data_buffer =
        VectorToDOMArrayBuffer(std::move(credential->client_data_json));
    DOMArrayBuffer* attestation_buffer = VectorToDOMArrayBuffer(
        std::move(credential->response->attestation_object));
    DOMArrayBuffer* raw_id =
        VectorToDOMArrayBuffer(std::move(credential->raw_id));
    AuthenticatorAttestationResponse* authenticator_response =
        AuthenticatorAttestationResponse::Create(client_data_buffer,
                                                 attestation_buffer);
    resolver_->Resolve(PublicKeyCredential::Create(credential->id, raw_id,
                                                   authenticator_response));
  }

  void OnError(WebCredentialManagerError reason) override {
    RejectDueToCredentialManagerError(resolver_, reason);
  }

 private:
  DOMArrayBuffer* VectorToDOMArrayBuffer(const Vector<uint8_t> buffer) {
    return DOMArrayBuffer::Create(static_cast<const void*>(buffer.data()),
                                  buffer.size());
  }

  const Persistent<ScriptPromiseResolver> resolver_;
};

CredentialsContainer* CredentialsContainer::Create() {
  return new CredentialsContainer();
}

CredentialsContainer::CredentialsContainer() {}

ScriptPromise CredentialsContainer::get(
    ScriptState* script_state,
    const CredentialRequestOptions& options) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  ExecutionContext* context = ExecutionContext::From(script_state);

  Frame* frame = ToDocument(context)->GetFrame();
  if ((options.hasPassword() || options.hasFederated()) &&
      !IsSameOriginWithAncestors(frame)) {
    resolver->Reject(DOMException::Create(
        kNotAllowedError,
        "`PasswordCredential` and `FederatedCredential` objects may only be "
        "retrieved in a document which is same-origin with all of its "
        "ancestors."));
    return promise;
  }

  if (!CheckBoilerplate(resolver))
    return promise;

  Vector<KURL> providers;
  if (options.hasFederated() && options.federated().hasProviders()) {
    for (const auto& string : options.federated().providers()) {
      KURL url = KURL(NullURL(), string);
      if (url.IsValid())
        providers.push_back(std::move(url));
    }
  }

  WebCredentialMediationRequirement requirement;

  if (options.mediation() == "silent") {
    UseCounter::Count(context,
                      WebFeature::kCredentialManagerGetMediationSilent);
    requirement = WebCredentialMediationRequirement::kSilent;
  } else if (options.mediation() == "optional") {
    UseCounter::Count(context,
                      WebFeature::kCredentialManagerGetMediationOptional);
    requirement = WebCredentialMediationRequirement::kOptional;
  } else {
    DCHECK_EQ("required", options.mediation());
    UseCounter::Count(context,
                      WebFeature::kCredentialManagerGetMediationRequired);
    requirement = WebCredentialMediationRequirement::kRequired;
  }

  CredentialManagerClient::From(context)->DispatchGet(
      requirement, options.password(), providers,
      new RequestCallbacks(resolver));
  return promise;
}

ScriptPromise CredentialsContainer::store(ScriptState* script_state,
                                          Credential* credential) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  Frame* frame = ToDocument(ExecutionContext::From(script_state))->GetFrame();
  if ((credential->type() == "password" || credential->type() == "federated") &&
      !IsSameOriginWithAncestors(frame)) {
    resolver->Reject(
        DOMException::Create(kNotAllowedError,
                             "`PasswordCredential` and `FederatedCredential` "
                             "objects may only be stored in a document which "
                             "is same-origin with all of its ancestors."));
    return promise;
  }

  if (!CheckBoilerplate(resolver))
    return promise;

  if (!(credential->GetPlatformCredential()->IsFederated() ||
        credential->GetPlatformCredential()->IsPassword())) {
    resolver->Reject(DOMException::Create(
        kNotSupportedError,
        "Store operation not permitted for PublicKey credentials."));
  }

  if (IsIconURLInsecure(credential)) {
    resolver->Reject(DOMException::Create(kSecurityError,
                                          "'iconURL' should be a secure URL"));
    return promise;
  }

  auto web_credential =
      WebCredential::Create(credential->GetPlatformCredential());
  CredentialManagerClient::From(ExecutionContext::From(script_state))
      ->DispatchStore(
          *web_credential,
          new NotificationCallbacks(
              resolver,
              NotificationCallbacks::SameOriginRequirement::kMustBeSameOrigin));
  return promise;
}

ScriptPromise CredentialsContainer::create(
    ScriptState* script_state,
    const CredentialCreationOptions& options,
    ExceptionState& exception_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (!CheckBoilerplate(resolver))
    return promise;

  if ((options.hasPassword() + options.hasFederated() +
       options.hasPublicKey()) != 1) {
    resolver->Reject(DOMException::Create(kNotSupportedError,
                                          "Only exactly one of 'password', "
                                          "'federated', and 'publicKey' "
                                          "credential types are currently "
                                          "supported."));
    return promise;
  }

  if (options.hasPassword()) {
    if (options.password().IsPasswordCredentialData()) {
      resolver->Resolve(PasswordCredential::Create(
          options.password().GetAsPasswordCredentialData(), exception_state));
    } else {
      resolver->Resolve(PasswordCredential::Create(
          options.password().GetAsHTMLFormElement(), exception_state));
    }
  } else if (options.hasFederated()) {
    resolver->Resolve(
        FederatedCredential::Create(options.federated(), exception_state));
  } else {
    DCHECK(options.hasPublicKey());
    // Dispatch the publicKey credential operation.
    // TODO(https://crbug.com/740081): Eventually unify with CredMan's mojo.
    // TODO(engedy): Make frame checks more efficient in the refactor.
    LocalFrame* frame =
        ToDocument(ExecutionContext::From(script_state))->GetFrame();
    CredentialManagerClient::From(ExecutionContext::From(script_state))
        ->DispatchMakeCredential(
            *frame, options.publicKey(),
            std::make_unique<PublicKeyCallbacks>(resolver));
  }
  return promise;
}

ScriptPromise CredentialsContainer::preventSilentAccess(
    ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (!CheckBoilerplate(resolver))
    return promise;

  CredentialManagerClient::From(ExecutionContext::From(script_state))
      ->DispatchPreventSilentAccess(new NotificationCallbacks(
          resolver,
          NotificationCallbacks::SameOriginRequirement::kCanBeCrossOrigin));
  return promise;
}

}  // namespace blink
