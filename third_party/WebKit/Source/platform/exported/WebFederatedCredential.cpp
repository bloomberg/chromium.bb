// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "public/platform/WebFederatedCredential.h"

#include "platform/credentialmanager/PlatformFederatedCredential.h"

namespace blink {

WebFederatedCredential::WebFederatedCredential(const WebString& id, const WebString& name, const WebString& avatarURL, const WebString& federation)
    : WebCredential(PlatformFederatedCredential::create(id, name, avatarURL, federation))
{
}

void WebFederatedCredential::assign(const WebFederatedCredential& other)
{
    m_platformCredential = other.m_platformCredential;
}

WebString WebFederatedCredential::federation() const
{
    return static_cast<PlatformFederatedCredential*>(m_platformCredential.get())->federation();
}

} // namespace blink

