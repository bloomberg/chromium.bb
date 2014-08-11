// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/credentialmanager/FederatedCredential.h"
#include "platform/credentialmanager/PlatformFederatedCredential.h"

namespace blink {

FederatedCredential* FederatedCredential::create(const String& id, const String& name, const String& avatarURL, const String& federation)
{
    return new FederatedCredential(id, name, avatarURL, federation);
}

FederatedCredential::FederatedCredential(const String& id, const String& name, const String& avatarURL, const String& federation)
    : Credential(PlatformFederatedCredential::create(id, name, avatarURL, federation))
{
}

const String& FederatedCredential::federation() const
{
    return static_cast<PlatformFederatedCredential*>(m_platformCredential.get())->federation();
}

} // namespace blink
