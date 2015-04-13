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
    template<typename T, size_t inlineCapacity, typename Allocator, bool noDestructor> class DequeIteratorBase;
    template<typename T, size_t inlineCapacity, typename Allocator, bool noDestructor> class DequeIterator;
    template<typename T, size_t inlineCapacity, typename Allocator, bool noDestructor> class DequeConstIterator;

    template<typename T, size_t inlineCapacity = 0, typename Allocator = DefaultAllocator, bool noDestructor = false>
    class Deque : public ConditionalDestructor<Deque<T, INLINE_CAPACITY, Allocator, noDestructor>, noDestructor> {
        WTF_USE_ALLOCATOR(Deque, Allocator);
    public:
        typedef DequeIterator<T, inlineCapacity, Allocator, noDestructor> iterator;
        typedef DequeConstIterator<T, inlineCapacity, Allocator, noDestructor> const_iterator;
        typedef std::reverse_iterator<iterator> reverse_iterator;
        typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
        typedef PassTraits<T> Pass;
        typedef typename PassTraits<T>::PassType PassType;

        Deque();
        Deque(const Deque<T, inlineCapacity, Allocator, noDestructor>&);
        // FIXME: Doesn't work if there is an inline buffer, due to crbug.com/360572
        Deque<T, 0, Allocator, noDestructor>& operator=(const Deque&);

        void finalize();
        void finalizeGarbageCollectedObject() { finalize(); }

        // We hard wire the inlineCapacity to zero here, due to crbug.com/360572
        void swap(Deque<T, 0, Allocator, noDestructor>&);

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
        PassType takeLast();

        T& at(size_t i)
        {
            RELEASE_ASSERT(i < size());
            size_t right = m_buffer.capacity() - m_start;
            return i < right ? m_buffer.buffer()[m_start + i] : m_buffer.buffer()[i - right];
        }
        const T& at(size_t i) const
        {
            RELEASE_ASSERT(i < size());
            size_t right = m_buffer.capacity() - m_start;
            return i < right ? m_buffer.buffer()[m_start + i] : m_buffer.buffer()[i - right];
        }

        T& operator[](size_t i) { return at(i); }
        const T& operator[](size_t i) const { return at(i); }

        template<typename U> void append(const U&);
        template<typename U> void prepend(const U&);
        void removeFirst();
        void removeLast();
        void remove(iterator&);
        void remove(const_iterator&);

        void clear();

        template<typename Predicate>
        iterator findIf(Predicate&);

        typedef int HasInlinedTraceMethodMarker;
        template<typename VisitorDispatcher> void trace(VisitorDispatcher);

    private:
        friend class DequeIteratorBase<T, inlineCapacity, Allocator, noDestructor>;

        typedef VectorBuffer<T, INLINE_CAPACITY, Allocator> Buffer;
        typedef VectorTypeOperations<T> TypeOperations;
        typedef DequeIteratorBase<T, inlineCapacity, Allocator, noDestructor> IteratorBase;

        void remove(size_t position);
        void destroyAll();
        void expandCapacityIfNeeded();
        void expandCapacity();

        Buffer m_buffer;
        unsigned m_start;
        unsigned m_end;
    };

    template<typename T, size_t inlineCapacity, typename Allocator, bool noDestructor>
    class DequeIteratorBase {
    protected:
        DequeIteratorBase();
        DequeIteratorBase(const Deque<T, inlineCapacity, Allocator, noDestructor>*, size_t);
        DequeIteratorBase(const DequeIteratorBase&);
        DequeIteratorBase<T, 0, Allocator, noDestructor>& operator=(const DequeIteratorBase<T, 0, Allocator, noDestructor>&);
        ~DequeIteratorBase();

        void assign(const DequeIteratorBase& other) { *this = other; }

        void increment();
        void decrement();

        T* before() const;
        T* after() const;

        bool isEqual(const DequeIteratorBase&) const;

    private:
        Deque<T, inlineCapacity, Allocator, noDestructor>* m_deque;
        unsigned m_index;

        friend class Deque<T, inlineCapacity, Allocator, noDestructor>;
    };

    template<typename T, size_t inlineCapacity = 0, typename Allocator = DefaultAllocator, bool noDestructor = false>
    class DequeIterator : public DequeIteratorBase<T, inlineCapacity, Allocator, noDestructor> {
    private:
        typedef DequeIteratorBase<T, inlineCapacity, Allocator, noDestructor> Base;
        typedef DequeIterator<T, inlineCapacity, Allocator, noDestructor> Iterator;

    public:
        typedef ptrdiff_t difference_type;
        typedef T value_type;
        typedef T* pointer;
        typedef T& reference;
        typedef std::bidirectional_iterator_tag iterator_category;

        DequeIterator(Deque<T, inlineCapacity, Allocator, noDestructor>* deque, size_t index) : Base(deque, index) { }

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

    template<typename T, size_t inlineCapacity = 0, typename Allocator = DefaultAllocator, bool noDestructor = false>
    class DequeConstIterator : public DequeIteratorBase<T, inlineCapacity, Allocator, noDestructor> {
    private:
        typedef DequeIteratorBase<T, inlineCapacity, Allocator, noDestructor> Base;
        typedef DequeConstIterator<T, inlineCapacity, Allocator, noDestructor> Iterator;
        typedef DequeIterator<T, inlineCapacity, Allocator, noDestructor> NonConstIterator;

    public:
        typedef ptrdiff_t difference_type;
        typedef T value_type;
        typedef const T* pointer;
        typedef const T& reference;
        typedef std::bidirectional_iterator_tag iterator_category;

        DequeConstIterator(const Deque<T, inlineCapacity, Allocator, noDestructor>* deque, size_t index) : Base(deque, index) { }

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

    template<typename T, size_t inlineCapacity, typename Allocator, bool noDestructor>
    inline Deque<T, inlineCapacity, Allocator, noDestructor>::Deque()
        : m_start(0)
        , m_end(0)
    {
    }

    template<typename T, size_t inlineCapacity, typename Allocator, bool noDestructor>
    inline Deque<T, inlineCapacity, Allocator, noDestructor>::Deque(const Deque<T, inlineCapacity, Allocator, noDestructor>& other)
        : m_buffer(other.m_buffer.capacity())
        , m_start(other.m_start)
        , m_end(other.m_end)
    {
        const T* otherBuffer = other.m_buffer.buffer();
        if (m_start <= m_end)
            TypeOperations::uninitializedCopy(otherBuffer + m_start, otherBuffer + m_end, m_buffer.buffer() + m_start);
        else {
            TypeOperations::uninitializedCopy(otherBuffer, otherBuffer + m_end, m_buffer.buffer());
            TypeOperations::uninitializedCopy(otherBuffer + m_start, otherBuffer + m_buffer.capacity(), m_buffer.buffer() + m_start);
        }
    }

    template<typename T, size_t inlineCapacity, typename Allocator, bool noDestructor>
    inline Deque<T, 0, Allocator, noDestructor>& Deque<T, inlineCapacity, Allocator, noDestructor>::operator=(const Deque& other)
    {
        Deque<T> copy(other);
        swap(copy);
        return *this;
    }

    template<typename T, size_t inlineCapacity, typename Allocator, bool noDestructor>
    inline void Deque<T, inlineCapacity, Allocator, noDestructor>::destroyAll()
    {
        if (m_start <= m_end) {
            TypeOperations::destruct(m_buffer.buffer() + m_start, m_buffer.buffer() + m_end);
        } else {
            TypeOperations::destruct(m_buffer.buffer(), m_buffer.buffer() + m_end);
            TypeOperations::destruct(m_buffer.buffer() + m_start, m_buffer.buffer() + m_buffer.capacity());
        }
    }

    // Off-GC-heap deques: Destructor should be called.
    // On-GC-heap deques: Destructor should be called if T needs a destructor
    // or the deque has an inlined buffer.
    template<typename T, size_t inlineCapacity, typename Allocator, bool noDestructor>
    inline void Deque<T, inlineCapacity, Allocator, noDestructor>::finalize()
    {
        if (!INLINE_CAPACITY && !m_buffer.buffer())
            return;
        if (!isEmpty() && (!Allocator::isGarbageCollected || VectorTraits<T>::needsDestruction || !m_buffer.hasOutOfLineBuffer()))
            destroyAll();

        m_buffer.destruct();
    }

    // FIXME: Doesn't work if there is an inline buffer, due to crbug.com/360572
    template<typename T, size_t inlineCapacity, typename Allocator, bool noDestructor>
    inline void Deque<T, inlineCapacity, Allocator, noDestructor>::swap(Deque<T, 0, Allocator, noDestructor>& other)
    {
        std::swap(m_start, other.m_start);
        std::swap(m_end, other.m_end);
        m_buffer.swapVectorBuffer(other.m_buffer);
    }

    template<typename T, size_t inlineCapacity, typename Allocator, bool noDestructor>
    inline void Deque<T, inlineCapacity, Allocator, noDestructor>::clear()
    {
        destroyAll();
        m_start = 0;
        m_end = 0;
        m_buffer.deallocateBuffer(m_buffer.buffer());
        m_buffer.resetBufferPointer();
    }

    template<typename T, size_t inlineCapacity, typename Allocator, bool noDestructor>
    template<typename Predicate>
    inline DequeIterator<T, inlineCapacity, Allocator, noDestructor> Deque<T, inlineCapacity, Allocator, noDestructor>::findIf(Predicate& predicate)
    {
        iterator end_iterator = end();
        for (iterator it = begin(); it != end_iterator; ++it) {
            if (predicate(*it))
                return it;
        }
        return end_iterator;
    }

    template<typename T, size_t inlineCapacity, typename Allocator, bool noDestructor>
    inline void Deque<T, inlineCapacity, Allocator, noDestructor>::expandCapacityIfNeeded()
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

    template<typename T, size_t inlineCapacity, typename Allocator, bool noDestructor>
    void Deque<T, inlineCapacity, Allocator, noDestructor>::expandCapacity()
    {
        size_t oldCapacity = m_buffer.capacity();
        T* oldBuffer = m_buffer.buffer();
        size_t newCapacity = std::max(static_cast<size_t>(16), oldCapacity + oldCapacity / 4 + 1);
        if (m_buffer.expandBuffer(newCapacity)) {
            if (m_start <= m_end) {
                // No adjustments to be done.
            } else {
                size_t newStart = m_buffer.capacity() - (oldCapacity - m_start);
                TypeOperations::moveOverlapping(oldBuffer + m_start, oldBuffer + oldCapacity, m_buffer.buffer() + newStart);
                m_start = newStart;
            }
            return;
        }
        m_buffer.allocateBuffer(newCapacity);
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

    template<typename T, size_t inlineCapacity, typename Allocator, bool noDestructor>
    inline typename Deque<T, inlineCapacity, Allocator, noDestructor>::PassType Deque<T, inlineCapacity, Allocator, noDestructor>::takeFirst()
    {
        T oldFirst = Pass::transfer(first());
        removeFirst();
        return Pass::transfer(oldFirst);
    }

    template<typename T, size_t inlineCapacity, typename Allocator, bool noDestructor>
    inline typename Deque<T, inlineCapacity, Allocator, noDestructor>::PassType Deque<T, inlineCapacity, Allocator, noDestructor>::takeLast()
    {
        T oldLast = Pass::transfer(last());
        removeLast();
        return Pass::transfer(oldLast);
    }

    template<typename T, size_t inlineCapacity, typename Allocator, bool noDestructor> template<typename U>
    inline void Deque<T, inlineCapacity, Allocator, noDestructor>::append(const U& value)
    {
        expandCapacityIfNeeded();
        new (NotNull, &m_buffer.buffer()[m_end]) T(value);
        if (m_end == m_buffer.capacity() - 1)
            m_end = 0;
        else
            ++m_end;
    }

    template<typename T, size_t inlineCapacity, typename Allocator, bool noDestructor> template<typename U>
    inline void Deque<T, inlineCapacity, Allocator, noDestructor>::prepend(const U& value)
    {
        expandCapacityIfNeeded();
        if (!m_start)
            m_start = m_buffer.capacity() - 1;
        else
            --m_start;
        new (NotNull, &m_buffer.buffer()[m_start]) T(value);
    }

    template<typename T, size_t inlineCapacity, typename Allocator, bool noDestructor>
    inline void Deque<T, inlineCapacity, Allocator, noDestructor>::removeFirst()
    {
        ASSERT(!isEmpty());
        TypeOperations::destruct(&m_buffer.buffer()[m_start], &m_buffer.buffer()[m_start + 1]);
        if (m_start == m_buffer.capacity() - 1)
            m_start = 0;
        else
            ++m_start;
    }

    template<typename T, size_t inlineCapacity, typename Allocator, bool noDestructor>
    inline void Deque<T, inlineCapacity, Allocator, noDestructor>::removeLast()
    {
        ASSERT(!isEmpty());
        if (!m_end)
            m_end = m_buffer.capacity() - 1;
        else
            --m_end;
        TypeOperations::destruct(&m_buffer.buffer()[m_end], &m_buffer.buffer()[m_end + 1]);
    }

    template<typename T, size_t inlineCapacity, typename Allocator, bool noDestructor>
    inline void Deque<T, inlineCapacity, Allocator, noDestructor>::remove(iterator& it)
    {
        remove(it.m_index);
    }

    template<typename T, size_t inlineCapacity, typename Allocator, bool noDestructor>
    inline void Deque<T, inlineCapacity, Allocator, noDestructor>::remove(const_iterator& it)
    {
        remove(it.m_index);
    }

    template<typename T, size_t inlineCapacity, typename Allocator, bool noDestructor>
    inline void Deque<T, inlineCapacity, Allocator, noDestructor>::remove(size_t position)
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

    template<typename T, size_t inlineCapacity, typename Allocator, bool noDestructor>
    inline DequeIteratorBase<T, inlineCapacity, Allocator, noDestructor>::DequeIteratorBase()
        : m_deque(0)
    {
    }

    template<typename T, size_t inlineCapacity, typename Allocator, bool noDestructor>
    inline DequeIteratorBase<T, inlineCapacity, Allocator, noDestructor>::DequeIteratorBase(const Deque<T, inlineCapacity, Allocator, noDestructor>* deque, size_t index)
        : m_deque(const_cast<Deque<T, inlineCapacity, Allocator, noDestructor>*>(deque))
        , m_index(index)
    {
    }

    template<typename T, size_t inlineCapacity, typename Allocator, bool noDestructor>
    inline DequeIteratorBase<T, inlineCapacity, Allocator, noDestructor>::DequeIteratorBase(const DequeIteratorBase& other)
        : m_deque(other.m_deque)
        , m_index(other.m_index)
    {
    }

    template<typename T, size_t inlineCapacity, typename Allocator, bool noDestructor>
    inline DequeIteratorBase<T, 0, Allocator, noDestructor>& DequeIteratorBase<T, inlineCapacity, Allocator, noDestructor>::operator=(const DequeIteratorBase<T, 0, Allocator, noDestructor>& other)
    {
        m_deque = other.m_deque;
        m_index = other.m_index;
        return *this;
    }

    template<typename T, size_t inlineCapacity, typename Allocator, bool noDestructor>
    inline DequeIteratorBase<T, inlineCapacity, Allocator, noDestructor>::~DequeIteratorBase()
    {
    }

    template<typename T, size_t inlineCapacity, typename Allocator, bool noDestructor>
    inline bool DequeIteratorBase<T, inlineCapacity, Allocator, noDestructor>::isEqual(const DequeIteratorBase& other) const
    {
        return m_index == other.m_index;
    }

    template<typename T, size_t inlineCapacity, typename Allocator, bool noDestructor>
    inline void DequeIteratorBase<T, inlineCapacity, Allocator, noDestructor>::increment()
    {
        ASSERT(m_index != m_deque->m_end);
        ASSERT(m_deque->m_buffer.capacity());
        if (m_index == m_deque->m_buffer.capacity() - 1)
            m_index = 0;
        else
            ++m_index;
    }

    template<typename T, size_t inlineCapacity, typename Allocator, bool noDestructor>
    inline void DequeIteratorBase<T, inlineCapacity, Allocator, noDestructor>::decrement()
    {
        ASSERT(m_index != m_deque->m_start);
        ASSERT(m_deque->m_buffer.capacity());
        if (!m_index)
            m_index = m_deque->m_buffer.capacity() - 1;
        else
            --m_index;
    }

    template<typename T, size_t inlineCapacity, typename Allocator, bool noDestructor>
    inline T* DequeIteratorBase<T, inlineCapacity, Allocator, noDestructor>::after() const
    {
        ASSERT(m_index != m_deque->m_end);
        return &m_deque->m_buffer.buffer()[m_index];
    }

    template<typename T, size_t inlineCapacity, typename Allocator, bool noDestructor>
    inline T* DequeIteratorBase<T, inlineCapacity, Allocator, noDestructor>::before() const
    {
        ASSERT(m_index != m_deque->m_start);
        if (!m_index)
            return &m_deque->m_buffer.buffer()[m_deque->m_buffer.capacity() - 1];
        return &m_deque->m_buffer.buffer()[m_index - 1];
    }

    // This is only called if the allocator is a HeapAllocator. It is used when
    // visiting during a tracing GC.
    template<typename T, size_t inlineCapacity, typename Allocator, bool noDestructor>
    template<typename VisitorDispatcher>
    void Deque<T, inlineCapacity, Allocator, noDestructor>::trace(VisitorDispatcher visitor)
    {
        ASSERT(Allocator::isGarbageCollected); // Garbage collector must be enabled.
        const T* bufferBegin = m_buffer.buffer();
        const T* end = bufferBegin + m_end;
        if (ShouldBeTraced<VectorTraits<T>>::value) {
            if (m_start <= m_end) {
                for (const T* bufferEntry = bufferBegin + m_start; bufferEntry != end; bufferEntry++)
                    Allocator::template trace<VisitorDispatcher, T, VectorTraits<T>>(visitor, *const_cast<T*>(bufferEntry));
            } else {
                for (const T* bufferEntry = bufferBegin; bufferEntry != end; bufferEntry++)
                    Allocator::template trace<VisitorDispatcher, T, VectorTraits<T>>(visitor, *const_cast<T*>(bufferEntry));
                const T* bufferEnd = m_buffer.buffer() + m_buffer.capacity();
                for (const T* bufferEntry = bufferBegin + m_start; bufferEntry != bufferEnd; bufferEntry++)
                    Allocator::template trace<VisitorDispatcher, T, VectorTraits<T>>(visitor, *const_cast<T*>(bufferEntry));
            }
        }
        if (m_buffer.hasOutOfLineBuffer())
            Allocator::markNoTracing(visitor, m_buffer.buffer());
    }

    template<typename T, size_t inlineCapacity, typename Allocator, bool noDestructor>
    inline void swap(Deque<T, inlineCapacity, Allocator, noDestructor>& a, Deque<T, inlineCapacity, Allocator, noDestructor>& b)
    {
        a.swap(b);
    }

#if !ENABLE(OILPAN)
    template<typename T, size_t N>
    struct NeedsTracing<Deque<T, N>> {
        static const bool value = false;
    };
#endif

} // namespace WTF

using WTF::Deque;

#endif // WTF_Deque_h
