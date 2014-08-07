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

Credential::Credential(const String& id, const String& name, const String& avatarURL)
    : m_id(id)
    , m_name(name)
    , m_avatarURL(avatarURL)
{
    ScriptWrappable::init(this);
}

Credential::~Credential()
{
}

} // namespace blink
