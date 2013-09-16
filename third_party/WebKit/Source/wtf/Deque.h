/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WTF_Deque_h
#define WTF_Deque_h

// FIXME: Could move what Vector and Deque share into a separate file.
// Deque doesn't actually use Vector.

#include "wtf/PassTraits.h"
#include "wtf/Vector.h"
#include <iterator>

namespace WTF {

    template<typename T, size_t inlineCapacity> class DequeIteratorBase;
    template<typename T, size_t inlineCapacity> class DequeIterator;
    template<typename T, size_t inlineCapacity> class DequeConstIterator;

    template<typename T, size_t inlineCapacity = 0>
    class Deque {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        typedef DequeIterator<T, inlineCapacity> iterator;
        typedef DequeConstIterator<T, inlineCapacity> const_iterator;
        typedef std::reverse_iterator<iterator> reverse_iterator;
        typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
        typedef PassTraits<T> Pass;
        typedef typename PassTraits<T>::PassType PassType;

        Deque();
        Deque(const Deque<T, inlineCapacity>&);
        Deque& operator=(const Deque<T, inlineCapacity>&);
        ~Deque();

        void swap(Deque<T, inlineCapacity>&);

        size_t size() const { return m_start <= m_end ? m_end - m_start : m_end + m_buffer.capacity() - m_start; }
        bool isEmpty() const { return m_start == m_end; }

        iterator begin() { return iterator(this, m_start); }
        iterator end() { return iterator(this, m_end); }
        const_iterator begin() const { return const_iterator(this, m_start); }
        const_iterator end() const { return const_iterator(this, m_end); }
        reverse_iterator rbegin() { return reverse_iterator(end()); }
        reverse_iterator rend() { return reverse_iterator(begin()); }
        const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
        const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }

        T& first() { ASSERT(m_start != m_end); return m_buffer.buffer()[m_start]; }
        const T& first() const { ASSERT(m_start != m_end); return m_buffer.buffer()[m_start]; }
        PassType takeFirst();

        T& last() { ASSERT(m_start != m_end); return *(--end()); }
        const T& last() const { ASSERT(m_start != m_end); return *(--end()); }

        template<typename U> void append(const U&);
        template<typename U> void prepend(const U&);
        void removeFirst();
        void remove(iterator&);
        void remove(const_iterator&);

        void clear();

        template<typename Predicate>
        iterator findIf(Predicate&);

    private:
        friend class DequeIteratorBase<T, inlineCapacity>;

        typedef VectorBuffer<T, inlineCapacity> Buffer;
        typedef VectorTypeOperations<T> TypeOperations;
        typedef DequeIteratorBase<T, inlineCapacity> IteratorBase;

        void remove(size_t position);
        void destroyAll();
        void expandCapacityIfNeeded();
        void expandCapacity();

