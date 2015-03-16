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

#ifndef Visitor_h
#define Visitor_h

#include "platform/PlatformExport.h"
#include "platform/heap/StackFrameDepth.h"
#include "platform/heap/ThreadState.h"
#include "wtf/Assertions.h"
#include "wtf/Atomics.h"
#include "wtf/Deque.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/HashTraits.h"
#include "wtf/TypeTraits.h"
#if ENABLE(GC_PROFILING)
#include "wtf/InstanceCounter.h"
#include "wtf/text/WTFString.h"
#endif

namespace blink {

template<typename T> class GarbageCollected;
template<typename T> class GarbageCollectedFinalized;
class GarbageCollectedMixin;
class HeapObjectHeader;
class InlinedGlobalMarkingVisitor;
template<typename T> class Member;
template<typename T> class WeakMember;
class Visitor;

template <typename T> struct IsGarbageCollectedType;
#define STATIC_ASSERT_IS_GARBAGE_COLLECTED(T, ErrorMessage) \
    static_assert(IsGarbageCollectedType<T>::value, ErrorMessage)
#define STATIC_ASSERT_IS_NOT_GARBAGE_COLLECTED(T, ErrorMessage) \
    static_assert(!IsGarbageCollectedType<T>::value, ErrorMessage)

template<bool needsTracing, WTF::WeakHandlingFlag weakHandlingFlag, WTF::ShouldWeakPointersBeMarkedStrongly strongify, typename T, typename Traits> struct CollectionBackingTraceTrait;

// The TraceMethodDelegate is used to convert a trace method for type T to a TraceCallback.
// This allows us to pass a type's trace method as a parameter to the PersistentNode
// constructor. The PersistentNode constructor needs the specific trace method due an issue
// with the Windows compiler which instantiates even unused variables. This causes problems
// in header files where we have only forward declarations of classes.
template<typename T, void (T::*method)(Visitor*)>
struct TraceMethodDelegate {
    static void trampoline(Visitor* visitor, void* self) { (reinterpret_cast<T*>(self)->*method)(visitor); }
};

// GCInfo contains meta-data associated with objects allocated in the
// Blink heap. This meta-data consists of a function pointer used to
// trace the pointers in the object during garbage collection, an
// indication of whether or not the object needs a finalization
// callback, and a function pointer used to finalize the object when
// the garbage collector determines that the object is no longer
// reachable. There is a GCInfo struct for each class that directly
// inherits from GarbageCollected or GarbageCollectedFinalized.
struct GCInfo {
    bool hasFinalizer() const { return m_nonTrivialFinalizer; }
    bool hasVTable() const { return m_hasVTable; }
    TraceCallback m_trace;
    FinalizationCallback m_finalize;
    bool m_nonTrivialFinalizer;
    bool m_hasVTable;
#if ENABLE(GC_PROFILING)
    // |m_className| is held as a reference to prevent dtor being called at exit.
    const String& m_className;
#endif
};

#if ENABLE(ASSERT)
PLATFORM_EXPORT void assertObjectHasGCInfo(const void*, size_t gcInfoIndex);
#endif

// The FinalizerTraitImpl specifies how to finalize objects. Object
// that inherit from GarbageCollectedFinalized are finalized by
// calling their 'finalize' method which by default will call the
// destructor on the object.
template<typename T, bool isGarbageCollectedFinalized>
struct FinalizerTraitImpl;

template<typename T>
struct FinalizerTraitImpl<T, true> {
    static void finalize(void* obj) { static_cast<T*>(obj)->finalizeGarbageCollectedObject(); };
};

template<typename T>
struct FinalizerTraitImpl<T, false> {
    static void finalize(void* obj) { };
};

// The FinalizerTrait is used to determine if a type requires
// finalization and what finalization means.
//
// By default classes that inherit from GarbageCollectedFinalized need
// finalization and finalization means calling the 'finalize' method
// of the object. The FinalizerTrait can be specialized if the default
// behavior is not desired.
template<typename T>
struct FinalizerTrait {
    static const bool nonTrivialFinalizer = WTF::IsSubclassOfTemplate<typename WTF::RemoveConst<T>::Type, GarbageCollectedFinalized>::value;
    static void finalize(void* obj) { FinalizerTraitImpl<T, nonTrivialFinalizer>::finalize(obj); }
};

// Template to determine if a class is a GarbageCollectedMixin by checking if it
// has IsGarbageCollectedMixinMarker
template<typename T>
struct IsGarbageCollectedMixin {
private:
    typedef char YesType;
    struct NoType {
        char padding[8];
    };

    template <typename U> static YesType checkMarker(typename U::IsGarbageCollectedMixinMarker*);
    template <typename U> static NoType checkMarker(...);

public:
    static const bool value = sizeof(checkMarker<T>(nullptr)) == sizeof(YesType);
};

// Trait to get the GCInfo structure for types that have their
// instances allocated in the Blink garbage-collected heap.
template<typename T> struct GCInfoTrait;

template<typename T, bool = WTF::IsSubclassOfTemplate<typename WTF::RemoveConst<T>::Type, GarbageCollected>::value> class NeedsAdjustAndMark;

template<typename T>
class NeedsAdjustAndMark<T, true> {
public:
    static const bool value = false;
};

template <typename T> const bool NeedsAdjustAndMark<T, true>::value;

template<typename T>
class NeedsAdjustAndMark<T, false> {
public:
    static const bool value = IsGarbageCollectedMixin<typename WTF::RemoveConst<T>::Type>::value;
};

template <typename T> const bool NeedsAdjustAndMark<T, false>::value;

template<typename T, bool = NeedsAdjustAndMark<T>::value> class DefaultTraceTrait;

// HasInlinedTraceMethod<T>::value is true for T supporting
// T::trace(InlinedGlobalMarkingVisitor).
// The template works by checking if T::HasInlinedTraceMethodMarker type is
// available using SFINAE. The HasInlinedTraceMethodMarker type is defined
// by DECLARE_TRACE and DEFINE_INLINE_TRACE helper macros, which are used to
// define trace methods supporting both inlined/uninlined tracing.
template <typename T>
struct HasInlinedTraceMethod {
#if ENABLE(INLINED_TRACE)
private:
    typedef char YesType;
    struct NoType {
        char padding[8];
    };

