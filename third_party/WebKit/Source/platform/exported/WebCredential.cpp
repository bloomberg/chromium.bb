// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "public/platform/WebCredential.h"

#include "platform/credentialmanager/PlatformCredential.h"

namespace blink {

WebCredential::WebCredential(const WebString& id, const WebString& name, const WebURL& avatarURL)
    : m_platformCredential(PlatformCredential::create(id, name, avatarURL))
{
}

void WebCredential::assign(const WebCredential& other)
{
    m_platformCredential = other.m_platformCredential;
}

WebCredential::WebCredential(PlatformCredential* credential)
    : m_platformCredential(credential)
{
}

void WebCredential::reset()
{
    m_platformCredential.reset();
}

WebString WebCredential::id() const
{
    return m_platformCredential->id();
}

WebString WebCredential::name() const
{
    return m_platformCredential->name();
}

WebURL WebCredential::avatarURL() const
{
    return m_platformCredential->avatarURL();
}

} // namespace blink
