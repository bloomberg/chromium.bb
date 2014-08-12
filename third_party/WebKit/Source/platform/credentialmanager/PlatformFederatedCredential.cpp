// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/credentialmanager/PlatformFederatedCredential.h"

namespace blink {

PlatformFederatedCredential* PlatformFederatedCredential::create(const String& id, const String& name, const KURL& avatarURL, const KURL& federation)
{
    return new PlatformFederatedCredential(id, name, avatarURL, federation);
}

PlatformFederatedCredential::PlatformFederatedCredential(const String& id, const String& name, const KURL& avatarURL, const KURL& federation)
    : PlatformCredential(id, name, avatarURL)
    , m_federation(federation)
{
}

PlatformFederatedCredential::~PlatformFederatedCredential()
{
}

} // namespace blink
