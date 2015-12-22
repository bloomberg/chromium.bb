// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/quota/StorageManager.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "modules/permissions/Permissions.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCallbacks.h"
#include "public/platform/modules/permissions/WebPermissionClient.h"
#include "public/platform/modules/permissions/WebPermissionStatus.h"

namespace blink {

namespace {

class DurableStorageQueryCallbacks final : public WebPermissionCallback {
public:
    DurableStorageQueryCallbacks(ScriptPromiseResolver* resolver)
        : m_resolver(resolver)
    {
    }

    void onSuccess(WebPermissionStatus status) override
    {
        String toReturn;
        switch (status) {
        case WebPermissionStatusGranted:
            toReturn = "granted";
            break;
        case WebPermissionStatusDenied:
            toReturn = "denied";
            break;
        case WebPermissionStatusPrompt:
            toReturn = "default";
            break;
        }
        m_resolver->resolve(toReturn);
    }
    void onError() override
    {
        ASSERT_NOT_REACHED();
    }

private:
    Persistent<ScriptPromiseResolver> m_resolver;
};

class DurableStorageRequestCallbacks final : public WebPermissionCallback {
public:
    DurableStorageRequestCallbacks(ScriptPromiseResolver* resolver)
        : m_resolver(resolver)
    {
    }

    void onSuccess(WebPermissionStatus status) override
    {
        m_resolver->resolve(status == WebPermissionStatusGranted);
    }
    void onError() override
    {
        ASSERT_NOT_REACHED();
    }

private:
    Persistent<ScriptPromiseResolver> m_resolver;
};

} // namespace

ScriptPromise StorageManager::requestPersistent(ScriptState* scriptState)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    ExecutionContext* executionContext = scriptState->executionContext();
    SecurityOrigin* securityOrigin = executionContext->securityOrigin();
    // TODO(dgrogan): Is the isUnique() check covered by the later
    // isSecureContext() check? If so, maybe remove it. Write a test if it
    // stays.
    if (securityOrigin->isUnique()) {
        resolver->reject(DOMException::create(NotSupportedError));
        return promise;
    }
    String errorMessage;
    if (!executionContext->isSecureContext(errorMessage)) {
        resolver->reject(DOMException::create(SecurityError, errorMessage));
        return promise;
    }
    ASSERT(executionContext->isDocument());
    WebPermissionClient* permissionClient = Permissions::getClient(executionContext);
    if (!permissionClient) {
        resolver->reject(DOMException::create(InvalidStateError, "In its current state, the global scope can't request permissions."));
        return promise;
    }
    permissionClient->requestPermission(WebPermissionTypeDurableStorage, KURL(KURL(), scriptState->executionContext()->securityOrigin()->toString()), new DurableStorageRequestCallbacks(resolver));

    return promise;
}

ScriptPromise StorageManager::persistentPermission(ScriptState* scriptState)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    WebPermissionClient* permissionClient = Permissions::getClient(scriptState->executionContext());
    if (!permissionClient) {
        resolver->reject(DOMException::create(InvalidStateError, "In its current state, the global scope can't query permissions."));
        return promise;
    }
    permissionClient->queryPermission(WebPermissionTypeDurableStorage, KURL(KURL(), scriptState->executionContext()->securityOrigin()->toString()), new DurableStorageQueryCallbacks(resolver));
    return promise;
}

DEFINE_TRACE(StorageManager)
{
}

} // namespace blink
