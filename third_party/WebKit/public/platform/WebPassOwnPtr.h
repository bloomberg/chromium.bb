// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPassOwnPtr_h
#define WebPassOwnPtr_h

#include "public/platform/WebCommon.h"
#include <cstddef>

#if INSIDE_BLINK
#include "wtf/PassOwnPtr.h"
#else
#include <base/memory/scoped_ptr.h>
#endif

namespace blink {

// WebPassOwnPtr<T> is used to pass a T pointer with ownership from chromium
// side to blink side. T's definition must be shared among all users
// (especially between chromium and blink).
// TODO(yhirano): Migrate to scoped_ptr or std::unique_ptr once the repository
// merge is done or C++11 std library is allowed.
template <typename T>
class WebPassOwnPtr final {
public:
    WebPassOwnPtr() : m_ptr(nullptr) {}
    WebPassOwnPtr(std::nullptr_t) : m_ptr(nullptr) {}
    // We need |const| to bind an rvalue. As a result, |m_ptr| needs to be
    // mutable because we manipulate it.
    template <typename U>
    WebPassOwnPtr(const WebPassOwnPtr<U>& o)
    {
        m_ptr = o.m_ptr;
        o.m_ptr = nullptr;
    }
    WebPassOwnPtr(const WebPassOwnPtr& o)
    {
        m_ptr = o.m_ptr;
        o.m_ptr = nullptr;
    }
    ~WebPassOwnPtr()
    {
        delete m_ptr;
    }
    WebPassOwnPtr& operator =(const WebPassOwnPtr&) = delete;

#if INSIDE_BLINK
    PassOwnPtr<T> release()
    {
        T* ptr = m_ptr;
        m_ptr = nullptr;
        return adoptPtr(ptr);
    }
#else
    operator scoped_ptr<T>()
    {
        T* ptr = m_ptr;
        m_ptr = nullptr;
        return scoped_ptr<T>(ptr);
    }
#endif // INSIDE_BLINK

    template <typename U> friend class WebPassOwnPtr;
    template <typename U> friend WebPassOwnPtr<U> adoptWebPtr(U*);

private:
    explicit WebPassOwnPtr(T* ptr) : m_ptr(ptr) {}

    // See the constructor comment to see why |mutable| is needed.
    mutable T* m_ptr;
};

template <typename T>
WebPassOwnPtr<T> adoptWebPtr(T* p) { return WebPassOwnPtr<T>(p); }

} // namespace blink

#endif