    template <typename U> static YesType checkMarker(typename U::HasInlinedTraceMethodMarker*);
    template <typename U> static NoType checkMarker(...);
public:
    static const bool value = sizeof(checkMarker<T>(nullptr)) == sizeof(YesType);
#else
    static const bool value = false;
#endif
};

template <typename T, bool = HasInlinedTraceMethod<T>::value>
struct TraceCompatibilityAdaptor;

// The TraceTrait is used to specify how to mark an object pointer and
// how to trace all of the pointers in the object.
//
// By default, the 'trace' method implemented on an object itself is
// used to trace the pointers to other heap objects inside the object.
//
// However, the TraceTrait can be specialized to use a different
// implementation. A common case where a TraceTrait specialization is
// needed is when multiple inheritance leads to pointers that are not
// to the start of the object in the Blink garbage-collected heap. In
// that case the pointer has to be adjusted before marking.
template<typename T>
class TraceTrait {
public:
    // Default implementation of TraceTrait<T>::trace just statically
    // dispatches to the trace method of the class T.
    template<typename VisitorDispatcher>
    static void trace(VisitorDispatcher visitor, void* self)
    {
        TraceCompatibilityAdaptor<T>::trace(visitor, static_cast<T*>(self));
    }

    template<typename VisitorDispatcher>
    static void mark(VisitorDispatcher visitor, const T* t)
    {
        DefaultTraceTrait<T>::mark(visitor, t);
    }

#if ENABLE(ASSERT)
    static void checkGCInfo(const T* t)
    {
        DefaultTraceTrait<T>::checkGCInfo(t);
    }
#endif
};

template<typename T> class TraceTrait<const T> : public TraceTrait<T> { };

#if ENABLE(INLINED_TRACE)

#define DECLARE_TRACE_IMPL(maybevirtual)                                     \
public:                                                                      \
    typedef int HasInlinedTraceMethodMarker;                                 \
    maybevirtual void trace(Visitor*);                                       \
    maybevirtual void trace(InlinedGlobalMarkingVisitor);                    \
                                                                             \
private:                                                                     \
    template <typename VisitorDispatcher> void traceImpl(VisitorDispatcher); \
                                                                             \
public:
#define DEFINE_TRACE(T)                                                        \
    void T::trace(Visitor* visitor) { traceImpl(visitor); }                    \
    void T::trace(InlinedGlobalMarkingVisitor visitor) { traceImpl(visitor); } \
    template <typename VisitorDispatcher>                                      \
    ALWAYS_INLINE void T::traceImpl(VisitorDispatcher visitor)

#define DEFINE_INLINE_TRACE_IMPL(maybevirtual)                                           \
    typedef int HasInlinedTraceMethodMarker;                                             \
    maybevirtual void trace(Visitor* visitor) { traceImpl(visitor); }                    \
    maybevirtual void trace(InlinedGlobalMarkingVisitor visitor) { traceImpl(visitor); } \
    template <typename VisitorDispatcher>                                                \
    inline void traceImpl(VisitorDispatcher visitor)

#define DECLARE_TRACE_AFTER_DISPATCH()                                                    \
public:                                                                                   \
    typedef int HasInlinedTraceAfterDispatchMethodMarker;                                 \
    void traceAfterDispatch(Visitor*);                                                    \
    void traceAfterDispatch(InlinedGlobalMarkingVisitor);                                 \
private:                                                                                  \
    template <typename VisitorDispatcher> void traceAfterDispatchImpl(VisitorDispatcher); \
public:

#define DEFINE_TRACE_AFTER_DISPATCH(T)                                                                   \
    void T::traceAfterDispatch(Visitor* visitor) { traceAfterDispatchImpl(visitor); }                    \
    void T::traceAfterDispatch(InlinedGlobalMarkingVisitor visitor) { traceAfterDispatchImpl(visitor); } \
    template <typename VisitorDispatcher>                                                                \
    ALWAYS_INLINE void T::traceAfterDispatchImpl(VisitorDispatcher visitor)

#define DEFINE_INLINE_TRACE_AFTER_DISPATCH()                                                          \
    typedef int HasInlinedTraceAfterDispatchMethodMarker;                                             \
    void traceAfterDispatch(Visitor* visitor) { traceAfterDispatchImpl(visitor); }                    \
    void traceAfterDispatch(InlinedGlobalMarkingVisitor visitor) { traceAfterDispatchImpl(visitor); } \
    template <typename VisitorDispatcher>                                                             \
    inline void traceAfterDispatchImpl(VisitorDispatcher visitor)

#else // !ENABLE(INLINED_TRACE)

#define DECLARE_TRACE_IMPL(maybevirtual) \
public:                                  \
    maybevirtual void trace(Visitor*);

#define DEFINE_TRACE(T) void T::trace(Visitor* visitor)

#define DEFINE_INLINE_TRACE_IMPL(maybevirtual) \
    maybevirtual void trace(Visitor* visitor)

#define DECLARE_TRACE_AFTER_DISPATCH() void traceAfterDispatch(Visitor*);

#define DEFINE_TRACE_AFTER_DISPATCH(T) \
    void T::traceAfterDispatch(Visitor* visitor)

#define DEFINE_INLINE_TRACE_AFTER_DISPATCH() \
    void traceAfterDispatch(Visitor* visitor)

#endif

#define EMPTY_MACRO_ARGUMENT

#define DECLARE_TRACE() DECLARE_TRACE_IMPL(EMPTY_MACRO_ARGUMENT)
#define DECLARE_VIRTUAL_TRACE() DECLARE_TRACE_IMPL(virtual)
#define DEFINE_INLINE_TRACE() DEFINE_INLINE_TRACE_IMPL(EMPTY_MACRO_ARGUMENT)
#define DEFINE_INLINE_VIRTUAL_TRACE() DEFINE_INLINE_TRACE_IMPL(virtual)

// If MARKER_EAGER_TRACING is set to 1, a marker thread is allowed
// to directly invoke the trace() method of not-as-yet marked objects upon
// marking. If it is set to 0, the |trace()| callback for an object will
// be pushed onto an explicit mark stack, which the marker proceeds to
// iteratively pop and invoke. The eager scheme enables inlining of a trace()
// method inside another, the latter keeps system call stack usage bounded
// and under explicit control.
//
// If eager tracing leads to excessively deep |trace()| call chains (and
// the system stack usage that this brings), the marker implementation will
// switch to using an explicit mark stack. Recursive and deep object graphs
// are uncommon for Blink objects.
//
// A class type can opt out of eager tracing by declaring a TraceEagerlyTrait<>
// specialization, mapping the trait's |value| to |false| (see the
// WILL_NOT_BE_EAGERLY_TRACED() macros below.) For Blink, this is done for
// the small set of GCed classes that are directly recursive.
#define MARKER_EAGER_TRACING 1

// The TraceEagerlyTrait<T> trait controls whether or not a class
// (and its subclasses) should be eagerly traced or not.
//
// If |TraceEagerlyTrait<T>::value| is |true|, then the marker thread
// should invoke |trace()| on not-yet-marked objects deriving from class T
// right away, and not queue their trace callbacks on its marker stack,
// which it will do if |value| is |false|.
//
// The trait can be declared to enable/disable eager tracing for a class T
// and any of its subclasses, or just to the class T, but none of its
// subclasses.
//
template<typename T, typename Enabled = void>
class TraceEagerlyTrait {
public:
    static const bool value = MARKER_EAGER_TRACING;
};

#define WILL_NOT_BE_EAGERLY_TRACED(TYPE)                                                    \
template<typename T>                                                                        \
class TraceEagerlyTrait<T, typename WTF::EnableIf<WTF::IsSubclass<T, TYPE>::value>::Type> { \
public:                                                                                     \
    static const bool value = false;                                                        \
}

// Disable eager tracing for TYPE, but not any of its subclasses.
#define WILL_NOT_BE_EAGERLY_TRACED_CLASS(TYPE)   \
template<>                                       \
class TraceEagerlyTrait<TYPE> {                  \
public:                                          \
    static const bool value = false;             \
}

template<typename Collection>
struct OffHeapCollectionTraceTrait;

template<typename T>
struct ObjectAliveTrait {
    template<typename VisitorDispatcher>
    static bool isHeapObjectAlive(VisitorDispatcher, T*);
};

// VisitorHelper contains common implementation of Visitor helper methods.
//
// VisitorHelper avoids virtual methods by using CRTP.
// c.f. http://en.wikipedia.org/wiki/Curiously_Recurring_Template_Pattern
template<typename Derived>
class VisitorHelper {
public:
    // One-argument templated mark method. This uses the static type of
    // the argument to get the TraceTrait. By default, the mark method
    // of the TraceTrait just calls the virtual two-argument mark method on this
    // visitor, where the second argument is the static trace method of the trait.
    template<typename T>
    void mark(T* t)
    {
        if (!t)
            return;
#if ENABLE(ASSERT)
        TraceTrait<T>::checkGCInfo(t);
        Derived::fromHelper(this)->checkMarkingAllowed();
#endif
        TraceTrait<T>::mark(Derived::fromHelper(this), t);

        STATIC_ASSERT_IS_GARBAGE_COLLECTED(T, "attempted to mark non garbage collected object");
    }

