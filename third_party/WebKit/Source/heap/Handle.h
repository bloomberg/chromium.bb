/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#ifndef Handle_h
#define Handle_h

#include "heap/Heap.h"
#include "heap/ThreadState.h"
#include "heap/Visitor.h"

#include "wtf/RawPtr.h"
#include "wtf/RefCounted.h"

namespace WebCore {

template<typename T> class Member;

class PersistentNode {
public:
    explicit PersistentNode(TraceCallback trace) : m_trace(trace) { }

    virtual ~PersistentNode() { }

    // Ideally the trace method should be virtual and automatically dispatch
    // to the most specific implementation. However having a virtual method
    // on PersistentNode leads to too eager template instantiation with MSVC
    // which leads to include cycles.
    // Instead we call the constructor with a TraceCallback which knows the
    // type of the most specific child and calls trace directly. See
    // TraceMethodDelegate in Visitor.h for how this is done.
    void trace(Visitor* visitor)
    {
        m_trace(visitor, this);
    }

protected:
    TraceCallback m_trace;

private:
    PersistentNode* m_next;
    PersistentNode* m_prev;

    template<ThreadAffinity affinity, typename Owner> friend class PersistentBase;
    friend class PersistentAnchor;
    friend class ThreadState;
};

template<ThreadAffinity Affinity, typename Owner>
class PersistentBase : public PersistentNode {
public:
    ~PersistentBase()
    {
#ifndef NDEBUG
        m_threadState->checkThread();
#endif
        m_next->m_prev = m_prev;
        m_prev->m_next = m_next;
    }

protected:
    inline PersistentBase()
        : PersistentNode(TraceMethodDelegate<Owner, &Owner::trace>::trampoline)
#ifndef NDEBUG
        , m_threadState(state())
#endif
    {
#ifndef NDEBUG
        m_threadState->checkThread();
#endif
        ThreadState* threadState = state();
        m_prev = threadState->roots();
        m_next = threadState->roots()->m_next;
        threadState->roots()->m_next = this;
        m_next->m_prev = this;
    }

    inline explicit PersistentBase(const PersistentBase& otherref)
        : PersistentNode(otherref.m_trace)
#ifndef NDEBUG
        , m_threadState(state())
#endif
    {
#ifndef NDEBUG
        m_threadState->checkThread();
#endif
        ASSERT(otherref.m_threadState == m_threadState);
        PersistentBase* other = const_cast<PersistentBase*>(&otherref);
        m_prev = other;
        m_next = other->m_next;
        other->m_next = this;
        m_next->m_prev = this;
    }

    inline PersistentBase& operator=(const PersistentBase& otherref)
    {
        return *this;
    }

    static ThreadState* state() { return ThreadStateFor<Affinity>::state(); }

#ifndef NDEBUG
private:
    ThreadState* m_threadState;
#endif
};

// A dummy Persistent handle that ensures the list of persistents is never null.
// This removes a test from a hot path.
class PersistentAnchor : public PersistentNode {
public:
    void trace(Visitor*) { }

private:
    virtual ~PersistentAnchor() { }
    PersistentAnchor() : PersistentNode(TraceMethodDelegate<PersistentAnchor, &PersistentAnchor::trace>::trampoline)
    {
        m_next = this;
        m_prev = this;
    }

    friend class ThreadState;
};

// Persistent handles are used to store pointers into the
// managed heap. As long as the Persistent handle is alive
// the GC will keep the object pointed to alive. Persistent
// handles can be stored in objects and they are not scoped.
// Persistent handles must not be used to contain pointers
// between objects that are in the managed heap. They are only
// meant to point to managed heap objects from variables/members
// outside the managed heap.
//
// A Persistent is always a GC root from the point of view of
// the garbage collector.
template<typename T>
class Persistent : public PersistentBase<ThreadingTrait<T>::Affinity, Persistent<T> > {
public:
    Persistent() : m_raw(0) { }

    Persistent(T* raw) : m_raw(raw) { }

    Persistent(std::nullptr_t) : m_raw(0) { }

    Persistent(const Persistent& other) : m_raw(other) { }

