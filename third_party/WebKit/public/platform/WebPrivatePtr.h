/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef WebPrivatePtr_h
#define WebPrivatePtr_h

#include "WebCommon.h"

#if INSIDE_BLINK
#include "wtf/PassRefPtr.h"
#endif

namespace WebKit {

// This class is an implementation detail of the WebKit API.  It exists
// to help simplify the implementation of WebKit interfaces that merely
// wrap a reference counted WebCore class.
//
// A typical implementation of a class which uses WebPrivatePtr might look like
// this:
//    class WebFoo {
//    public:
//        virtual ~WebFoo() { }  // Only necessary if WebFoo will be used as a
//                               // base class.
//        WebFoo() { }
//        WebFoo(const WebFoo& other) { assign(other); }
//        WebFoo& operator=(const WebFoo& other)
//        {
//            assign(other);
//            return *this;
//        }
//        BLINK_EXPORT void assign(const WebFoo&);  // Implemented in the body.
//
//        // Methods that are exposed to Chromium and which are specific to
//        // WebFoo go here.
//        BLINK_EXPORT doWebFooThing();
//
//        // Methods that are used only by other WebKit/chromium API classes
//        // should only be declared when INSIDE_BLINK is set.
//    #if INSIDE_BLINK
//        WebFoo(const WTF::PassRefPtr<WebCore::Foo>&);
//    #endif
//
//    private:
//        WebPrivatePtr<WebCore::Foo> m_private;
//    };
//
template <typename T>
class WebPrivatePtr {
public:
    WebPrivatePtr() : m_ptr(0) { }
    ~WebPrivatePtr() { BLINK_ASSERT(!m_ptr); }

    bool isNull() const { return !m_ptr; }

#if INSIDE_BLINK
    WebPrivatePtr(const PassRefPtr<T>& prp)
        : m_ptr(prp.leakRef())
    {
    }

    void reset()
    {
        assign(0);
    }

    WebPrivatePtr<T>& operator=(const WebPrivatePtr<T>& other)
    {
        T* p = other.m_ptr;
        if (p)
            p->ref();
        assign(p);
        return *this;
    }

    WebPrivatePtr<T>& operator=(const PassRefPtr<T>& prp)
    {
        assign(prp.leakRef());
        return *this;
    }

    T* get() const
    {
        return m_ptr;
    }

    T* operator->() const
    {
        ASSERT(m_ptr);
        return m_ptr;
    }
#endif

private:
#if INSIDE_BLINK
    void assign(T* p)
    {
        // p is already ref'd for us by the caller
        if (m_ptr)
            m_ptr->deref();
        m_ptr = p;
    }
#else
    // Disable the assignment operator; we define it above for when
    // INSIDE_BLINK is set, but we need to make sure that it is not
    // used outside there; the compiler-provided version won't handle reference
    // counting properly.
    WebPrivatePtr<T>& operator=(const WebPrivatePtr<T>& other);
#endif
    // Disable the copy constructor; classes that contain a WebPrivatePtr
    // should implement their copy constructor using assign().
    WebPrivatePtr(const WebPrivatePtr<T>&);

    T* m_ptr;
};

} // namespace WebKit

#endif