    // Member version of the one-argument templated trace method.
    template<typename T>
    void trace(const Member<T>& t)
    {
        Derived::fromHelper(this)->mark(t.get());
    }

    // Fallback method used only when we need to trace raw pointers of T.
    // This is the case when a member is a union where we do not support members.
    template<typename T>
    void trace(const T* t)
    {
        Derived::fromHelper(this)->mark(const_cast<T*>(t));
    }

    template<typename T>
    void trace(T* t)
    {
        Derived::fromHelper(this)->mark(t);
    }

    // WeakMember version of the templated trace method. It doesn't keep
    // the traced thing alive, but will write null to the WeakMember later
    // if the pointed-to object is dead. It's lying for this to be const,
    // but the overloading resolver prioritizes constness too high when
    // picking the correct overload, so all these trace methods have to have
    // the same constness on their argument to allow the type to decide.
    template<typename T>
    void trace(const WeakMember<T>& t)
    {
        // Check that we actually know the definition of T when tracing.
        static_assert(sizeof(T), "we need to know the definition of the type we are tracing");
        registerWeakCell(const_cast<WeakMember<T>&>(t).cell());
        STATIC_ASSERT_IS_GARBAGE_COLLECTED(T, "cannot weak trace non garbage collected object");
    }

    template<typename T>
    void traceInCollection(T& t, WTF::ShouldWeakPointersBeMarkedStrongly strongify)
    {
        HashTraits<T>::traceInCollection(Derived::fromHelper(this), t, strongify);
    }

    // Fallback trace method for part objects to allow individual trace methods
    // to trace through a part object with visitor->trace(m_partObject). This
    // takes a const argument, because otherwise it will match too eagerly: a
    // non-const argument would match a non-const Vector<T>& argument better
    // than the specialization that takes const Vector<T>&. For a similar reason,
    // the other specializations take a const argument even though they are
    // usually used with non-const arguments, otherwise this function would match
    // too well.
    template<typename T>
    void trace(const T& t)
    {
        if (WTF::IsPolymorphic<T>::value) {
            intptr_t vtable = *reinterpret_cast<const intptr_t*>(&t);
            if (!vtable)
                return;
        }
        TraceCompatibilityAdaptor<T>::trace(Derived::fromHelper(this), &const_cast<T&>(t));
    }

