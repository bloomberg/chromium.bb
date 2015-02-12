// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/credentialmanager/LocalCredential.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/html/DOMFormData.h"
#include "platform/credentialmanager/PlatformLocalCredential.h"
#include "public/platform/WebCredential.h"
#include "public/platform/WebLocalCredential.h"

namespace blink {

LocalCredential* LocalCredential::create(WebLocalCredential* webLocalCredential)
{
    return new LocalCredential(webLocalCredential);
}

LocalCredential* LocalCredential::create(const String& id, const String& password, const String& name, const String& avatar, ExceptionState& exceptionState)
{
    KURL avatarURL = parseStringAsURL(avatar, exceptionState);
    if (exceptionState.hadException())
        return nullptr;
    return new LocalCredential(id, password, name, avatarURL);
}

LocalCredential::LocalCredential(WebLocalCredential* webLocalCredential)
    : Credential(webLocalCredential->platformCredential())
{
}

LocalCredential::LocalCredential(const String& id, const String& password, const String& name, const KURL& avatar)
    : Credential(PlatformLocalCredential::create(id, password, name, avatar))
    , m_formData(DOMFormData::create())
{
    m_formData->append("username", id);
    m_formData->append("password", password);
}

const String& LocalCredential::password() const
{
    return static_cast<PlatformLocalCredential*>(m_platformCredential.get())->password();
}

DEFINE_TRACE(LocalCredential)
{
    visitor->trace(m_formData);
    Credential::trace(visitor);
}

} // namespace blink
