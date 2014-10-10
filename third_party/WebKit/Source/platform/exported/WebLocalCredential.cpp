// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "public/platform/WebLocalCredential.h"

#include "platform/credentialmanager/PlatformLocalCredential.h"

namespace blink {

WebLocalCredential::WebLocalCredential(const WebString& id, const WebString& name, const WebURL& avatarURL, const WebString& password)
    : WebCredential(PlatformLocalCredential::create(id, name, avatarURL, password))
{
}

void WebLocalCredential::assign(const WebLocalCredential& other)
{
    m_platformCredential = other.m_platformCredential;
}

WebString WebLocalCredential::password() const
{
    return static_cast<PlatformLocalCredential*>(m_platformCredential.get())->password();
}

WebLocalCredential::WebLocalCredential(PlatformCredential* credential)
    : WebCredential(credential)
{
}

WebLocalCredential& WebLocalCredential::operator=(PlatformCredential* credential)
{
    m_platformCredential = credential;
    return *this;
}

} // namespace blink

