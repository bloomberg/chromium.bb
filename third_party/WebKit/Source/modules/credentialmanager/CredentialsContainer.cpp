// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/credentialmanager/CredentialsContainer.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/Frame.h"
#include "core/frame/UseCounter.h"
#include "core/page/FrameTree.h"
#include "modules/credentialmanager/Credential.h"
#include "modules/credentialmanager/CredentialManagerClient.h"
#include "modules/credentialmanager/CredentialRequestOptions.h"
#include "modules/credentialmanager/FederatedCredential.h"
#include "modules/credentialmanager/FederatedCredentialRequestOptions.h"
#include "modules/credentialmanager/PasswordCredential.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCredential.h"
#include "public/platform/WebCredentialManagerClient.h"
#include "public/platform/WebCredentialManagerError.h"
#include "public/platform/WebFederatedCredential.h"
#include "public/platform/WebPasswordCredential.h"
#include "wtf/PtrUtil.h"
#include <memory>

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
        ToDocument(resolver_->GetScriptState()->GetExecutionContext())
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
    Frame* frame =
        ToDocument(resolver_->GetScriptState()->GetExecutionContext())
            ->GetFrame();
    SECURITY_CHECK(!frame || frame == frame->Tree().Top());

    std::unique_ptr<WebCredential> credential =
        WTF::WrapUnique(web_credential.release());
    if (!credential || !frame) {
      resolver_->Resolve();
      return;
    }

    ASSERT(credential->IsPasswordCredential() ||
           credential->IsFederatedCredential());
    UseCounter::Count(resolver_->GetScriptState()->GetExecutionContext(),
                      UseCounter::kCredentialManagerGetReturnedCredential);
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

CredentialsContainer* CredentialsContainer::Create() {
  return new CredentialsContainer();
}

CredentialsContainer::CredentialsContainer() {}

static bool CheckBoilerplate(ScriptPromiseResolver* resolver) {
  Frame* frame =
      ToDocument(resolver->GetScriptState()->GetExecutionContext())->GetFrame();
  if (!frame || frame != frame->Tree().Top()) {
    resolver->Reject(DOMException::Create(kSecurityError,
                                          "CredentialContainer methods may "
                                          "only be executed in a top-level "
                                          "document."));
    return false;
  }

  String error_message;
  if (!resolver->GetScriptState()->GetExecutionContext()->IsSecureContext(
          error_message)) {
    resolver->Reject(DOMException::Create(kSecurityError, error_message));
    return false;
  }

  CredentialManagerClient* client = CredentialManagerClient::From(
      resolver->GetScriptState()->GetExecutionContext());
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

  Vector<KURL> providers;
  if (options.hasFederated() && options.federated().hasProviders()) {
    // TODO(mkwst): CredentialRequestOptions::federated() needs to return a
    // reference, not a value.  Because it returns a temporary value now, a for
    // loop that directly references the value generates code that holds a
    // reference to a value that no longer exists by the time the loop starts
    // looping. In order to avoid this crazyness for the moment, we're making a
    // copy of the vector. https://crbug.com/587088
    const Vector<String> provider_strings = options.federated().providers();
    for (const auto& string : provider_strings) {
      KURL url = KURL(KURL(), string);
      if (url.IsValid())
        providers.push_back(url);
    }
  }

  UseCounter::Count(script_state->GetExecutionContext(),
                    options.unmediated()
                        ? UseCounter::kCredentialManagerGetWithoutUI
                        : UseCounter::kCredentialManagerGetWithUI);

  CredentialManagerClient::From(script_state->GetExecutionContext())
      ->DispatchGet(options.unmediated(), options.password(), providers,
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
  CredentialManagerClient::From(script_state->GetExecutionContext())
      ->DispatchStore(*web_credential, new NotificationCallbacks(resolver));
  return promise;
}

ScriptPromise CredentialsContainer::requireUserMediation(
    ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (!CheckBoilerplate(resolver))
    return promise;

  CredentialManagerClient::From(script_state->GetExecutionContext())
      ->DispatchRequireUserMediation(new NotificationCallbacks(resolver));
  return promise;
}

}  // namespace blink
