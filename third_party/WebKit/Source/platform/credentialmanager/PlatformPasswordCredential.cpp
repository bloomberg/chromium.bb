// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/credentialmanager/PlatformPasswordCredential.h"

namespace blink {

PlatformPasswordCredential* PlatformPasswordCredential::create(const String& id, const String& password, const String& name, const KURL& avatarURL)
{
    return new PlatformPasswordCredential(id, password, name, avatarURL);
}

PlatformPasswordCredential::PlatformPasswordCredential(const String& id, const String& password, const String& name, const KURL& avatarURL)
    : PlatformCredential(id, name, avatarURL)
    , m_password(password)
{
    setType("password");
}

PlatformPasswordCredential::~PlatformPasswordCredential()
{
}

} // namespace blink