    // For simple cases where you just want to zero out a cell when the thing
    // it is pointing at is garbage, you can use this. This will register a
    // callback for each cell that needs to be zeroed, so if you have a lot of
    // weak cells in your object you should still consider using
    // registerWeakMembers above.
    //
    // In contrast to registerWeakMembers, the weak cell callbacks are
    // run on the thread performing garbage collection. Therefore, all
    // threads are stopped during weak cell callbacks.
    template<typename T>
    void registerWeakCell(T** cell)
    {
        Derived::fromHelper(this)->registerWeakCellWithCallback(reinterpret_cast<void**>(cell), &handleWeakCell<T>);
    }

    // The following trace methods are for off-heap collections.
    template<typename T, size_t inlineCapacity>
    void trace(const Vector<T, inlineCapacity>& vector)
    {
        OffHeapCollectionTraceTrait<Vector<T, inlineCapacity, WTF::DefaultAllocator>>::trace(Derived::fromHelper(this), vector);
    }

    template<typename T, size_t N>
    void trace(const Deque<T, N>& deque)
    {
        OffHeapCollectionTraceTrait<Deque<T, N>>::trace(Derived::fromHelper(this), deque);
    }

#if !ENABLE(OILPAN)
    // These trace methods are needed to allow compiling and calling trace on
    // transition types. We need to support calls in the non-oilpan build
    // because a fully transitioned type (which will have its trace method
    // called) might trace a field that is in transition. Once transition types
    // are removed these can be removed.
    template<typename T> void trace(const OwnPtr<T>&) { }
    template<typename T> void trace(const RefPtr<T>&) { }
    template<typename T> void trace(const RawPtr<T>&) { }
    template<typename T> void trace(const WeakPtr<T>&) { }

    // On non-oilpan builds, it is convenient to allow calling trace on
    // WillBeHeap{Vector,Deque}<FooPtrWillBeMember<T>>.
    // Forbid tracing on-heap objects in off-heap collections.
    // This is forbidden because convservative marking cannot identify
    // those off-heap collection backing stores.
    template<typename T, size_t inlineCapacity> void trace(const Vector<OwnPtr<T>, inlineCapacity>& vector)
    {
        STATIC_ASSERT_IS_NOT_GARBAGE_COLLECTED(T, "cannot trace garbage collected object inside Vector");
    }
    template<typename T, size_t inlineCapacity> void trace(const Vector<RefPtr<T>, inlineCapacity>& vector)
    {
        STATIC_ASSERT_IS_NOT_GARBAGE_COLLECTED(T, "cannot trace garbage collected object inside Vector");
    }
    template<typename T, size_t inlineCapacity> void trace(const Vector<RawPtr<T>, inlineCapacity>& vector)
    {
        STATIC_ASSERT_IS_NOT_GARBAGE_COLLECTED(T, "cannot trace garbage collected object inside Vector");
    }
    template<typename T, size_t inlineCapacity> void trace(const Vector<WeakPtr<T>, inlineCapacity>& vector)
    {
        STATIC_ASSERT_IS_NOT_GARBAGE_COLLECTED(T, "cannot trace garbage collected object inside Vector");
    }
    template<typename T, size_t N> void trace(const Deque<OwnPtr<T>, N>& deque)
    {
        STATIC_ASSERT_IS_NOT_GARBAGE_COLLECTED(T, "cannot trace garbage collected object inside Deque");
    }
    template<typename T, size_t N> void trace(const Deque<RefPtr<T>, N>& deque)
    {
        STATIC_ASSERT_IS_NOT_GARBAGE_COLLECTED(T, "cannot trace garbage collected object inside Deque");
    }
    template<typename T, size_t N> void trace(const Deque<RawPtr<T>, N>& deque)
    {
        STATIC_ASSERT_IS_NOT_GARBAGE_COLLECTED(T, "cannot trace garbage collected object inside Deque");
    }
    template<typename T, size_t N> void trace(const Deque<WeakPtr<T>, N>& deque)
    {
        STATIC_ASSERT_IS_NOT_GARBAGE_COLLECTED(T, "cannot trace garbage collected object inside Deque");
    }
#endif

    void markNoTracing(const void* pointer) { Derived::fromHelper(this)->mark(pointer, reinterpret_cast<TraceCallback>(0)); }
    void markHeaderNoTracing(HeapObjectHeader* header) { Derived::fromHelper(this)->markHeader(header, reinterpret_cast<TraceCallback>(0)); }
    template<typename T> void markNoTracing(const T* pointer) { Derived::fromHelper(this)->mark(pointer, reinterpret_cast<TraceCallback>(0)); }

    template<typename T, void (T::*method)(Visitor*)>
    void registerWeakMembers(const T* obj)
    {
        Derived::fromHelper(this)->registerWeakMembers(obj, &TraceMethodDelegate<T, method>::trampoline);
    }

    void registerWeakMembers(const void* object, WeakPointerCallback callback)
    {
        Derived::fromHelper(this)->registerWeakMembers(object, object, callback);
    }

    template<typename T> inline bool isAlive(T* obj)
    {
        // Check that we actually know the definition of T when tracing.
        static_assert(sizeof(T), "T must be fully defined");
        // The strongification of collections relies on the fact that once a
        // collection has been strongified, there is no way that it can contain
        // non-live entries, so no entries will be removed. Since you can't set
        // the mark bit on a null pointer, that means that null pointers are
        // always 'alive'.
        if (!obj)
            return true;
        return ObjectAliveTrait<T>::isHeapObjectAlive(Derived::fromHelper(this), obj);
    }
    template<typename T> inline bool isAlive(const Member<T>& member)
    {
        return isAlive(member.get());
    }
    template<typename T> inline bool isAlive(RawPtr<T> ptr)
    {
        return isAlive(ptr.get());
    }

private:
    template <typename T>
    static void handleWeakCell(Visitor* self, void* obj);
};

// Visitor is used to traverse the Blink object graph. Used for the
// marking phase of the mark-sweep garbage collector.
//
// Pointers are marked and pushed on the marking stack by calling the
// |mark| method with the pointer as an argument.
//
// Pointers within objects are traced by calling the |trace| methods
// with the object as an argument. Tracing objects will mark all of the
// contained pointers and push them on the marking stack.
class PLATFORM_EXPORT Visitor : public VisitorHelper<Visitor> {
public:
    friend class VisitorHelper<Visitor>;
    friend class InlinedGlobalMarkingVisitor;

