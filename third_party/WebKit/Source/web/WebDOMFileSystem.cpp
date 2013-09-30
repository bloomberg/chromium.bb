/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebDOMFileSystem.h"

#include "V8DOMFileSystem.h"
#include "bindings/v8/WrapperTypeInfo.h"
#include "modules/filesystem/DOMFileSystem.h"
#include <v8.h>

using namespace WebCore;

namespace WebKit {

WebDOMFileSystem WebDOMFileSystem::fromV8Value(v8::Handle<v8::Value> value)
{
    if (!V8DOMFileSystem::HasInstanceInAnyWorld(value, v8::Isolate::GetCurrent()))
        return WebDOMFileSystem();
    v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(value);
    DOMFileSystem* domFileSystem = V8DOMFileSystem::toNative(object);
    ASSERT(domFileSystem);
    return WebDOMFileSystem(domFileSystem);
}

void WebDOMFileSystem::reset()
{
    m_private.reset();
}

void WebDOMFileSystem::assign(const WebDOMFileSystem& other)
{
    m_private = other.m_private;
}

WebString WebDOMFileSystem::name() const
{
    ASSERT(m_private.get());
    return m_private->name();
}

WebFileSystem::Type WebDOMFileSystem::type() const
{
    ASSERT(m_private.get());
    switch (m_private->type()) {
    case FileSystemTypeTemporary:
        return WebFileSystem::TypeTemporary;
    case FileSystemTypePersistent:
        return WebFileSystem::TypePersistent;
    case FileSystemTypeIsolated:
        return WebFileSystem::TypeIsolated;
    case FileSystemTypeExternal:
        return WebFileSystem::TypeExternal;
    default:
        ASSERT_NOT_REACHED();
        return WebFileSystem::TypeTemporary;
    }
}

WebURL WebDOMFileSystem::rootURL() const
{
    ASSERT(m_private.get());
    return m_private->rootURL();
}

WebDOMFileSystem::WebDOMFileSystem(const WTF::PassRefPtr<DOMFileSystem>& domFileSystem)
    : m_private(domFileSystem)
{
}

WebDOMFileSystem& WebDOMFileSystem::operator=(const WTF::PassRefPtr<WebCore::DOMFileSystem>& domFileSystem)
{
    m_private = domFileSystem;
    return *this;
}

WebDOMFileSystem::operator WTF::PassRefPtr<WebCore::DOMFileSystem>() const
{
    return m_private.get();
}

} // namespace WebKit
