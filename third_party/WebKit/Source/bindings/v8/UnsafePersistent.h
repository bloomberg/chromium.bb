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

#ifndef UnsafePersistent_h
#define UnsafePersistent_h

#include <v8.h>

namespace WebCore {

// An unsafe way to pass Persistent handles around. Do not use unless you know
// what you're doing. UnsafePersistent is only safe to use when we know that the
// memory pointed by the it is not going away: 1) When GC cannot happen while
// the UnsafePersistent is alive or 2) when there is a strong Persistent keeping
// the memory alive while the UnsafePersistent is alive.
template<typename T> class UnsafePersistent {
public:
    UnsafePersistent() : m_value(0) { }
    explicit UnsafePersistent(T* value) : m_value(value) { }
    explicit UnsafePersistent(v8::Persistent<T>& handle)
    {
        m_value = handle.ClearAndLeak();
    }

    UnsafePersistent(v8::Isolate* isolate, v8::Handle<T>& handle)
    {
        v8::Persistent<T> persistent(isolate, handle);
        m_value = persistent.ClearAndLeak();
    }

    T* value() const
    {
        return m_value;
    }

    // This is incredibly unsafe: the handle is valid only when this
    // UnsafePersistent is alive and valid (see class level comment).
    v8::Persistent<T>* persistent()
    {
        v8::Persistent<T>* handle = reinterpret_cast<v8::Persistent<T>*>(&m_value);
        return handle;
    }

    // FIXME: Remove this function, replace the usages with newLocal(). Do not
    // add code which calls this function.
    v8::Handle<T> deprecatedHandle()
    {
        v8::Handle<T>* handle = reinterpret_cast<v8::Handle<T>*>(&m_value);
        return *handle;
    }

    void dispose()
    {
        persistent()->Dispose();
        m_value = 0;
    }

    void clear()
    {
        m_value = 0;
    }

    v8::Local<T> newLocal(v8::Isolate* isolate)
    {
        return v8::Local<T>::New(isolate, *persistent());
    }

    bool isEmpty() const
    {
        return !m_value;
    }

private:
    T* m_value;
};

} // namespace WebCore

#endif // UnsafePersistent_h