    enum VisitorType {
        GlobalMarkingVisitorType,
        GenericVisitorType,
    };

    virtual ~Visitor() { }

    // FIXME: This is a temporary hack to cheat old Blink GC plugin checks.
    // Old GC Plugin doesn't accept calling VisitorHelper<Visitor>::trace
    // as a valid mark. This manual redirect worksaround the issue by
    // making the method declaration on Visitor class.
    template<typename T>
    void trace(const T& t)
    {
        VisitorHelper<Visitor>::trace(t);
    }

    using VisitorHelper<Visitor>::mark;

    // This method marks an object and adds it to the set of objects
    // that should have their trace method called. Since not all
    // objects have vtables we have to have the callback as an
    // explicit argument, but we can use the templated one-argument
    // mark method above to automatically provide the callback
    // function.
    virtual void mark(const void*, TraceCallback) = 0;

    // Used to mark objects during conservative scanning.
    virtual void markHeader(HeapObjectHeader*, TraceCallback) = 0;

    // Used to delay the marking of objects until the usual marking
    // including emphemeron iteration is done. This is used to delay
    // the marking of collection backing stores until we know if they
    // are reachable from locations other than the collection front
    // object. If collection backings are reachable from other
    // locations we strongify them to avoid issues with iterators and
    // weak processing.
    virtual void registerDelayedMarkNoTracing(const void*) = 0;

    // If the object calls this during the regular trace callback, then the
    // WeakPointerCallback argument may be called later, when the strong roots
    // have all been found. The WeakPointerCallback will normally use isAlive
    // to find out whether some pointers are pointing to dying objects. When
    // the WeakPointerCallback is done the object must have purged all pointers
    // to objects where isAlive returned false. In the weak callback it is not
    // allowed to touch other objects (except using isAlive) or to allocate on
    // the GC heap. Note that even removing things from HeapHashSet or
    // HeapHashMap can cause an allocation if the backing store resizes, but
    // these collections know to remove WeakMember elements safely.
    //
    // The weak pointer callbacks are run on the thread that owns the
    // object and other threads are not stopped during the
    // callbacks. Since isAlive is used in the callback to determine
    // if objects pointed to are alive it is crucial that the object
    // pointed to belong to the same thread as the object receiving
    // the weak callback. Since other threads have been resumed the
    // mark bits are not valid for objects from other threads.
    virtual void registerWeakMembers(const void*, const void*, WeakPointerCallback) = 0;
    using VisitorHelper<Visitor>::registerWeakMembers;

    virtual void registerWeakTable(const void*, EphemeronCallback, EphemeronCallback) = 0;
#if ENABLE(ASSERT)
    virtual bool weakTableRegistered(const void*) = 0;
#endif

    virtual bool isMarked(const void*) = 0;
    virtual bool ensureMarked(const void*) = 0;

#if ENABLE(GC_PROFILING)
    void setHostInfo(void* object, const String& name)
    {
        m_hostObject = object;
        m_hostName = name;
    }
#endif

    inline static bool canTraceEagerly()
    {
        ASSERT(m_stackFrameDepth);
        return m_stackFrameDepth->isSafeToRecurse();
    }

    inline static void configureEagerTraceLimit()
    {
        if (!m_stackFrameDepth)
            m_stackFrameDepth = new StackFrameDepth;
        m_stackFrameDepth->configureLimit();
    }

    inline bool isGlobalMarkingVisitor() const { return m_isGlobalMarkingVisitor; }

protected:
    explicit Visitor(VisitorType type)
        : m_isGlobalMarkingVisitor(type == GlobalMarkingVisitorType)
    { }

    virtual void registerWeakCellWithCallback(void**, WeakPointerCallback) = 0;
#if ENABLE(GC_PROFILING)
    virtual void recordObjectGraphEdge(const void*)
    {
        ASSERT_NOT_REACHED();
    }

    void* m_hostObject;
    String m_hostName;
#endif

#if ENABLE(ASSERT)
    virtual void checkMarkingAllowed() { }
#endif

private:
    static Visitor* fromHelper(VisitorHelper<Visitor>* helper) { return static_cast<Visitor*>(helper); }
    static StackFrameDepth* m_stackFrameDepth;

    bool m_isGlobalMarkingVisitor;
};

template <typename Derived>
template <typename T>
void VisitorHelper<Derived>::handleWeakCell(Visitor* self, void* obj)
{
    T** cell = reinterpret_cast<T**>(obj);
    if (*cell && !self->isAlive(*cell))
        *cell = nullptr;
}

// We trace vectors by using the trace trait on each element, which means you
// can have vectors of general objects (not just pointers to objects) that can
// be traced.
template<typename T, size_t N>
struct OffHeapCollectionTraceTrait<WTF::Vector<T, N, WTF::DefaultAllocator>> {
    typedef WTF::Vector<T, N, WTF::DefaultAllocator> Vector;

    template<typename VisitorDispatcher>
    static void trace(VisitorDispatcher visitor, const Vector& vector)
    {
        if (vector.isEmpty())
            return;
        for (typename Vector::const_iterator it = vector.begin(), end = vector.end(); it != end; ++it)
            TraceTrait<T>::trace(visitor, const_cast<T*>(it));
    }
};

template<typename T, size_t N>
struct OffHeapCollectionTraceTrait<WTF::Deque<T, N>> {
    typedef WTF::Deque<T, N> Deque;

