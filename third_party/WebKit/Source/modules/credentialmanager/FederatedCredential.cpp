// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/credentialmanager/FederatedCredential.h"

#include "bindings/core/v8/ExceptionState.h"
#include "platform/credentialmanager/PlatformFederatedCredential.h"
#include "public/platform/WebFederatedCredential.h"

namespace blink {

FederatedCredential* FederatedCredential::create(WebFederatedCredential* webFederatedCredential)
{
    return new FederatedCredential(webFederatedCredential);
}

FederatedCredential* FederatedCredential::create(const String& id, const String& federation, const String& name, const String& avatar, ExceptionState& exceptionState)
{
    KURL avatarURL = parseStringAsURL(avatar, exceptionState);
    KURL federationURL = parseStringAsURL(federation, exceptionState);
    if (exceptionState.hadException())
        return nullptr;
    return new FederatedCredential(id, federationURL, name, avatarURL);
}

FederatedCredential::FederatedCredential(WebFederatedCredential* webFederatedCredential)
    : Credential(webFederatedCredential->platformCredential())
{
}

FederatedCredential::FederatedCredential(const String& id, const KURL& federation, const String& name, const KURL& avatar)
    : Credential(PlatformFederatedCredential::create(id, federation, name, avatar))
{
}

const KURL& FederatedCredential::federation() const
{
    return static_cast<PlatformFederatedCredential*>(m_platformCredential.get())->federation();
}

} // namespace blink
