/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/storage/Storage.h"

#include <wtf/PassRefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

PassRefPtr<Storage> Storage::create(Frame* frame, PassRefPtr<StorageArea> storageArea)
{
    return adoptRef(new Storage(frame, storageArea));
}

Storage::Storage(Frame* frame, PassRefPtr<StorageArea> storageArea)
    : DOMWindowProperty(frame)
    , m_storageArea(storageArea)
{
    ASSERT(m_frame);
    ASSERT(m_storageArea);
    ScriptWrappable::init(this);
    if (m_storageArea)
        m_storageArea->incrementAccessCount();
}

Storage::~Storage()
{
    if (m_storageArea)
        m_storageArea->decrementAccessCount();
}

String Storage::anonymousIndexedGetter(unsigned index, ExceptionCode& ec)
{
    return anonymousNamedGetter(String::number(index), ec);
}

String Storage::anonymousNamedGetter(const AtomicString& name, ExceptionCode& ec)
{
    ec = 0;
    bool found = contains(name, ec);
    if (ec || !found)
        return String();
    String result = getItem(name, ec);
    if (ec)
        return String();
    return result;
}

bool Storage::anonymousNamedSetter(const AtomicString& name, const AtomicString& value, ExceptionCode& ec)
{
    setItem(name, value, ec);
    return true;
}

bool Storage::anonymousIndexedSetter(unsigned index, const AtomicString& value, ExceptionCode& ec)
{
    return anonymousNamedSetter(String::number(index), value, ec);
}

bool Storage::anonymousNamedDeleter(const AtomicString& name, ExceptionCode& ec)
{
    bool found = contains(name, ec);
    if (!found || ec)
        return false;
    removeItem(name, ec);
    if (ec)
        return false;
    return true;
}

bool Storage::anonymousIndexedDeleter(unsigned index, ExceptionCode& ec)
{
    return anonymousNamedDeleter(String::number(index), ec);
}

void Storage::namedPropertyEnumerator(Vector<String>& names, ExceptionCode& ec)
{
    unsigned length = this->length(ec);
    if (ec)
        return;
    names.resize(length);
    for (unsigned i = 0; i < length; ++i) {
        String key = this->key(i, ec);
        if (ec)
            return;
        ASSERT(!key.isNull());
        String val = getItem(key, ec);
        if (ec)
            return;
        names[i] = key;
    }
}

bool Storage::namedPropertyQuery(const AtomicString& name, ExceptionCode& ec)
{
    if (name == "length")
        return false;
    bool found = contains(name, ec);
    if (ec || !found)
        return false;
    return true;
}

}
