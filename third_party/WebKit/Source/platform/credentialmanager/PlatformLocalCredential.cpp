// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/credentialmanager/PlatformLocalCredential.h"

namespace blink {

PlatformLocalCredential* PlatformLocalCredential::create(const String& id, const String& password, const String& name, const KURL& avatarURL)
{
    return new PlatformLocalCredential(id, password, name, avatarURL);
}

PlatformLocalCredential::PlatformLocalCredential(const String& id, const String& password, const String& name, const KURL& avatarURL)
    : PlatformCredential(id, name, avatarURL)
    , m_password(password)
{
}

PlatformLocalCredential::~PlatformLocalCredential()
{
}

} // namespace blink

