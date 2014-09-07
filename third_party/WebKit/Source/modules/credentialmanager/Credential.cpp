// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/credentialmanager/Credential.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"

namespace blink {

Credential* Credential::create(const String& id, const String& name, const KURL& avatar)
{
    return new Credential(id, name, avatar);
}

Credential* Credential::create(const String& id, const String& name, const String& avatar, ExceptionState& exceptionState)
{
    KURL avatarURL = parseStringAsURL(avatar, exceptionState);
    if (exceptionState.hadException())
        return nullptr;
    return new Credential(id, name, avatarURL);
}

Credential::Credential(PlatformCredential* credential)
    : m_platformCredential(credential)
{
}

Credential::Credential(const String& id, const String& name, const KURL& avatar)
    : m_platformCredential(PlatformCredential::create(id, name, avatar))
{
}

KURL Credential::parseStringAsURL(const String& url, ExceptionState& exceptionState)
{
    KURL parsedURL = KURL(KURL(), url);
    if (!parsedURL.isValid())
        exceptionState.throwDOMException(SyntaxError, "'" + url + "' is not a valid URL.");
    return parsedURL;
}

void Credential::trace(Visitor* visitor)
{
    visitor->trace(m_platformCredential);
}

} // namespace blink
