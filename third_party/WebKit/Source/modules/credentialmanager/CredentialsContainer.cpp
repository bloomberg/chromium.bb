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
#include "modules/credentialmanager/MakeCredentialOptions.h"
#include "modules/credentialmanager/PasswordCredential.h"
#include "modules/credentialmanager/PublicKeyCredential.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCredential.h"
#include "public/platform/WebCredentialManagerClient.h"
#include "public/platform/WebCredentialMediationRequirement.h"
#include "public/platform/WebFederatedCredential.h"
#include "public/platform/WebPasswordCredential.h"

namespace blink {

static void RejectDueToCredentialManagerError(
    ScriptPromiseResolver* resolver,
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

class NotificationCallbacks
    : public WebCredentialManagerClient::NotificationCallbacks {
  WTF_MAKE_NONCOPYABLE(NotificationCallbacks);

 public:
  explicit NotificationCallbacks(ScriptPromiseResolver* resolver)
      : resolver_(resolver) {}
  ~NotificationCallbacks() override {}

  void OnSuccess() override {
    Frame* frame =
        ToDocument(ExecutionContext::From(resolver_->GetScriptState()))
            ->GetFrame();
    SECURITY_CHECK(!frame || frame == frame->Tree().Top());

    resolver_->Resolve();
  }

  void OnError(WebCredentialManagerError reason) override {
    RejectDueToCredentialManagerError(resolver_, reason);
  }

 private:
  const Persistent<ScriptPromiseResolver> resolver_;
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
    SECURITY_CHECK(!frame || frame == frame->Tree().Top());

    std::unique_ptr<WebCredential> credential =
        WTF::WrapUnique(web_credential.release());
    if (!credential || !frame) {
      resolver_->Resolve();
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

    if (!credential || !frame) {
      resolver_->Resolve();
      return;
    }

    if (!credential->client_data_json.IsEmpty() ||
        credential->response->attestation_object.IsEmpty()) {
      DOMArrayBuffer* client_data_buffer =
          VectorToDOMArrayBuffer(std::move(credential->client_data_json));
      DOMArrayBuffer* attestation_buffer = VectorToDOMArrayBuffer(
          std::move(credential->response->attestation_object));
      DOMArrayBuffer* raw_id =
          VectorToDOMArrayBuffer(std::move(credential->raw_id));
      AuthenticatorAttestationResponse* authenticator_response =
          AuthenticatorAttestationResponse::Create(client_data_buffer,
                                                   attestation_buffer);
      PublicKeyCredential* publicKeyCredential = PublicKeyCredential::Create(
          credential->id, raw_id, authenticator_response);
      resolver_->Resolve(publicKeyCredential);
    }
    resolver_->Resolve();
  }

  void OnError(WebCredentialManagerError reason) override {
    RejectDueToCredentialManagerError(resolver_, reason);
  }

 private:
  DOMArrayBuffer* VectorToDOMArrayBuffer(Vector<uint8_t> buffer) {
    return DOMArrayBuffer::Create(static_cast<void*>(buffer.data()),
                                  buffer.size());
  }

  const Persistent<ScriptPromiseResolver> resolver_;
};

CredentialsContainer* CredentialsContainer::Create() {
  return new CredentialsContainer();
}

CredentialsContainer::CredentialsContainer() {}

static bool CheckBoilerplate(ScriptPromiseResolver* resolver) {
  Frame* frame = ToDocument(ExecutionContext::From(resolver->GetScriptState()))
                     ->GetFrame();
  if (!frame || frame != frame->Tree().Top()) {
    resolver->Reject(DOMException::Create(kSecurityError,
                                          "CredentialContainer methods may "
                                          "only be executed in a top-level "
                                          "document."));
    return false;
  }

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

ScriptPromise CredentialsContainer::get(
    ScriptState* script_state,
    const CredentialRequestOptions& options) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (!CheckBoilerplate(resolver))
    return promise;

  ExecutionContext* context = ExecutionContext::From(script_state);
  // Set the default mediation option if none is provided.
  // If both 'unmediated' and 'mediation' are set log a warning if they are
  // contradicting.
  // Also sets 'mediation' appropriately when only 'unmediated' is set.
  // TODO(http://crbug.com/715077): Remove this when 'unmediated' is removed.
  String mediation = "optional";
  if (options.hasUnmediated() && !options.hasMediation()) {
    mediation = options.unmediated() ? "silent" : "optional";
    UseCounter::Count(
        context,
        WebFeature::kCredentialManagerCredentialRequestOptionsOnlyUnmediated);
  } else if (options.hasMediation()) {
    mediation = options.mediation();
    if (options.hasUnmediated() &&
        ((options.unmediated() && options.mediation() != "silent") ||
         (!options.unmediated() && options.mediation() != "optional"))) {
      context->AddConsoleMessage(ConsoleMessage::Create(
          kJSMessageSource, kWarningMessageLevel,
          "mediation: '" + options.mediation() + "' overrides unmediated: " +
              (options.unmediated() ? "true" : "false") + "."));
    }
  }

  Vector<KURL> providers;
  if (options.hasFederated() && options.federated().hasProviders()) {
    for (const auto& string : options.federated().providers()) {
      KURL url = KURL(NullURL(), string);
      if (url.IsValid())
        providers.push_back(std::move(url));
    }
  }

  WebCredentialMediationRequirement requirement;

  if (mediation == "silent") {
    UseCounter::Count(context,
                      WebFeature::kCredentialManagerGetMediationSilent);
    requirement = WebCredentialMediationRequirement::kSilent;
  } else if (mediation == "optional") {
    UseCounter::Count(context,
                      WebFeature::kCredentialManagerGetMediationOptional);
    requirement = WebCredentialMediationRequirement::kOptional;
  } else {
    DCHECK_EQ("required", mediation);
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
  if (!CheckBoilerplate(resolver))
    return promise;

  auto web_credential =
      WebCredential::Create(credential->GetPlatformCredential());
  CredentialManagerClient::From(ExecutionContext::From(script_state))
      ->DispatchStore(*web_credential, new NotificationCallbacks(resolver));
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

  if ((options.hasPassword() ^ options.hasFederated() ^
       options.hasPublicKey()) != 1) {
    resolver->Reject(DOMException::Create(kNotSupportedError,
                                          "Only exactly one of 'password', "
                                          "'federated', and 'publicKey' "
                                          "credential types are currently "
                                          "supported."));
    return promise;
  }

  if (options.hasPassword()) {
    if (options.password().isPasswordCredentialData()) {
      resolver->Resolve(PasswordCredential::Create(
          options.password().getAsPasswordCredentialData(), exception_state));
    } else {
      resolver->Resolve(PasswordCredential::Create(
          options.password().getAsHTMLFormElement(), exception_state));
    }
  } else if (options.hasFederated()) {
    resolver->Resolve(
        FederatedCredential::Create(options.federated(), exception_state));
  } else {
    DCHECK(options.hasPublicKey());
    // Dispatch the publicKey credential operation.
    // TODO(https://crbug.com/740081): Eventually unify with CredMan's mojo.
    LocalFrame* frame =
        ToDocument(ExecutionContext::From(script_state))->GetFrame();

    CredentialManagerClient::From(ExecutionContext::From(script_state))
        ->DispatchMakeCredential(frame, options.publicKey(),
                                 new PublicKeyCallbacks(resolver));
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
      ->DispatchPreventSilentAccess(new NotificationCallbacks(resolver));
  return promise;
}

ScriptPromise CredentialsContainer::requireUserMediation(
    ScriptState* script_state) {
  return preventSilentAccess(script_state);
}

}  // namespace blink