    template<typename U>
    Persistent(const Persistent<U>& other) : m_raw(other) { }

    template<typename U>
    Persistent(const Member<U>& other) : m_raw(other) { }

    template<typename U>
    Persistent(const RawPtr<U>& other) : m_raw(other.get()) { }

    template<typename U>
    Persistent& operator=(U* other)
    {
        m_raw = other;
        return *this;
    }

    virtual ~Persistent()
    {
        m_raw = 0;
    }

    template<typename U>
    U* as() const
    {
        return static_cast<U*>(m_raw);
    }

    void trace(Visitor* visitor) { visitor->mark(m_raw); }

    T* release()
    {
        T* result = m_raw;
        m_raw = 0;
        return result;
    }

    T& operator*() const { return *m_raw; }

    bool operator!() const { return !m_raw; }

    operator T*() const { return m_raw; }
    operator RawPtr<T>() const { return m_raw; }

    T* operator->() const { return *this; }

    Persistent& operator=(std::nullptr_t)
    {
        m_raw = 0;
        return *this;
    }

    Persistent& operator=(const Persistent& other)
    {
        m_raw = other;
        return *this;
    }

    template<typename U>
    Persistent& operator=(const Persistent<U>& other)
    {
        m_raw = other;
        return *this;
    }

    template<typename U>
    Persistent& operator=(const Member<U>& other)
    {
        m_raw = other;
        return *this;
    }

    T* get() const { return m_raw; }

private:
    T* m_raw;
};

// Members are used in classes to contain strong pointers to other oilpan heap
// allocated objects.
// All Member fields of a class must be traced in the class' trace method.
// During the mark phase of the GC all live objects are marked as live and
// all Member fields of a live object will be traced marked as live as well.
template<typename T>
class Member {
public:
    Member() : m_raw(0) { }

    Member(T* raw) : m_raw(raw) { }

    Member(std::nullptr_t) : m_raw(0) { }

    Member(WTF::HashTableDeletedValueType) : m_raw(reinterpret_cast<T*>(-1)) { }

    bool isHashTableDeletedValue() const { return m_raw == reinterpret_cast<T*>(-1); }

    template<typename U>
    Member(const Persistent<U>& other) : m_raw(other) { }

    Member(const Member& other) : m_raw(other) { }

    template<typename U>
    Member(const Member<U>& other) : m_raw(other) { }

    T* release()
    {
        T* result = m_raw;
        m_raw = 0;
        return result;
    }

    template<typename U>
    U* as() const
    {
        return static_cast<U*>(m_raw);
    }

    bool operator!() const { return !m_raw; }

    operator T*() const { return m_raw; }

    T* operator->() const { return m_raw; }
    T& operator*() const { return *m_raw; }

    Member& operator=(std::nullptr_t)
    {
        m_raw = 0;
        return *this;
    }

    template<typename U>
    Member& operator=(const Persistent<U>& other)
    {
        m_raw = other;
        return *this;
    }

    Member& operator=(const Member& other)
    {
        m_raw = other;
        return *this;
    }

    template<typename U>
    Member& operator=(const Member<U>& other)
    {
        m_raw = other;
        return *this;
    }

    template<typename U>
    Member& operator=(U* other)
    {
        m_raw = other;
        return *this;
    }

    void swap(Member<T>& other) { std::swap(m_raw, other.m_raw); }

    T* get() const { return m_raw; }

protected:
    T* m_raw;

    template<bool x, bool y, bool z, typename U, typename V> friend struct CollectionBackingTraceTrait;
};

template<typename T>
class TraceTrait<Member<T> > {
public:
    static void trace(Visitor* visitor, void* self)
    {
        TraceTrait<T>::mark(visitor, *static_cast<Member<T>*>(self));
    }
};

// WeakMember is similar to Member in that it is used to point to other oilpan
// heap allocated objects.
// However instead of creating a strong pointer to the object, the WeakMember creates
// a weak pointer, which does not keep the pointee alive. Hence if all pointers to
// to a heap allocated object are weak the object will be garbage collected. At the
// time of GC the weak pointers will automatically be set to null.
template<typename T>
class WeakMember : public Member<T> {
public:
    WeakMember() : Member<T>() { }

