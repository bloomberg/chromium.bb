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
#include "V8DirectoryEntry.h"
#include "V8FileEntry.h"
#include "WebFrameImpl.h"
#include "bindings/v8/WrapperTypeInfo.h"
#include "core/dom/Document.h"
#include "modules/filesystem/DOMFileSystem.h"
#include "modules/filesystem/DirectoryEntry.h"
#include "modules/filesystem/FileEntry.h"
#include <v8.h>

using namespace WebCore;

namespace blink {

WebDOMFileSystem WebDOMFileSystem::fromV8Value(v8::Handle<v8::Value> value)
{
    if (!V8DOMFileSystem::hasInstance(value, v8::Isolate::GetCurrent()))
        return WebDOMFileSystem();
    v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(value);
    DOMFileSystem* domFileSystem = V8DOMFileSystem::toNative(object);
    ASSERT(domFileSystem);
    return WebDOMFileSystem(domFileSystem);
}

WebDOMFileSystem WebDOMFileSystem::create(
    WebLocalFrame* frame,
    WebFileSystemType type,
    const WebString& name,
    const WebURL& rootURL,
    SerializableType serializableType)
{
    ASSERT(frame && toWebFrameImpl(frame)->frame());
    RefPtrWillBeRawPtr<DOMFileSystem> domFileSystem = DOMFileSystem::create(toWebFrameImpl(frame)->frame()->document(), name, static_cast<WebCore::FileSystemType>(type), rootURL);
    if (serializableType == SerializableTypeSerializable)
        domFileSystem->makeClonable();
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

v8::Handle<v8::Value> WebDOMFileSystem::toV8Value()
{
    if (!m_private.get())
        return v8::Handle<v8::Value>();
    return toV8(m_private.get(), v8::Handle<v8::Object>(), toIsolate(m_private->executionContext()));
}

v8::Handle<v8::Value> WebDOMFileSystem::createV8Entry(
    const WebString& path,
    EntryType entryType)
{
    if (!m_private.get())
        return v8::Handle<v8::Value>();
    if (entryType == EntryTypeDirectory)
        return toV8(DirectoryEntry::create(m_private.get(), path), v8::Handle<v8::Object>(), toIsolate(m_private->executionContext()));
    ASSERT(entryType == EntryTypeFile);
    return toV8(FileEntry::create(m_private.get(), path), v8::Handle<v8::Object>(), toIsolate(m_private->executionContext()));
}

WebDOMFileSystem::WebDOMFileSystem(const PassRefPtrWillBeRawPtr<DOMFileSystem>& domFileSystem)
    : m_private(domFileSystem)
{
}

WebDOMFileSystem& WebDOMFileSystem::operator=(const PassRefPtrWillBeRawPtr<WebCore::DOMFileSystem>& domFileSystem)
{
    m_private = domFileSystem;
    return *this;
}

} // namespace blink
