// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/credentialmanager/PasswordCredential.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/html/DOMFormData.h"
#include "platform/credentialmanager/PlatformPasswordCredential.h"
#include "public/platform/WebCredential.h"
#include "public/platform/WebPasswordCredential.h"

namespace blink {

PasswordCredential* PasswordCredential::create(WebPasswordCredential* webPasswordCredential)
{
    return new PasswordCredential(webPasswordCredential);
}

PasswordCredential* PasswordCredential::create(const String& id, const String& password, const String& name, const String& avatar, ExceptionState& exceptionState)
{
    KURL avatarURL = parseStringAsURL(avatar, exceptionState);
    if (exceptionState.hadException())
        return nullptr;
    return new PasswordCredential(id, password, name, avatarURL);
}

PasswordCredential::PasswordCredential(WebPasswordCredential* webPasswordCredential)
    : Credential(webPasswordCredential->platformCredential())
{
}

PasswordCredential::PasswordCredential(const String& id, const String& password, const String& name, const KURL& avatar)
    : Credential(PlatformPasswordCredential::create(id, password, name, avatar))
    , m_formData(DOMFormData::create())
{
    m_formData->append("username", id);
    m_formData->append("password", password);
}

const String& PasswordCredential::password() const
{
    return static_cast<PlatformPasswordCredential*>(m_platformCredential.get())->password();
}

DEFINE_TRACE(PasswordCredential)
{
    visitor->trace(m_formData);
    Credential::trace(visitor);
}

} // namespace blink