    WeakMember(T* raw) : Member<T>(raw) { }

    WeakMember(std::nullptr_t) : Member<T>(nullptr) { }

    WeakMember(WTF::HashTableDeletedValueType x) : Member<T>(x) { }

    template<typename U>
    WeakMember(const Persistent<U>& other) : Member<T>(other) { }

    template<typename U>
    WeakMember(const Member<U>& other) : Member<T>(other) { }

    WeakMember& operator=(std::nullptr_t)
    {
        this->m_raw = 0;
        return *this;
    }

    template<typename U>
    WeakMember& operator=(const Persistent<U>& other)
    {
        this->m_raw = other;
        return *this;
    }

    template<typename U>
    WeakMember& operator=(const Member<U>& other)
    {
        this->m_raw = other;
        return *this;
    }

    template<typename U>
    WeakMember& operator=(U* other)
    {
        this->m_raw = other;
        return *this;
    }

private:
    T** cell() const { return const_cast<T**>(&this->m_raw); }

    friend class Visitor;
};

// Comparison operators between (Weak)Members and Persistents
template<typename T, typename U> inline bool operator==(const Member<T>& a, const Member<U>& b) { return a.get() == b.get(); }
template<typename T, typename U> inline bool operator!=(const Member<T>& a, const Member<U>& b) { return a.get() != b.get(); }
template<typename T, typename U> inline bool operator==(const Member<T>& a, const Persistent<U>& b) { return a.get() == b.get(); }
template<typename T, typename U> inline bool operator!=(const Member<T>& a, const Persistent<U>& b) { return a.get() != b.get(); }
template<typename T, typename U> inline bool operator==(const Persistent<T>& a, const Member<U>& b) { return a.get() == b.get(); }
template<typename T, typename U> inline bool operator!=(const Persistent<T>& a, const Member<U>& b) { return a.get() != b.get(); }
template<typename T, typename U> inline bool operator==(const Persistent<T>& a, const Persistent<U>& b) { return a.get() == b.get(); }
template<typename T, typename U> inline bool operator!=(const Persistent<T>& a, const Persistent<U>& b) { return a.get() != b.get(); }

// Template aliases for the transition period where we want to support
// both reference counting and garbage collection based on a
// compile-time flag.
//
// With clang we can use c++11 template aliases which is really what
// we want. For GCC and MSVC we simulate the template aliases with
// stylized macros until we can use template aliases.
#if ENABLE(OILPAN)

#if COMPILER(CLANG)
template<typename T> using PassRefPtrWillBeRawPtr = RawPtr<T>;
template<typename T> using RefCountedWillBeGarbageCollected = GarbageCollected<T>;
template<typename T> using RefCountedWillBeGarbageCollectedFinalized = GarbageCollectedFinalized<T>;
template<typename T> using RefCountedWillBeRefCountedGarbageCollected = RefCountedGarbageCollected<T>;
template<typename T> using RefPtrWillBePersistent = Persistent<T>;
template<typename T> using RefPtrWillBeRawPtr = RawPtr<T>;
template<typename T> using RefPtrWillBeMember = Member<T>;
template<typename T> using RawPtrWillBeMember = Member<T>;
template<typename T> using RawPtrWillBeWeakMember = WeakMember<T>;
template<typename T> using OwnPtrWillBeMember = Member<T>;
template<typename T> using PassOwnPtrWillBeRawPtr = RawPtr<T>;
template<typename T> using NoBaseWillBeGarbageCollected = GarbageCollected<T>;
template<typename T> using NoBaseWillBeGarbageCollectedFinalized = GarbageCollectedFinalized<T>;
#else // !COMPILER(CLANG)
#define PassRefPtrWillBeRawPtr RawPtr
#define RefCountedWillBeGarbageCollected GarbageCollected
#define RefCountedWillBeGarbageCollectedFinalized GarbageCollectedFinalized
#define RefCountedWillBeRefCountedGarbageCollected RefCountedGarbageCollected
#define RefPtrWillBePersistent Persistent
#define RefPtrWillBeRawPtr RawPtr
#define RefPtrWillBeMember Member
#define RawPtrWillBeMember Member
#define RawPtrWillBeWeakMember WeakMember
#define OwnPtrWillBeMember Member
#define PassOwnPtrWillBeRawPtr RawPtr
#define NoBaseWillBeGarbageCollected GarbageCollected
#define NoBaseWillBeGarbageCollectedFinalized GarbageCollectedFinalized
#endif // COMPILER(CLANG)

template<typename T> PassRefPtrWillBeRawPtr<T> adoptRefWillBeNoop(T* ptr) { return PassRefPtrWillBeRawPtr<T>(ptr); }
template<typename T> PassOwnPtrWillBeRawPtr<T> adoptPtrWillBeNoop(T* ptr) { return PassOwnPtrWillBeRawPtr<T>(ptr); }

#else // !ENABLE(OILPAN)

template<typename T>
class DummyBase {
public:
    DummyBase() { }
    ~DummyBase() { }
};

#if COMPILER(CLANG)
template<typename T> using PassRefPtrWillBeRawPtr = PassRefPtr<T>;
template<typename T> using RefCountedWillBeGarbageCollected = RefCounted<T>;
template<typename T> using RefCountedWillBeGarbageCollectedFinalized = RefCounted<T>;
template<typename T> using RefCountedWillBeRefCountedGarbageCollected = RefCounted<T>;
template<typename T> using RefPtrWillBePersistent = RefPtr<T>;
template<typename T> using RefPtrWillBeRawPtr = RefPtr<T>;
template<typename T> using RefPtrWillBeMember = RefPtr<T>;
template<typename T> using RawPtrWillBeMember = RawPtr<T>;
template<typename T> using RawPtrWillBeWeakMember = RawPtr<T>;
template<typename T> using OwnPtrWillBeMember = OwnPtr<T>;
template<typename T> using PassOwnPtrWillBeRawPtr = PassOwnPtr<T>;
template<typename T> using NoBaseWillBeGarbageCollected = DummyBase<T>;
template<typename T> using NoBaseWillBeGarbageCollectedFinalized = DummyBase<T>;
#else // !COMPILER(CLANG)
#define PassRefPtrWillBeRawPtr PassRefPtr
#define RefCountedWillBeGarbageCollected RefCounted
#define RefCountedWillBeGarbageCollectedFinalized RefCounted
#define RefCountedWillBeRefCountedGarbageCollected RefCounted
#define RefPtrWillBePersistent RefPtr
#define RefPtrWillBeRawPtr RefPtr
#define RefPtrWillBeMember RefPtr
#define RawPtrWillBeMember RawPtr
#define RawPtrWillBeWeakMember RawPtr
#define OwnPtrWillBeMember OwnPtr
#define PassOwnPtrWillBeRawPtr PassOwnPtr
#define NoBaseWillBeGarbageCollected DummyBase
#define NoBaseWillBeGarbageCollectedFinalized DummyBase
#endif // COMPILER(CLANG)

template<typename T> PassRefPtrWillBeRawPtr<T> adoptRefWillBeNoop(T* ptr) { return adoptRef(ptr); }
template<typename T> PassOwnPtrWillBeRawPtr<T> adoptPtrWillBeNoop(T* ptr) { return adoptPtr(ptr); }

#endif // ENABLE(OILPAN)

} // namespace WebCore

