// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/credentialmanager/CredentialsContainer.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "modules/credentialmanager/Credential.h"
#include "modules/credentialmanager/CredentialManagerClient.h"
#include "modules/credentialmanager/LocalCredential.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCredential.h"
#include "public/platform/WebCredentialManagerClient.h"
#include "public/platform/WebCredentialManagerError.h"
#include "public/platform/WebLocalCredential.h"

namespace blink {

static void rejectDueToCredentialManagerError(PassRefPtr<ScriptPromiseResolver> resolver, WebCredentialManagerError* reason)
{
    switch (reason->errorType) {
    case WebCredentialManagerError::ErrorTypeDisabled:
        resolver->reject(DOMException::create(InvalidStateError, "The credential manager is disabled."));
        break;
    case WebCredentialManagerError::ErrorTypeUnknown:
    default:
        resolver->reject(DOMException::create(NotReadableError, "An unknown error occured while talking to the credential manager."));
        break;
    }
}

class NotificationCallbacks : public WebCredentialManagerClient::NotificationCallbacks {
    WTF_MAKE_NONCOPYABLE(NotificationCallbacks);
public:
    explicit NotificationCallbacks(PassRefPtr<ScriptPromiseResolver> resolver) : m_resolver(resolver) { }
    virtual ~NotificationCallbacks() { }

    virtual void onSuccess() OVERRIDE
    {
        m_resolver->resolve();
    }

    virtual void onError(WebCredentialManagerError* reason) OVERRIDE
    {
        rejectDueToCredentialManagerError(m_resolver, reason);
    }

private:
    const RefPtr<ScriptPromiseResolver> m_resolver;
};

class RequestCallbacks : public WebCredentialManagerClient::RequestCallbacks {
    WTF_MAKE_NONCOPYABLE(RequestCallbacks);
public:
    explicit RequestCallbacks(PassRefPtr<ScriptPromiseResolver> resolver) : m_resolver(resolver) { }
    virtual ~RequestCallbacks() { }

    virtual void onSuccess(WebCredential* credential) OVERRIDE
    {
        if (!credential) {
            m_resolver->resolve();
            return;
        }

        // FIXME: Split this into Local/Federated types. Right now it's hard-coded to be a LocalCredential. :(
        m_resolver->resolve(LocalCredential::create(static_cast<WebLocalCredential*>(credential)));
    }

    virtual void onError(WebCredentialManagerError* reason) OVERRIDE
    {
        rejectDueToCredentialManagerError(m_resolver, reason);
    }

private:
    const RefPtr<ScriptPromiseResolver> m_resolver;
};


CredentialsContainer* CredentialsContainer::create()
{
    return new CredentialsContainer();
}

CredentialsContainer::CredentialsContainer()
{
}

static bool checkBoilerplate(PassRefPtr<ScriptPromiseResolver> resolver)
{
    CredentialManagerClient* client = CredentialManagerClient::from(resolver->scriptState()->executionContext());
    if (!client) {
        resolver->reject(DOMException::create(InvalidStateError, "Could not establish connection to the credential manager."));
        return false;
    }

    SecurityOrigin* securityOrigin = resolver->scriptState()->executionContext()->securityOrigin();
    String errorMessage;
    if (!securityOrigin->canAccessFeatureRequiringSecureOrigin(errorMessage)) {
        resolver->reject(DOMException::create(SecurityError, errorMessage));
        return false;
    }

    return true;
}

ScriptPromise CredentialsContainer::request(ScriptState* scriptState, const Dictionary&)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (!checkBoilerplate(resolver))
        return promise;

    WebVector<WebURL> tempVector;
    CredentialManagerClient::from(scriptState->executionContext())->dispatchRequest(false, tempVector, new RequestCallbacks(resolver));
    return promise;
}

ScriptPromise CredentialsContainer::notifySignedIn(ScriptState* scriptState, Credential* credential)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (!checkBoilerplate(resolver))
        return promise;

    CredentialManagerClient::from(scriptState->executionContext())->dispatchSignedIn(WebCredential(credential->platformCredential()), new NotificationCallbacks(resolver));
    return promise;
}

ScriptPromise CredentialsContainer::notifyFailedSignIn(ScriptState* scriptState, Credential* credential)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (!checkBoilerplate(resolver))
        return promise;

    CredentialManagerClient::from(scriptState->executionContext())->dispatchFailedSignIn(WebCredential(credential->platformCredential()), new NotificationCallbacks(resolver));
    return promise;
}

ScriptPromise CredentialsContainer::notifySignedOut(ScriptState* scriptState)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (!checkBoilerplate(resolver))
        return promise;

    CredentialManagerClient::from(scriptState->executionContext())->dispatchSignedOut(new NotificationCallbacks(resolver));
    return promise;
}

} // namespace blink
