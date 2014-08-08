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

namespace blink {

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

ScriptPromise CredentialsContainer::request(ScriptState* scriptState, const Dictionary&)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    if (!canAccessCredentialManagerAPI(scriptState, resolver))
        return promise;

    resolver->resolve();
    return promise;
}

} // namespace blink
