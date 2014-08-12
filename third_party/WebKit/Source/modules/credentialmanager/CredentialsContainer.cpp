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
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCredential.h"
#include "public/platform/WebCredentialManager.h"
#include "public/platform/WebCredentialManagerError.h"

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

class NotificationCallbacks : public WebCredentialManager::NotificationCallbacks {
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

class RequestCallbacks : public WebCredentialManager::RequestCallbacks {
    WTF_MAKE_NONCOPYABLE(RequestCallbacks);
public:
    explicit RequestCallbacks(PassRefPtr<ScriptPromiseResolver> resolver) : m_resolver(resolver) { }
    virtual ~RequestCallbacks() { }

    virtual void onSuccess(WebCredential* credential) OVERRIDE
    {
        m_resolver->resolve(Credential::create(credential->id(), credential->name(), credential->avatarURL()));
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
    ScriptWrappable::init(this);
}

static bool canAccessCredentialManagerAPI(ScriptState* scriptState, PassRefPtr<ScriptPromiseResolver> resolver)
{
    SecurityOrigin* securityOrigin = scriptState->executionContext()->securityOrigin();
    String errorMessage;
    if (!securityOrigin->canAccessFeatureRequiringSecureOrigin(errorMessage)) {
        resolver->reject(DOMException::create(SecurityError, errorMessage));
        return false;
    }
    return true;
}

static ScriptPromise stubImplementation(ScriptState* scriptState)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    if (!canAccessCredentialManagerAPI(scriptState, resolver))
        return promise;

    resolver->resolve();
    return promise;
}

ScriptPromise CredentialsContainer::request(ScriptState* scriptState, const Dictionary&)
{
    return stubImplementation(scriptState);
}

ScriptPromise CredentialsContainer::notifySignedIn(ScriptState* scriptState, Credential* credential)
{
    return stubImplementation(scriptState);
}

ScriptPromise CredentialsContainer::notifyFailedSignIn(ScriptState* scriptState, Credential* credential)
{
    return stubImplementation(scriptState);
}

ScriptPromise CredentialsContainer::notifySignedOut(ScriptState* scriptState)
{
    return stubImplementation(scriptState);
}

} // namespace blink
