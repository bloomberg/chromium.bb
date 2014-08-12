// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/credentialmanager/FederatedCredential.h"

#include "bindings/core/v8/ExceptionState.h"
#include "platform/credentialmanager/PlatformFederatedCredential.h"

namespace blink {

FederatedCredential* FederatedCredential::create(const String& id, const String& name, const String& avatar, const String& federation, ExceptionState& exceptionState)
{
    KURL avatarURL = parseStringAsURL(avatar, exceptionState);
    KURL federationURL = parseStringAsURL(federation, exceptionState);
    if (exceptionState.hadException())
        return nullptr;
    return new FederatedCredential(id, name, avatarURL, federationURL);
}

FederatedCredential::FederatedCredential(const String& id, const String& name, const KURL& avatar, const KURL& federation)
    : Credential(PlatformFederatedCredential::create(id, name, avatar, federation))
{
}

const KURL& FederatedCredential::federation() const
{
    return static_cast<PlatformFederatedCredential*>(m_platformCredential.get())->federation();
}

} // namespace blink