        Buffer m_buffer;
        unsigned m_start;
        unsigned m_end;
    };

    template<typename T, size_t inlineCapacity = 0>
    class DequeIteratorBase {
    protected:
        DequeIteratorBase();
        DequeIteratorBase(const Deque<T, inlineCapacity>*, size_t);
        DequeIteratorBase(const DequeIteratorBase&);
        DequeIteratorBase& operator=(const DequeIteratorBase&);
        ~DequeIteratorBase();

        void assign(const DequeIteratorBase& other) { *this = other; }

        void increment();
        void decrement();

        T* before() const;
        T* after() const;

        bool isEqual(const DequeIteratorBase&) const;

    private:
        Deque<T, inlineCapacity>* m_deque;
        unsigned m_index;

        friend class Deque<T, inlineCapacity>;
    };

    template<typename T, size_t inlineCapacity = 0>
    class DequeIterator : public DequeIteratorBase<T, inlineCapacity> {
    private:
        typedef DequeIteratorBase<T, inlineCapacity> Base;
        typedef DequeIterator<T, inlineCapacity> Iterator;

    public:
        typedef ptrdiff_t difference_type;
        typedef T value_type;
        typedef T* pointer;
        typedef T& reference;
        typedef std::bidirectional_iterator_tag iterator_category;

        DequeIterator(Deque<T, inlineCapacity>* deque, size_t index) : Base(deque, index) { }

        DequeIterator(const Iterator& other) : Base(other) { }
        DequeIterator& operator=(const Iterator& other) { Base::assign(other); return *this; }

        T& operator*() const { return *Base::after(); }
        T* operator->() const { return Base::after(); }

        bool operator==(const Iterator& other) const { return Base::isEqual(other); }
        bool operator!=(const Iterator& other) const { return !Base::isEqual(other); }

        Iterator& operator++() { Base::increment(); return *this; }
        // postfix ++ intentionally omitted
        Iterator& operator--() { Base::decrement(); return *this; }
        // postfix -- intentionally omitted
    };

    template<typename T, size_t inlineCapacity = 0>
    class DequeConstIterator : public DequeIteratorBase<T, inlineCapacity> {
    private:
        typedef DequeIteratorBase<T, inlineCapacity> Base;
        typedef DequeConstIterator<T, inlineCapacity> Iterator;
        typedef DequeIterator<T, inlineCapacity> NonConstIterator;

    public:
        typedef ptrdiff_t difference_type;
        typedef T value_type;
        typedef const T* pointer;
        typedef const T& reference;
        typedef std::bidirectional_iterator_tag iterator_category;

        DequeConstIterator(const Deque<T, inlineCapacity>* deque, size_t index) : Base(deque, index) { }

        DequeConstIterator(const Iterator& other) : Base(other) { }
        DequeConstIterator(const NonConstIterator& other) : Base(other) { }
        DequeConstIterator& operator=(const Iterator& other) { Base::assign(other); return *this; }
        DequeConstIterator& operator=(const NonConstIterator& other) { Base::assign(other); return *this; }

        const T& operator*() const { return *Base::after(); }
        const T* operator->() const { return Base::after(); }

        bool operator==(const Iterator& other) const { return Base::isEqual(other); }
        bool operator!=(const Iterator& other) const { return !Base::isEqual(other); }

        Iterator& operator++() { Base::increment(); return *this; }
        // postfix ++ intentionally omitted
        Iterator& operator--() { Base::decrement(); return *this; }
        // postfix -- intentionally omitted
    };

    template<typename T, size_t inlineCapacity>
    inline Deque<T, inlineCapacity>::Deque()
        : m_start(0)
        , m_end(0)
    {
    }

    template<typename T, size_t inlineCapacity>
    inline Deque<T, inlineCapacity>::Deque(const Deque<T, inlineCapacity>& other)
        : m_start(other.m_start)
        , m_end(other.m_end)
        , m_buffer(other.m_buffer.capacity())
    {
        const T* otherBuffer = other.m_buffer.buffer();
        if (m_start <= m_end)
            TypeOperations::uninitializedCopy(otherBuffer + m_start, otherBuffer + m_end, m_buffer.buffer() + m_start);
        else {
            TypeOperations::uninitializedCopy(otherBuffer, otherBuffer + m_end, m_buffer.buffer());
            TypeOperations::uninitializedCopy(otherBuffer + m_start, otherBuffer + m_buffer.capacity(), m_buffer.buffer() + m_start);
        }
    }

    template<typename T, size_t inlineCapacity>
    void deleteAllValues(const Deque<T, inlineCapacity>& collection)
    {
        typedef typename Deque<T, inlineCapacity>::const_iterator iterator;
        iterator end = collection.end();
        for (iterator it = collection.begin(); it != end; ++it)
            delete *it;
    }

    template<typename T, size_t inlineCapacity>
    inline Deque<T, inlineCapacity>& Deque<T, inlineCapacity>::operator=(const Deque<T, inlineCapacity>& other)
    {
        // FIXME: This is inefficient if we're using an inline buffer and T is
        // expensive to copy since it will copy the buffer twice instead of once.
        Deque<T> copy(other);
        swap(copy);
        return *this;
    }

    template<typename T, size_t inlineCapacity>
    inline void Deque<T, inlineCapacity>::destroyAll()
    {
        if (m_start <= m_end)
            TypeOperations::destruct(m_buffer.buffer() + m_start, m_buffer.buffer() + m_end);
        else {
            TypeOperations::destruct(m_buffer.buffer(), m_buffer.buffer() + m_end);
            TypeOperations::destruct(m_buffer.buffer() + m_start, m_buffer.buffer() + m_buffer.capacity());
        }
    }

    template<typename T, size_t inlineCapacity>
    inline Deque<T, inlineCapacity>::~Deque()
    {
        destroyAll();
        m_buffer.destruct();
    }

    template<typename T, size_t inlineCapacity>
    inline void Deque<T, inlineCapacity>::swap(Deque<T, inlineCapacity>& other)
    {
        std::swap(m_start, other.m_start);
        std::swap(m_end, other.m_end);
        m_buffer.swap(other.m_buffer);
    }

    template<typename T, size_t inlineCapacity>
    inline void Deque<T, inlineCapacity>::clear()
    {
        destroyAll();
        m_start = 0;
        m_end = 0;
        m_buffer.deallocateBuffer(m_buffer.buffer());
        m_buffer.clearBufferPointer();
    }

    template<typename T, size_t inlineCapacity>
    template<typename Predicate>
    inline DequeIterator<T, inlineCapacity> Deque<T, inlineCapacity>::findIf(Predicate& predicate)
    {
        iterator end_iterator = end();
        for (iterator it = begin(); it != end_iterator; ++it) {
            if (predicate(*it))
                return it;
        }
        return end_iterator;
    }

    template<typename T, size_t inlineCapacity>
    inline void Deque<T, inlineCapacity>::expandCapacityIfNeeded()
    {
        if (m_start) {
            if (m_end + 1 != m_start)
                return;
        } else if (m_end) {
            if (m_end != m_buffer.capacity() - 1)
                return;
        } else if (m_buffer.capacity())
            return;

        expandCapacity();
    }

    template<typename T, size_t inlineCapacity>
    void Deque<T, inlineCapacity>::expandCapacity()
    {
        size_t oldCapacity = m_buffer.capacity();
        T* oldBuffer = m_buffer.buffer();
        m_buffer.allocateBuffer(std::max(static_cast<size_t>(16), oldCapacity + oldCapacity / 4 + 1));
        if (m_start <= m_end)
            TypeOperations::move(oldBuffer + m_start, oldBuffer + m_end, m_buffer.buffer() + m_start);
        else {
            TypeOperations::move(oldBuffer, oldBuffer + m_end, m_buffer.buffer());
            size_t newStart = m_buffer.capacity() - (oldCapacity - m_start);
            TypeOperations::move(oldBuffer + m_start, oldBuffer + oldCapacity, m_buffer.buffer() + newStart);
            m_start = newStart;
        }
        m_buffer.deallocateBuffer(oldBuffer);
    }

    template<typename T, size_t inlineCapacity>
    inline typename Deque<T, inlineCapacity>::PassType Deque<T, inlineCapacity>::takeFirst()
    {
        T oldFirst = Pass::transfer(first());
        removeFirst();
        return Pass::transfer(oldFirst);
    }

    template<typename T, size_t inlineCapacity> template<typename U>
    inline void Deque<T, inlineCapacity>::append(const U& value)
    {
        expandCapacityIfNeeded();
        new (NotNull, &m_buffer.buffer()[m_end]) T(value);
        if (m_end == m_buffer.capacity() - 1)
            m_end = 0;
        else
            ++m_end;
    }

    template<typename T, size_t inlineCapacity> template<typename U>
    inline void Deque<T, inlineCapacity>::prepend(const U& value)
    {
        expandCapacityIfNeeded();
        if (!m_start)
            m_start = m_buffer.capacity() - 1;
        else
            --m_start;
        new (NotNull, &m_buffer.buffer()[m_start]) T(value);
    }

    template<typename T, size_t inlineCapacity>
    inline void Deque<T, inlineCapacity>::removeFirst()
    {
        ASSERT(!isEmpty());
        TypeOperations::destruct(&m_buffer.buffer()[m_start], &m_buffer.buffer()[m_start + 1]);
        if (m_start == m_buffer.capacity() - 1)
            m_start = 0;
        else
            ++m_start;
    }

    template<typename T, size_t inlineCapacity>
    inline void Deque<T, inlineCapacity>::remove(iterator& it)
    {
        remove(it.m_index);
    }

    template<typename T, size_t inlineCapacity>
    inline void Deque<T, inlineCapacity>::remove(const_iterator& it)
    {
        remove(it.m_index);
    }

    template<typename T, size_t inlineCapacity>
    inline void Deque<T, inlineCapacity>::remove(size_t position)
    {
        if (position == m_end)
            return;

        T* buffer = m_buffer.buffer();
        TypeOperations::destruct(&buffer[position], &buffer[position + 1]);

        // Find which segment of the circular buffer contained the remove element, and only move elements in that part.
        if (position >= m_start) {
            TypeOperations::moveOverlapping(buffer + m_start, buffer + position, buffer + m_start + 1);
            m_start = (m_start + 1) % m_buffer.capacity();
        } else {
            TypeOperations::moveOverlapping(buffer + position + 1, buffer + m_end, buffer + position);
            m_end = (m_end - 1 + m_buffer.capacity()) % m_buffer.capacity();
        }
    }

    template<typename T, size_t inlineCapacity>
    inline DequeIteratorBase<T, inlineCapacity>::DequeIteratorBase()
        : m_deque(0)
    {
    }

    template<typename T, size_t inlineCapacity>
    inline DequeIteratorBase<T, inlineCapacity>::DequeIteratorBase(const Deque<T, inlineCapacity>* deque, size_t index)
        : m_deque(const_cast<Deque<T, inlineCapacity>*>(deque))
        , m_index(index)
    {
    }

    template<typename T, size_t inlineCapacity>
    inline DequeIteratorBase<T, inlineCapacity>::DequeIteratorBase(const DequeIteratorBase& other)
        : m_deque(other.m_deque)
        , m_index(other.m_index)
    {
    }

    template<typename T, size_t inlineCapacity>
    inline DequeIteratorBase<T, inlineCapacity>& DequeIteratorBase<T, inlineCapacity>::operator=(const DequeIteratorBase& other)
    {
        m_deque = other.m_deque;
        m_index = other.m_index;
        return *this;
    }

    template<typename T, size_t inlineCapacity>
    inline DequeIteratorBase<T, inlineCapacity>::~DequeIteratorBase()
    {
    }

    template<typename T, size_t inlineCapacity>
    inline bool DequeIteratorBase<T, inlineCapacity>::isEqual(const DequeIteratorBase& other) const
    {
        return m_index == other.m_index;
    }

    template<typename T, size_t inlineCapacity>
    inline void DequeIteratorBase<T, inlineCapacity>::increment()
    {
        ASSERT(m_index != m_deque->m_end);
        ASSERT(m_deque->m_buffer.capacity());
        if (m_index == m_deque->m_buffer.capacity() - 1)
            m_index = 0;
        else
            ++m_index;
    }

    template<typename T, size_t inlineCapacity>
    inline void DequeIteratorBase<T, inlineCapacity>::decrement()
    {
        ASSERT(m_index != m_deque->m_start);
        ASSERT(m_deque->m_buffer.capacity());
        if (!m_index)
            m_index = m_deque->m_buffer.capacity() - 1;
        else
            --m_index;
    }

    template<typename T, size_t inlineCapacity>
    inline T* DequeIteratorBase<T, inlineCapacity>::after() const
    {
        ASSERT(m_index != m_deque->m_end);
        return &m_deque->m_buffer.buffer()[m_index];
    }

    template<typename T, size_t inlineCapacity>
    inline T* DequeIteratorBase<T, inlineCapacity>::before() const
    {
        ASSERT(m_index != m_deque->m_start);
        if (!m_index)
            return &m_deque->m_buffer.buffer()[m_deque->m_buffer.capacity() - 1];
        return &m_deque->m_buffer.buffer()[m_index - 1];
    }

} // namespace WTF

using WTF::Deque;

#endif // WTF_Deque_h