    template<typename VisitorDispatcher>
    static void trace(VisitorDispatcher visitor, const Deque& deque)
    {
        if (deque.isEmpty())
            return;
        for (typename Deque::const_iterator it = deque.begin(), end = deque.end(); it != end; ++it)
            TraceTrait<T>::trace(visitor, const_cast<T*>(&(*it)));
    }
};

template<typename T, typename Traits = WTF::VectorTraits<T>>
class HeapVectorBacking;

template<typename Table>
class HeapHashTableBacking {
public:
    static void finalize(void* pointer);
};

template<typename T>
class DefaultTraceTrait<T, false> {
public:
    template<typename VisitorDispatcher>
    static void mark(VisitorDispatcher visitor, const T* t)
    {
        // Default mark method of the trait just calls the two-argument mark
        // method on the visitor. The second argument is the static trace method
        // of the trait, which by default calls the instance method
        // trace(Visitor*) on the object.
        //
        // If the trait allows it, invoke the trace callback right here on the
        // not-yet-marked object.
        if (TraceEagerlyTrait<T>::value) {
            // Protect against too deep trace call chains, and the
            // unbounded system stack usage they can bring about.
            //
            // Assert against deep stacks so as to flush them out,
            // but test and appropriately handle them should they occur
            // in release builds.
            //
            // ASan adds extra stack usage, so disable the assert when it is
            // enabled so as to avoid testing against a much lower & too low,
            // stack depth threshold.
            //
            // FIXME: visitor->isMarked(t) exception is to allow empty trace()
            // calls from HashTable weak processing. Remove the condition once
            // it is refactored.
#if !defined(ADDRESS_SANITIZER)
            ASSERT(visitor->canTraceEagerly() || visitor->isMarked(t));
#endif
            if (LIKELY(visitor->canTraceEagerly())) {
                if (visitor->ensureMarked(t)) {
                    TraceTrait<T>::trace(visitor, const_cast<T*>(t));
                }
                return;
            }
        }
        visitor->mark(const_cast<T*>(t), &TraceTrait<T>::trace);
    }

#if ENABLE(ASSERT)
    static void checkGCInfo(const T* t)
    {
        assertObjectHasGCInfo(const_cast<T*>(t), GCInfoTrait<T>::index());
    }
#endif
};

template<typename T>
class DefaultTraceTrait<T, true> {
public:
    template<typename VisitorDispatcher>
    static void mark(VisitorDispatcher visitor, const T* self)
    {
        if (!self)
            return;

        // If you hit this ASSERT, it means that there is a dangling pointer
        // from a live thread heap to a dead thread heap. We must eliminate
        // the dangling pointer.
        // Release builds don't have the ASSERT, but it is OK because
        // release builds will crash at the following self->adjustAndMark
        // because all the entries of the orphaned heaps are zeroed out and
        // thus the item does not have a valid vtable.
        ASSERT(!pageFromObject(self)->orphaned());
        self->adjustAndMark(visitor);
    }

#if ENABLE(ASSERT)
    static void checkGCInfo(const T*) { }
#endif
};

template<typename T, bool = NeedsAdjustAndMark<T>::value> class DefaultObjectAliveTrait;

template<typename T>
class DefaultObjectAliveTrait<T, false> {
public:
    template<typename VisitorDispatcher>
    static bool isHeapObjectAlive(VisitorDispatcher visitor, T* obj)
    {
        return visitor->isMarked(obj);
    }
};

template<typename T>
class DefaultObjectAliveTrait<T, true> {
public:
    template<typename VisitorDispatcher>
    static bool isHeapObjectAlive(VisitorDispatcher visitor, T* obj)
    {
        return obj->isHeapObjectAlive(visitor);
    }
};

template<typename T>
template<typename VisitorDispatcher>
bool ObjectAliveTrait<T>::isHeapObjectAlive(VisitorDispatcher visitor, T* obj)
{
    return DefaultObjectAliveTrait<T>::isHeapObjectAlive(visitor, obj);
}

// The GarbageCollectedMixin interface and helper macro
// USING_GARBAGE_COLLECTED_MIXIN can be used to automatically define
// TraceTrait/ObjectAliveTrait on non-leftmost deriving classes
// which need to be garbage collected.
//
// Consider the following case:
// class B {};
// class A : public GarbageCollected, public B {};
//
// We can't correctly handle "Member<B> p = &a" as we can't compute addr of
// object header statically. This can be solved by using GarbageCollectedMixin:
// class B : public GarbageCollectedMixin {};
// class A : public GarbageCollected, public B {
//   USING_GARBAGE_COLLECTED_MIXIN(A)
// };
//
// With the helper, as long as we are using Member<B>, TypeTrait<B> will
// dispatch adjustAndMark dynamically to find collect addr of the object header.
// Note that this is only enabled for Member<B>. For Member<A> which we can
// compute the object header addr statically, this dynamic dispatch is not used.

class PLATFORM_EXPORT GarbageCollectedMixin {
public:
    typedef int IsGarbageCollectedMixinMarker;
    virtual void adjustAndMark(Visitor*) const = 0;
    virtual bool isHeapObjectAlive(Visitor*) const = 0;
    virtual void trace(Visitor*) { }
#if ENABLE(INLINED_TRACE)
    virtual void adjustAndMark(InlinedGlobalMarkingVisitor) const = 0;
    virtual bool isHeapObjectAlive(InlinedGlobalMarkingVisitor) const = 0;
    virtual void trace(InlinedGlobalMarkingVisitor);
#endif
};

#define DEFINE_GARBAGE_COLLECTED_MIXIN_METHODS(VISITOR, TYPE)                                                                           \
public:                                                                                                                                 \
    virtual void adjustAndMark(VISITOR visitor) const override                                                                          \
    {                                                                                                                                   \
        typedef WTF::IsSubclassOfTemplate<typename WTF::RemoveConst<TYPE>::Type, blink::GarbageCollected> IsSubclassOfGarbageCollected; \
        static_assert(IsSubclassOfGarbageCollected::value, "only garbage collected objects can have garbage collected mixins");         \
        if (TraceEagerlyTrait<TYPE>::value) {                                                                                           \
            if (visitor->ensureMarked(static_cast<const TYPE*>(this)))                                                                  \
                TraceTrait<TYPE>::trace(visitor, const_cast<TYPE*>(this));                                                              \
            return;                                                                                                                     \
        }                                                                                                                               \
        visitor->mark(static_cast<const TYPE*>(this), &blink::TraceTrait<TYPE>::trace);                                                 \
    }                                                                                                                                   \
    virtual bool isHeapObjectAlive(VISITOR visitor) const override                                                                      \
    {                                                                                                                                   \
        return visitor->isAlive(this);                                                                                                  \
    }                                                                                                                                   \
private:

// A C++ object's vptr will be initialized to its leftmost base's vtable after
// the constructors of all its subclasses have run, so if a subclass constructor
// tries to access any of the vtbl entries of its leftmost base prematurely,
// it'll find an as-yet incorrect vptr and fail. Which is exactly what a
// garbage collector will try to do if it tries to access the leftmost base
// while one of the subclass constructors of a GC mixin object triggers a GC.
// It is consequently not safe to allow any GCs while these objects are under
// (sub constructor) construction.
//
// To prevent GCs in that restricted window of a mixin object's construction:
//
//  - The initial allocation of the mixin object will enter a no GC scope.
//    This is done by overriding 'operator new' for mixin instances.
//  - When the constructor for the mixin is invoked, after all the
//    derived constructors have run, it will invoke the constructor
//    for a field whose only purpose is to leave the GC scope.
//    GarbageCollectedMixinConstructorMarker's constructor takes care of
//    this and the field is declared by way of USING_GARBAGE_COLLECTED_MIXIN().
//
// If a GC mixin class, declared so using USING_GARBAGE_COLLECTED_MIXIN(), derives
// leftmost from another such USING_GARBAGE_COLLECTED_MIXIN-mixin class, extra
// care and handling is needed for the above two steps; see comment below.


// Trait template to resolve the effective base GC class to use when allocating
// objects at some type T. Requires specialization for Node and CSSValue
// derived types to have these be allocated on appropriate heaps.
//
// FIXME: this trait is needed to support Node's overriding 'operator new'
// implementation in combination with GC mixins (step 1 above.) Should
// Node' operator new no longer be needed, this trait can be removed.
template<typename T, typename Enabled = void>
class EffectiveGCBaseTrait {
public:
    using Type = T;
};

#define ALLOCATE_ALL_INSTANCES_ON_SAME_GC_HEAP(TYPE)  \
template<typename T>                                  \
class EffectiveGCBaseTrait<T, typename WTF::EnableIf<WTF::IsSubclass<T, blink::TYPE>::value>::Type> { \
public:                \
    using Type = TYPE; \
}

#if ENABLE(OILPAN)
#define WILL_HAVE_ALL_INSTANCES_ON_SAME_GC_HEAP(TYPE) ALLOCATE_ALL_INSTANCES_ON_SAME_GC_HEAP(TYPE)
#else
#define WILL_HAVE_ALL_INSTANCES_ON_SAME_GC_HEAP(TYPE)
#endif

#define DEFINE_GARBAGE_COLLECTED_MIXIN_CONSTRUCTOR_MARKER(TYPE)                         \
public:                                                                                 \
    GC_PLUGIN_IGNORE("crbug.com/456823")                                                \
    void* operator new(size_t size)                                                     \
    {                                                                                   \
        void* object = Heap::allocate<typename EffectiveGCBaseTrait<TYPE>::Type>(size); \
        ThreadState* state = ThreadStateFor<ThreadingTrait<TYPE>::Affinity>::state();   \
        state->enterGCForbiddenScope(TYPE::mixinLevels);                                \
        return object;                                                                  \
    }                                                                                   \
private:                                                                                \
    GarbageCollectedMixinConstructorMarker<TYPE> m_mixinConstructorMarker;

#if ENABLE(ASSERT)
#define DEFINE_GARBAGE_COLLECTED_MIXIN_NONNESTED_DEBUG()     \
protected:                                                   \
    virtual void mixinsCannotBeImplicitlyNested() final { /* Please use USING_GARBAGE_COLLECTED_MIXIN_NESTED(Mixin, DerivedFromOtherMixin); */ }
#define DEFINE_GARBAGE_COLLECTED_MIXIN_NESTED_DEBUG(TYPE, SUBTYPE)   \
protected:                                                           \
    static void declaredNestedMixinsMustBeAccurate()                 \
    {                                                                \
        static_assert(WTF::IsSubclass<blink::TYPE, blink::SUBTYPE>::value, "Mixin class does not derive from its stated, nested mixin class."); \
    }
#else
#define DEFINE_GARBAGE_COLLECTED_MIXIN_NONNESTED_DEBUG()
#define DEFINE_GARBAGE_COLLECTED_MIXIN_NESTED_DEBUG(TYPE, SUBTYPE)
#endif

// Mixins that wrap/nest others requires extra handling:
//
//  class A : public GarbageCollected<A>, public GarbageCollectedMixin {
//  USING_GARBAGE_COLLECTED_MIXIN(A);
//  ...
//  }'
//  public B final : public A, public SomeOtherMixinInterface {
//  USING_GARBAGE_COLLECTED_MIXIN(B);
//  ...
//  };
//
// The "operator new" for B will enter the forbidden GC scope, but
// upon construction, two GarbageCollectedMixinConstructorMarker constructors
// will run -- one for A (first) and another for B (secondly.) Only
// the second one should leave the forbidden GC scope.
//
// Arrange for the balanced use of the forbidden GC scope counter by
// adding on the number of mixin constructor markers for a type.
// This is equal to the how many GC mixins that the type nests.
//
// FIXME: We currently require that a nested GC mixin (e.g., B)
// must declare what it nests: USING_GARBAGE_COLLECTED_MIXIN_NESTED(B, A);
// must be used for it. It's a static error if it doesn't.
//
// It would however be preferable to statically derive the
// "mixin nesting level" for these mixin types and use that without
// having to explicitly state the particular type that a mixin nests.
// It seems like it could be expressible as a compile-time type
// computation..

#define DEFINE_GARBAGE_COLLECTED_MIXIN_NONNESTED()           \
protected:                                                   \
    static const unsigned mixinLevels = 1;
#define DEFINE_GARBAGE_COLLECTED_MIXIN_NESTED(TYPE, SUBTYPE)      \
protected:                                                        \
    static const unsigned mixinLevels = SUBTYPE::mixinLevels + 1;

#if ENABLE(INLINED_TRACE)
#define USING_GARBAGE_COLLECTED_MIXIN_BASE(TYPE)                  \
    DEFINE_GARBAGE_COLLECTED_MIXIN_METHODS(blink::Visitor*, TYPE) \
    DEFINE_GARBAGE_COLLECTED_MIXIN_METHODS(blink::InlinedGlobalMarkingVisitor, TYPE) \
    DEFINE_GARBAGE_COLLECTED_MIXIN_CONSTRUCTOR_MARKER(TYPE)
#else
#define USING_GARBAGE_COLLECTED_MIXIN_BASE(TYPE)                  \
    DEFINE_GARBAGE_COLLECTED_MIXIN_METHODS(blink::Visitor*, TYPE) \
    DEFINE_GARBAGE_COLLECTED_MIXIN_CONSTRUCTOR_MARKER(TYPE)
#endif

#define USING_GARBAGE_COLLECTED_MIXIN(TYPE)          \
    DEFINE_GARBAGE_COLLECTED_MIXIN_NONNESTED_DEBUG() \
    DEFINE_GARBAGE_COLLECTED_MIXIN_NONNESTED()       \
    USING_GARBAGE_COLLECTED_MIXIN_BASE(TYPE)

#define USING_GARBAGE_COLLECTED_MIXIN_NESTED(TYPE, SUBTYPE)    \
    DEFINE_GARBAGE_COLLECTED_MIXIN_NESTED_DEBUG(TYPE, SUBTYPE) \
    DEFINE_GARBAGE_COLLECTED_MIXIN_NESTED(TYPE, SUBTYPE)       \
    USING_GARBAGE_COLLECTED_MIXIN_BASE(TYPE)

#if ENABLE(OILPAN)
#define WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(TYPE) USING_GARBAGE_COLLECTED_MIXIN(TYPE)
#define WILL_BE_USING_GARBAGE_COLLECTED_MIXIN_NESTED(TYPE, NESTEDMIXIN) USING_GARBAGE_COLLECTED_MIXIN_NESTED(TYPE, NESTEDMIXIN)
#else
#define WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(TYPE)
#define WILL_BE_USING_GARBAGE_COLLECTED_MIXIN_NESTED(TYPE, NESTEDMIXIN)
#endif

// An empty class with a constructor that's arranged invoked when all derived constructors
// of a mixin instance have completed and it is safe to allow GCs again. See
// AllocateObjectTrait<> comment for more.
//
// USING_GARBAGE_COLLECTED_MIXIN() declares a GarbageCollectedMixinConstructorMarker<> private
// field. By following Blink convention of using the macro at the top of a class declaration,
// its constructor will run first.
template<typename T>
class GarbageCollectedMixinConstructorMarker {
public:
    GarbageCollectedMixinConstructorMarker()
    {
        // FIXME: if prompt conservative GCs are needed, forced GCs that
        // were denied while within this scope, could now be performed.
        // For now, assume the next out-of-line allocation request will
        // happen soon enough and take care of it. Mixin objects aren't
        // overly common.
        ThreadState* state = ThreadStateFor<ThreadingTrait<T>::Affinity>::state();
        state->leaveGCForbiddenScope();
    }
};

#if ENABLE(GC_PROFILING)
template<typename T>
struct TypenameStringTrait {
    static const String& get()
    {
        DEFINE_STATIC_LOCAL(String, typenameString, (WTF::extractTypeNameFromFunctionName(WTF::extractNameFunction<T>())));
        return typenameString;
    }
};
#endif

// s_gcInfoTable holds the per-class GCInfo descriptors; each heap
// object header keeps its index into this table.
extern PLATFORM_EXPORT GCInfo const** s_gcInfoTable;

class GCInfoTable {
public:
    PLATFORM_EXPORT static void ensureGCInfoIndex(const GCInfo*, size_t*);

