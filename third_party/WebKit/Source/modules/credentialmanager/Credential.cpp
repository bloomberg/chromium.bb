// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/credentialmanager/Credential.h"

namespace blink {

Credential* Credential::create(const String& id, const String& name, const String& avatarURL)
{
    return new Credential(id, name, avatarURL);
}

Credential::Credential(PlatformCredential* credential)
    : m_platformCredential(credential)
{
}

Credential::Credential(const String& id, const String& name, const String& avatarURL)
    : m_platformCredential(PlatformCredential::create(id, name, avatarURL))
{
    ScriptWrappable::init(this);
}

void Credential::trace(Visitor* visitor)
{
    visitor->trace(m_platformCredential);
}

} // namespace blink
