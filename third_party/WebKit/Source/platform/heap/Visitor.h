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

#include <memory>
#include "platform/PlatformExport.h"
#include "platform/heap/GarbageCollected.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/HashTraits.h"
#include "platform/wtf/TypeTraits.h"

namespace blink {

template <typename T>
class GarbageCollected;
class HeapObjectHeader;
template <typename T>
class TraceTrait;
class ThreadState;
class Visitor;
template <typename T>
class SameThreadCheckedMember;
template <typename T>
class TraceWrapperMember;

// The TraceMethodDelegate is used to convert a trace method for type T to a
// TraceCallback.  This allows us to pass a type's trace method as a parameter
// to the PersistentNode constructor. The PersistentNode constructor needs the
// specific trace method due an issue with the Windows compiler which
// instantiates even unused variables. This causes problems
// in header files where we have only forward declarations of classes.
template <typename T, void (T::*method)(Visitor*)>
struct TraceMethodDelegate {
  STATIC_ONLY(TraceMethodDelegate);
  static void Trampoline(Visitor* visitor, void* self) {
    (reinterpret_cast<T*>(self)->*method)(visitor);
  }
};

// Visitor is used to traverse Oilpan's object graph.
class PLATFORM_EXPORT Visitor {
 public:
  Visitor(ThreadState*);
  virtual ~Visitor();

  inline ThreadState* GetState() const { return state_; }
  inline ThreadHeap& Heap() const { return GetState()->Heap(); }

  // Static visitor implementation forwarding to dynamic interface.

  // Member version of the one-argument templated trace method.
  template <typename T>
  void Trace(const Member<T>& t) {
    Trace(t.Get());
  }

  template <typename T>
  void Trace(const TraceWrapperMember<T>& t) {
    Trace(*(static_cast<const Member<T>*>(&t)));
  }

  template <typename T>
  void Trace(const SameThreadCheckedMember<T>& t) {
    Trace(*(static_cast<const Member<T>*>(&t)));
  }

  // Fallback methods used only when we need to trace raw pointers of T. This is
  // the case when a member is a union where we do not support members.
  template <typename T>
  void Trace(const T* t) {
    Trace(const_cast<T*>(t));
  }

  template <typename T>
  void Trace(T* t) {
    static_assert(sizeof(T), "T must be fully defined");
    static_assert(IsGarbageCollectedType<T>::value,
                  "T needs to be a garbage collected object");
    if (!t)
      return;
    Visit(const_cast<void*>(reinterpret_cast<const void*>(t)),
          TraceTrait<T>::Trace, TraceTrait<T>::Mark);
  }

  // WeakMember version of the templated trace method. It doesn't keep
  // the traced thing alive, but will write null to the WeakMember later
  // if the pointed-to object is dead. It's lying for this to be const,
  // but the overloading resolver prioritizes constness too high when
  // picking the correct overload, so all these trace methods have to have
  // the same constness on their argument to allow the type to decide.
  template <typename T>
  void Trace(const WeakMember<T>& t) {
    static_assert(sizeof(T), "T must be fully defined");
    static_assert(IsGarbageCollectedType<T>::value,
                  "T needs to be a garbage collected object");
    RegisterWeakCell(const_cast<WeakMember<T>&>(t).Cell());
  }

