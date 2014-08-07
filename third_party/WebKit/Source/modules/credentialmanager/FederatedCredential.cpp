// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/credentialmanager/FederatedCredential.h"

namespace blink {

FederatedCredential* FederatedCredential::create(const String& id, const String& name, const String& avatarURL, const String& federation)
{
    return new FederatedCredential(id, name, avatarURL, federation);
}

FederatedCredential::FederatedCredential(const String& id, const String& name, const String& avatarURL, const String& federation)
    : Credential(id, name, avatarURL)
    , m_federation(federation)
{
}

FederatedCredential::~FederatedCredential()
{
}

} // namespace blink
