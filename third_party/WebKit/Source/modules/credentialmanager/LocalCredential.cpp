// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/credentialmanager/LocalCredential.h"
#include "platform/credentialmanager/PlatformLocalCredential.h"

namespace blink {

LocalCredential* LocalCredential::create(const String& id, const String& name, const String& avatarURL, const String& password)
{
    return new LocalCredential(id, name, avatarURL, password);
}

LocalCredential::LocalCredential(const String& id, const String& name, const String& avatarURL, const String& password)
    : Credential(PlatformLocalCredential::create(id, name, avatarURL, password))
{
}

const String& LocalCredential::password() const
{
    return static_cast<PlatformLocalCredential*>(m_platformCredential.get())->password();
}

} // namespace blink