  // Fallback trace method for part objects to allow individual trace methods
  // to trace through a part object with visitor->trace(m_partObject). This
  // takes a const argument, because otherwise it will match too eagerly: a
  // non-const argument would match a non-const Vector<T>& argument better
  // than the specialization that takes const Vector<T>&. For a similar reason,
  // the other specializations take a const argument even though they are
  // usually used with non-const arguments, otherwise this function would match
  // too well.
  template <typename T>
  void Trace(const T& t) {
    static_assert(sizeof(T), "T must be fully defined");
    if (std::is_polymorphic<T>::value) {
      intptr_t vtable = *reinterpret_cast<const intptr_t*>(&t);
      if (!vtable)
        return;
    }
    TraceTrait<T>::Trace(this, &const_cast<T&>(t));
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
  template <typename T>
  void RegisterWeakCell(T** cell) {
    RegisterWeakCallback(
        reinterpret_cast<void**>(
            const_cast<typename std::remove_const<T>::type**>(cell)),
        &HandleWeakCell<T>);
  }

  template <typename T, void (T::*method)(Visitor*)>
  void RegisterWeakMembers(const T* obj) {
    RegisterWeakCallback(const_cast<T*>(obj),
                         &TraceMethodDelegate<T, method>::Trampoline);
  }

  // Dynamic visitor interface.

  // Visits an object through a strong reference.
  virtual void Visit(void*, TraceCallback, TraceCallback) = 0;

  // Registers backing store pointers so that they can be moved and properly
  // updated.
  virtual void RegisterBackingStoreReference(void* slot) = 0;
  virtual void RegisterBackingStoreCallback(void* backing_store,
                                            MovingObjectCallback,
                                            void* callback_data) = 0;

  // Used to delay the marking of objects until the usual marking including
  // ephemeron iteration is done. This is used to delay the marking of
  // collection backing stores until we know if they are reachable from
  // locations other than the collection front object. If collection backings
  // are reachable from other locations we strongify them to avoid issues with
  // iterators and weak processing.
  virtual void RegisterDelayedMarkNoTracing(const void* pointer) = 0;

  // Used to register ephemeron callbacks.
  virtual void RegisterWeakTable(const void* closure,
                                 EphemeronCallback iteration_callback,
                                 EphemeronCallback iteration_done_callback) = 0;
#if DCHECK_IS_ON()
  virtual bool WeakTableRegistered(const void* closure) = 0;
#endif

  // |WeakCallback| will usually use |ObjectAliveTrait| to figure out liveness
  // of any children of |closure|. Upon return from the callback all references
  // to dead objects must have been purged. Any operation that extends the
  // object graph, including allocation or reviving objects, is prohibited.
  // Clearing out additional pointers is allowed. Note that removing elements
  // from heap collections such as HeapHashSet can cause an allocation if the
  // backing store requires resizing. These collections know how to deal with
  // WeakMember elements though.
  virtual void RegisterWeakCallback(void* closure, WeakCallback) = 0;

 private:
  template <typename T>
  static void HandleWeakCell(Visitor* self, void*);

  ThreadState* const state_;
};

// Visitor used to mark Oilpan objects.
class PLATFORM_EXPORT MarkingVisitor final : public Visitor {
 public:
  enum MarkingMode {
    // This is a default visitor. This is used for GCType=GCWithSweep
    // and GCType=GCWithoutSweep.
    kGlobalMarking,
    // This visitor just marks objects and ignores weak processing.
    // This is used for GCType=TakeSnapshot.
    kSnapshotMarking,
    // This visitor is used to trace objects during weak processing.
    // This visitor is allowed to trace only already marked objects.
    kWeakProcessing,
    // Perform global marking along with preparing for additional sweep
    // compaction of heap arenas afterwards. Compared to the GlobalMarking
    // visitor, this visitor will also register references to objects
    // that might be moved during arena compaction -- the compaction
    // pass will then fix up those references when the object move goes
    // ahead.
    kGlobalMarkingWithCompaction,
  };

  static std::unique_ptr<MarkingVisitor> Create(ThreadState*, MarkingMode);

  MarkingVisitor(ThreadState*, MarkingMode);
  virtual ~MarkingVisitor();

  inline MarkingMode GetMarkingMode() const { return marking_mode_; }

  // Marking implementation.

  // This method marks an object and adds it to the set of objects that should
  // have their trace method called. Since not all objects have vtables we have
  // to have the callback as an explicit argument, but we can use the templated
  // one-argument mark method above to automatically provide the callback
  // function.
  inline void Mark(const void* object_pointer, TraceCallback);

  // Used to mark objects during conservative scanning.
  inline void MarkHeader(HeapObjectHeader*, TraceCallback);
  inline void MarkHeaderNoTracing(HeapObjectHeader*);

  // Marks the header of an object. Is used for eagerly tracing of objects.
  inline bool EnsureMarked(const void* pointer);

  // Used for eagerly marking objects and for delayed marking of backing stores
  // when the actual payload is processed differently, e.g., by weak handling.
  inline void MarkNoTracing(const void* pointer) {
    Mark(pointer, reinterpret_cast<TraceCallback>(0));
  }

  // Implementation of the visitor interface. See above for descriptions.

  void Visit(void* object,
             TraceCallback trace_callback,
             TraceCallback mark_callback) final {
    mark_callback(this, object);
  }

  void RegisterBackingStoreReference(void* slot) final;
  void RegisterBackingStoreCallback(void* backing_store,
                                    MovingObjectCallback,
                                    void* callback_data) final;
  void RegisterDelayedMarkNoTracing(const void* pointer) final;
  void RegisterWeakTable(const void* closure,
                         EphemeronCallback iteration_callback,
                         EphemeronCallback iteration_done_callback) final;
#if DCHECK_IS_ON()
  bool WeakTableRegistered(const void* closure) final;
#endif
  void RegisterWeakCallback(void* closure, WeakCallback) final;

 private:
  static void MarkNoTracingCallback(Visitor*, void*);

  const MarkingMode marking_mode_;
};

}  // namespace blink

#endif  // Visitor_h