    static void init();
    static void shutdown();

    // The (max + 1) GCInfo index supported.
    static const size_t maxIndex = 1 << 15;

private:
    static void resize();

    static int s_gcInfoIndex;
    static size_t s_gcInfoTableSize;
};

// This macro should be used when returning a unique 15 bit integer
// for a given gcInfo.
#define RETURN_GCINFO_INDEX()                                  \
    static size_t gcInfoIndex = 0;                             \
    ASSERT(s_gcInfoTable);                                     \
    if (!acquireLoad(&gcInfoIndex))                            \
        GCInfoTable::ensureGCInfoIndex(&gcInfo, &gcInfoIndex); \
    ASSERT(gcInfoIndex >= 1);                                  \
    ASSERT(gcInfoIndex < GCInfoTable::maxIndex);               \
    return gcInfoIndex;

template<typename T>
struct GCInfoAtBase {
    static size_t index()
    {
        static const GCInfo gcInfo = {
            TraceTrait<T>::trace,
            FinalizerTrait<T>::finalize,
            FinalizerTrait<T>::nonTrivialFinalizer,
            WTF::IsPolymorphic<T>::value,
#if ENABLE(GC_PROFILING)
            TypenameStringTrait<T>::get()
#endif
        };
        RETURN_GCINFO_INDEX();
    }
};

template<typename T, bool = WTF::IsSubclassOfTemplate<typename WTF::RemoveConst<T>::Type, GarbageCollected>::value> struct GetGarbageCollectedBase;

template<typename T>
struct GetGarbageCollectedBase<T, true> {
    typedef typename T::GarbageCollectedBase type;
};

template<typename T>
struct GetGarbageCollectedBase<T, false> {
    typedef T type;
};

template<typename T>
struct GCInfoTrait {
    static size_t index()
    {
        return GCInfoAtBase<typename GetGarbageCollectedBase<T>::type>::index();
    }
};

} // namespace blink

#endif // Visitor_h
