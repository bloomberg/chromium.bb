/*
 *  Copyright (C) 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 *  Copyright (C) 2013 Intel Corporation. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef WTF_OwnPtr_h
#define WTF_OwnPtr_h

#include "wtf/Allocator.h"
#include "wtf/HashTableDeletedValueType.h"
#include "wtf/Noncopyable.h"
#include "wtf/OwnPtrCommon.h"
#include <algorithm>
#include <utility>

namespace WTF {

template <typename T> class PassOwnPtr;

template <typename T> class OwnPtr {
    USING_FAST_MALLOC(OwnPtr);
    WTF_MAKE_NONCOPYABLE(OwnPtr);
public:
    typedef typename std::remove_extent<T>::type ValueType;
    typedef ValueType* PtrType;

    OwnPtr() : m_ptr(nullptr) {}
    OwnPtr(std::nullptr_t) : m_ptr(nullptr) {}

    OwnPtr(PassOwnPtr<T>&&);
    template <typename U> OwnPtr(PassOwnPtr<U>&&, EnsurePtrConvertibleArgDecl(U, T));

    // Hash table deleted values, which are only constructed and never copied or
    // destroyed.
    OwnPtr(HashTableDeletedValueType) : m_ptr(hashTableDeletedValue()) {}
    bool isHashTableDeletedValue() const { return m_ptr == hashTableDeletedValue(); }

    ~OwnPtr()
    {
        OwnedPtrDeleter<T>::deletePtr(m_ptr);
        m_ptr = nullptr;
    }

    PtrType get() const { return m_ptr; }

    void clear();
    PassOwnPtr<T> release();
    PtrType leakPtr() WARN_UNUSED_RETURN;

    ValueType& operator*() const { ASSERT(m_ptr); return *m_ptr; }
    PtrType operator->() const { ASSERT(m_ptr); return m_ptr; }

    ValueType& operator[](std::ptrdiff_t i) const;

    bool operator!() const { return !m_ptr; }
    explicit operator bool() const { return m_ptr; }

    OwnPtr& operator=(PassOwnPtr<T>&&);
    OwnPtr& operator=(std::nullptr_t) { clear(); return *this; }
    template <typename U> OwnPtr& operator=(PassOwnPtr<U>&&);

    OwnPtr(OwnPtr&&);
    template <typename U> OwnPtr(OwnPtr<U>&&);

    OwnPtr& operator=(OwnPtr&&);
    template <typename U> OwnPtr& operator=(OwnPtr<U>&&);

    void swap(OwnPtr& o) { std::swap(m_ptr, o.m_ptr); }

    static T* hashTableDeletedValue() { return reinterpret_cast<T*>(-1); }

private:
    // We should never have two OwnPtrs for the same underlying object
    // (otherwise we'll get double-destruction), so these equality operators
    // should never be needed.
    template <typename U> bool operator==(const OwnPtr<U>&) const
    {
        static_assert(!sizeof(U*), "OwnPtrs should never be equal");
        return false;
    }
    template <typename U> bool operator!=(const OwnPtr<U>&) const
    {
        static_assert(!sizeof(U*), "OwnPtrs should never be equal");
        return false;
    }
    template <typename U> bool operator==(const PassOwnPtr<U>&) const
    {
        static_assert(!sizeof(U*), "OwnPtrs should never be equal");
        return false;
    }
    template <typename U> bool operator!=(const PassOwnPtr<U>&) const
    {
        static_assert(!sizeof(U*), "OwnPtrs should never be equal");
        return false;
    }

    PtrType m_ptr;
};

template <typename T> inline OwnPtr<T>::OwnPtr(PassOwnPtr<T>&& o)
    : m_ptr(o.leakPtr())
{
}

template <typename T>
template <typename U> inline OwnPtr<T>::OwnPtr(PassOwnPtr<U>&& o, EnsurePtrConvertibleArgDefn(U, T))
    : m_ptr(o.leakPtr())
{
    static_assert(!std::is_array<T>::value, "pointers to array must never be converted");
}

template <typename T> inline void OwnPtr<T>::clear()
{
    PtrType ptr = m_ptr;
    m_ptr = nullptr;
    OwnedPtrDeleter<T>::deletePtr(ptr);
}

template <typename T> inline PassOwnPtr<T> OwnPtr<T>::release()
{
    PtrType ptr = m_ptr;
    m_ptr = nullptr;
    return PassOwnPtr<T>(ptr);
}

template <typename T> inline typename OwnPtr<T>::PtrType OwnPtr<T>::leakPtr()
{
    PtrType ptr = m_ptr;
    m_ptr = nullptr;
    return ptr;
}

template <typename T> inline typename OwnPtr<T>::ValueType& OwnPtr<T>::operator[](std::ptrdiff_t i) const
{
    static_assert(std::is_array<T>::value, "elements access is possible for arrays only");
    ASSERT(m_ptr);
    ASSERT(i >= 0);
    return m_ptr[i];
}

template <typename T> inline OwnPtr<T>& OwnPtr<T>::operator=(PassOwnPtr<T>&& o)
{
    PtrType ptr = m_ptr;
    m_ptr = o.leakPtr();
    ASSERT(!ptr || m_ptr != ptr);
    OwnedPtrDeleter<T>::deletePtr(ptr);
    return *this;
}

template <typename T>
template <typename U> inline OwnPtr<T>& OwnPtr<T>::operator=(PassOwnPtr<U>&& o)
{
    static_assert(!std::is_array<T>::value, "pointers to array must never be converted");
    PtrType ptr = m_ptr;
    m_ptr = o.leakPtr();
    ASSERT(!ptr || m_ptr != ptr);
    OwnedPtrDeleter<T>::deletePtr(ptr);
    return *this;
}

template <typename T> inline OwnPtr<T>::OwnPtr(OwnPtr<T>&& o)
    : m_ptr(o.leakPtr())
{
}

template <typename T>
template <typename U> inline OwnPtr<T>::OwnPtr(OwnPtr<U>&& o)
    : m_ptr(o.leakPtr())
{
    static_assert(!std::is_array<T>::value, "pointers to array must never be converted");
}

template <typename T> inline OwnPtr<T>& OwnPtr<T>::operator=(OwnPtr<T>&& o)
{
    PtrType ptr = m_ptr;
    m_ptr = o.leakPtr();
    ASSERT(!ptr || m_ptr != ptr);
    OwnedPtrDeleter<T>::deletePtr(ptr);

    return *this;
}

template <typename T>
template <typename U> inline OwnPtr<T>& OwnPtr<T>::operator=(OwnPtr<U>&& o)
{
    static_assert(!std::is_array<T>::value, "pointers to array must never be converted");
    PtrType ptr = m_ptr;
    m_ptr = o.leakPtr();
    ASSERT(!ptr || m_ptr != ptr);
    OwnedPtrDeleter<T>::deletePtr(ptr);

    return *this;
}

template <typename T> inline void swap(OwnPtr<T>& a, OwnPtr<T>& b)
{
    a.swap(b);
}

template <typename T, typename U> inline bool operator==(const OwnPtr<T>& a, U* b)
{
    return a.get() == b;
}

template <typename T, typename U> inline bool operator==(T* a, const OwnPtr<U>& b)
{
    return a == b.get();
}

template <typename T, typename U> inline bool operator!=(const OwnPtr<T>& a, U* b)
{
    return a.get() != b;
}

template <typename T, typename U> inline bool operator!=(T* a, const OwnPtr<U>& b)
{
    return a != b.get();
}

template <typename T> inline typename OwnPtr<T>::PtrType getPtr(const OwnPtr<T>& p)
{
    return p.get();
}

} // namespace WTF

using WTF::OwnPtr;

#endif // WTF_OwnPtr_h
