// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "public/platform/WebFederatedCredential.h"

#include "platform/credentialmanager/PlatformFederatedCredential.h"

namespace blink {
WebFederatedCredential::WebFederatedCredential(const WebString& id, const WebURL& federation, const WebString& name, const WebURL& avatarURL)
    : WebCredential(PlatformFederatedCredential::create(id, federation, name, avatarURL))
{
}

// FIXME: Throw this away once it's unused on the Chromium side.
WebFederatedCredential::WebFederatedCredential(const WebString& id, const WebString& name, const WebURL& avatarURL, const WebURL& federation)
    : WebCredential(PlatformFederatedCredential::create(id, federation, name, avatarURL))
{
}

void WebFederatedCredential::assign(const WebFederatedCredential& other)
{
    m_platformCredential = other.m_platformCredential;
}

WebURL WebFederatedCredential::federation() const
{
    return static_cast<PlatformFederatedCredential*>(m_platformCredential.get())->federation();
}

WebFederatedCredential::WebFederatedCredential(PlatformCredential* credential)
    : WebCredential(credential)
{
}

WebFederatedCredential& WebFederatedCredential::operator=(PlatformCredential* credential)
{
    m_platformCredential = credential;
    return *this;
}

} // namespace blink

