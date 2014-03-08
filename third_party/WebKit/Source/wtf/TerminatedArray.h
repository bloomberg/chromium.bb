// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef TerminatedArray_h
#define TerminatedArray_h

#include "wtf/FastAllocBase.h"
#include "wtf/OwnPtr.h"

namespace WTF {

// TerminatedArray<T> represents a sequence of elements of type T in which each
// element knows whether it is the last element in the sequence or not. For this
// check type T must provide isLastInArray method.
// TerminatedArray<T> can only be constructed by TerminatedArrayBuilder<T>.
template<typename T>
class TerminatedArray {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(TerminatedArray);
public:
    T& at(size_t index) { return reinterpret_cast<T*>(this)[index]; }
    const T& at(size_t index) const { return reinterpret_cast<const T*>(this)[index]; }

    template<typename U>
    class iterator_base {
    public:
        iterator_base& operator++()
        {
            if (m_val->isLastInArray()) {
                m_val = 0;
            } else {
                ++m_val;
            }
            return *this;
        }

        U& operator*() const { return *m_val; }

        bool operator==(const iterator_base& other) const { return m_val == other.m_val; }
        bool operator!=(const iterator_base& other) const { return !(*this == other); }

    private:
        iterator_base(U* val) : m_val(val) { }

        U* m_val;

        friend class TerminatedArray;
    };

    typedef iterator_base<T> iterator;
    typedef iterator_base<const T> const_iterator;

    iterator begin() { return iterator(reinterpret_cast<T*>(this)); }
    const_iterator begin() const { return const_iterator(reinterpret_cast<const T*>(this)); }

    iterator end() { return iterator(0); }
    const_iterator end() const { return const_iterator(0); }

    size_t size() const
    {
        size_t count = 0;
        for (const_iterator it = begin(); it != end(); ++it)
            count++;
        return count;
    }

private:
    static PassOwnPtr<TerminatedArray> create(size_t capacity)
    {
        return adoptPtr(static_cast<TerminatedArray*>(fastMalloc(capacity * sizeof(T))));
    }

    static PassOwnPtr<TerminatedArray> resize(PassOwnPtr<TerminatedArray> array, size_t capacity)
    {
        return adoptPtr(static_cast<TerminatedArray*>(fastRealloc(array.leakPtr(), capacity * sizeof(T))));
    }

    template<typename> friend class TerminatedArrayBuilder;
};

} // namespace WTF

using WTF::TerminatedArray;

#endif // TerminatedArray_h