namespace WTF {

template <typename T> struct VectorTraits<WebCore::Member<T> > : VectorTraitsBase<false, WebCore::Member<T> > {
    static const bool needsDestruction = false;
    static const bool canInitializeWithMemset = true;
    static const bool canMoveWithMemcpy = true;
};

template <typename T> struct VectorTraits<WebCore::WeakMember<T> > : VectorTraitsBase<false, WebCore::WeakMember<T> > {
    static const bool needsDestruction = false;
    static const bool canInitializeWithMemset = true;
    static const bool canMoveWithMemcpy = true;
};

template<typename T> struct HashTraits<WebCore::Member<T> > : SimpleClassHashTraits<WebCore::Member<T> > {
    static const bool needsDestruction = false;
    // FIXME: The distinction between PeekInType and PassInType is there for
    // the sake of the reference counting handles. When they are gone the two
    // types can be merged into PassInType.
    // FIXME: Implement proper const'ness for iterator types. Requires support
    // in the marking Visitor.
    typedef T* PeekInType;
    typedef T* PassInType;
    typedef T* IteratorGetType;
    typedef T* IteratorConstGetType;
    typedef T* IteratorReferenceType;
    typedef T* IteratorConstReferenceType;
    static IteratorConstGetType getToConstGetConversion(const WebCore::Member<T>* x) { return x->get(); }
    static IteratorReferenceType getToReferenceConversion(IteratorGetType x) { return x; }
    static IteratorConstReferenceType getToReferenceConstConversion(IteratorConstGetType x) { return x; }
    // FIXME: Similarly, there is no need for a distinction between PeekOutType
    // and PassOutType without reference counting.
    typedef T* PeekOutType;
    typedef T* PassOutType;

    template<typename U>
    static void store(const U& value, WebCore::Member<T>& storage) { storage = value; }

    static PeekOutType peek(const WebCore::Member<T>& value) { return value; }
    static PassOutType passOut(const WebCore::Member<T>& value) { return value; }
};

template<typename T> struct HashTraits<WebCore::WeakMember<T> > : SimpleClassHashTraits<WebCore::WeakMember<T> > {
    static const bool needsDestruction = false;
    // FIXME: The distinction between PeekInType and PassInType is there for
    // the sake of the reference counting handles. When they are gone the two
    // types can be merged into PassInType.
    // FIXME: Implement proper const'ness for iterator types. Requires support
    // in the marking Visitor.
    typedef T* PeekInType;
    typedef T* PassInType;
    typedef T* IteratorGetType;
    typedef T* IteratorConstGetType;
    typedef T* IteratorReferenceType;
    typedef T* IteratorConstReferenceType;
    static IteratorConstGetType getToConstGetConversion(const WebCore::WeakMember<T>* x) { return x->get(); }
    static IteratorReferenceType getToReferenceConversion(IteratorGetType x) { return x; }
    static IteratorConstReferenceType getToReferenceConstConversion(IteratorConstGetType x) { return x; }
    // FIXME: Similarly, there is no need for a distinction between PeekOutType
    // and PassOutType without reference counting.
    typedef T* PeekOutType;
    typedef T* PassOutType;

    template<typename U>
    static void store(const U& value, WebCore::WeakMember<T>& storage) { storage = value; }

    static PeekOutType peek(const WebCore::WeakMember<T>& value) { return value; }
    static PassOutType passOut(const WebCore::WeakMember<T>& value) { return value; }
};

template<typename T> struct PtrHash<WebCore::Member<T> > : PtrHash<T*> {
    template<typename U>
    static unsigned hash(const U& key) { return PtrHash<T*>::hash(key); }
    static bool equal(T* a, const WebCore::Member<T>& b) { return a == b; }
    static bool equal(const WebCore::Member<T>& a, T* b) { return a == b; }
    template<typename U, typename V>
    static bool equal(const U& a, const V& b) { return a == b; }
};

template<typename T> struct PtrHash<WebCore::WeakMember<T> > : PtrHash<WebCore::Member<T> > {
};

// PtrHash is the default hash for hash tables with members.
template<typename T> struct DefaultHash<WebCore::Member<T> > {
    typedef PtrHash<WebCore::Member<T> > Hash;
};

template<typename T> struct DefaultHash<WebCore::WeakMember<T> > {
    typedef PtrHash<WebCore::WeakMember<T> > Hash;
};

template<typename T>
struct NeedsTracing<WebCore::Member<T> > {
    static const bool value = true;
};

template<typename T>
struct IsWeak<WebCore::WeakMember<T> > {
    static const bool value = true;
};

template<typename Key, typename Value, typename Extractor, typename Traits, typename KeyTraits>
struct IsWeak<WebCore::HeapHashTableBacking<Key, Value, Extractor, Traits, KeyTraits> > {
    static const bool value = Traits::isWeak;
};

} // namespace WTF

#endif
