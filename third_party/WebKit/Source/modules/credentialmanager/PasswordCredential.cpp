// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/credentialmanager/PasswordCredential.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExecutionContext.h"
#include "core/html/FormData.h"
#include "modules/credentialmanager/FormDataOptions.h"
#include "modules/credentialmanager/PasswordCredentialData.h"
#include "platform/credentialmanager/PlatformPasswordCredential.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebCredential.h"
#include "public/platform/WebPasswordCredential.h"

namespace blink {

PasswordCredential* PasswordCredential::create(WebPasswordCredential* webPasswordCredential)
{
    return new PasswordCredential(webPasswordCredential);
}

PasswordCredential* PasswordCredential::create(const PasswordCredentialData& data, ExceptionState& exceptionState)
{
    KURL iconURL = parseStringAsURL(data.iconURL(), exceptionState);
    if (exceptionState.hadException())
        return nullptr;
    return new PasswordCredential(data.id(), data.password(), data.name(), iconURL);
}

PasswordCredential::PasswordCredential(WebPasswordCredential* webPasswordCredential)
    : Credential(webPasswordCredential->platformCredential())
{
}

PasswordCredential::PasswordCredential(const String& id, const String& password, const String& name, const KURL& icon)
    : Credential(PlatformPasswordCredential::create(id, password, name, icon))
{
}

FormData* PasswordCredential::toFormData(ScriptState* scriptState, const FormDataOptions& options)
{
    FormData* fd = FormData::create();

    String errorMessage;
    if (!scriptState->executionContext()->isSecureContext(errorMessage))
        return fd;

    fd->append(options.idName(), id());
    fd->append(options.passwordName(), password());
    fd->makeOpaque();
    return fd;
}

const String& PasswordCredential::password() const
{
    return static_cast<PlatformPasswordCredential*>(m_platformCredential.get())->password();
}

DEFINE_TRACE(PasswordCredential)
{
    Credential::trace(visitor);
}

} // namespace blink
