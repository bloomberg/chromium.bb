// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GarbageCollected_h
#define GarbageCollected_h

#include "platform/heap/ThreadState.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/TypeTraits.h"

namespace blink {

template <typename T>
class GarbageCollected;

// GC_PLUGIN_IGNORE is used to make the plugin ignore a particular class or
// field when checking for proper usage.  When using GC_PLUGIN_IGNORE
// a bug-number should be provided as an argument where the bug describes
// what needs to happen to remove the GC_PLUGIN_IGNORE again.
#if defined(__clang__)
#define GC_PLUGIN_IGNORE(bug) \
  __attribute__((annotate("blink_gc_plugin_ignore")))
#else
#define GC_PLUGIN_IGNORE(bug)
#endif

// Template to determine if a class is a GarbageCollectedMixin by checking if it
// has IsGarbageCollectedMixinMarker
template <typename T>
struct IsGarbageCollectedMixin {
 private:
  typedef char YesType;
  struct NoType {
    char padding[8];
  };

  template <typename U>
  static YesType CheckMarker(typename U::IsGarbageCollectedMixinMarker*);
  template <typename U>
  static NoType CheckMarker(...);

 public:
  static const bool value = sizeof(CheckMarker<T>(nullptr)) == sizeof(YesType);
};

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
//   USING_GARBAGE_COLLECTED_MIXIN(A);
// };
//
// With the helper, as long as we are using Member<B>, TypeTrait<B> will
// dispatch adjustAndMark dynamically to find collect addr of the object header.
// Note that this is only enabled for Member<B>. For Member<A> which we can
// compute the object header addr statically, this dynamic dispatch is not used.
//
class PLATFORM_EXPORT GarbageCollectedMixin {
 public:
  typedef int IsGarbageCollectedMixinMarker;
  virtual void AdjustAndMark(Visitor*) const = 0;
  virtual void Trace(Visitor*) {}
  virtual bool IsHeapObjectAlive() const = 0;
};

#define DEFINE_GARBAGE_COLLECTED_MIXIN_METHODS(VISITOR, TYPE)                 \
 public:                                                                      \
  void AdjustAndMark(VISITOR visitor) const override {                        \
    typedef WTF::IsSubclassOfTemplate<typename std::remove_const<TYPE>::type, \
                                      blink::GarbageCollected>                \
        IsSubclassOfGarbageCollected;                                         \
    static_assert(                                                            \
        IsSubclassOfGarbageCollected::value,                                  \
        "only garbage collected objects can have garbage collected mixins");  \
    if (TraceEagerlyTrait<TYPE>::value) {                                     \
      if (visitor->EnsureMarked(static_cast<const TYPE*>(this)))              \
        TraceTrait<TYPE>::Trace(visitor, const_cast<TYPE*>(this));            \
      return;                                                                 \
    }                                                                         \
    visitor->Mark(static_cast<const TYPE*>(this),                             \
                  &blink::TraceTrait<TYPE>::Trace);                           \
  }                                                                           \
                                                                              \
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

#define DEFINE_GARBAGE_COLLECTED_MIXIN_CONSTRUCTOR_MARKER(TYPE)          \
 public:                                                                 \
  GC_PLUGIN_IGNORE("crbug.com/456823")                                   \
  NO_SANITIZE_UNRELATED_CAST void* operator new(size_t size) {           \
    void* object =                                                       \
        TYPE::AllocateObject(size, IsEagerlyFinalizedType<TYPE>::value); \
    ThreadState* state =                                                 \
        ThreadStateFor<ThreadingTrait<TYPE>::kAffinity>::GetState();     \
    state->EnterGCForbiddenScopeIfNeeded(                                \
        &(reinterpret_cast<TYPE*>(object)->mixin_constructor_marker_));  \
    return object;                                                       \
  }                                                                      \
  GarbageCollectedMixinConstructorMarker mixin_constructor_marker_;      \
                                                                         \
 private:

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
// will run -- one for A (first) and another for B (secondly). Only
// the second one should leave the forbidden GC scope. This is realized by
// recording the address of B's GarbageCollectedMixinConstructorMarker
// when the "operator new" for B runs, and leaving the forbidden GC scope
// when the constructor of the recorded GarbageCollectedMixinConstructorMarker
// runs.
#define USING_GARBAGE_COLLECTED_MIXIN(TYPE)                     \
  IS_GARBAGE_COLLECTED_TYPE();                                  \
  DEFINE_GARBAGE_COLLECTED_MIXIN_METHODS(blink::Visitor*, TYPE) \
  DEFINE_GARBAGE_COLLECTED_MIXIN_CONSTRUCTOR_MARKER(TYPE)       \
 public:                                                        \
  bool IsHeapObjectAlive() const override {                     \
    return ThreadHeap::IsHeapObjectAlive(this);                 \
  }                                                             \
                                                                \
 private:

// An empty class with a constructor that's arranged invoked when all derived
// constructors of a mixin instance have completed and it is safe to allow GCs
// again. See AllocateObjectTrait<> comment for more.
//
// USING_GARBAGE_COLLECTED_MIXIN() declares a
// GarbageCollectedMixinConstructorMarker<> private field. By following Blink
// convention of using the macro at the top of a class declaration, its
// constructor will run first.
class GarbageCollectedMixinConstructorMarker {
 public:
  GarbageCollectedMixinConstructorMarker() {
    // FIXME: if prompt conservative GCs are needed, forced GCs that
    // were denied while within this scope, could now be performed.
    // For now, assume the next out-of-line allocation request will
    // happen soon enough and take care of it. Mixin objects aren't
    // overly common.
    ThreadState* state = ThreadState::Current();
    state->LeaveGCForbiddenScopeIfNeeded(this);
  }
};

// Base class for objects allocated in the Blink garbage-collected heap.
//
// Defines a 'new' operator that allocates the memory in the heap.  'delete'
// should not be called on objects that inherit from GarbageCollected.
//
// Instances of GarbageCollected will *NOT* get finalized.  Their destructor
// will not be called.  Therefore, only classes that have trivial destructors
// with no semantic meaning (including all their subclasses) should inherit from
// GarbageCollected.  If there are non-trival destructors in a given class or
// any of its subclasses, GarbageCollectedFinalized should be used which
// guarantees that the destructor is called on an instance when the garbage
// collector determines that it is no longer reachable.
template <typename T>
class GarbageCollected;

// Base class for objects allocated in the Blink garbage-collected heap.
//
// Defines a 'new' operator that allocates the memory in the heap.  'delete'
// should not be called on objects that inherit from GarbageCollected.
//
// Instances of GarbageCollectedFinalized will have their destructor called when
// the garbage collector determines that the object is no longer reachable.
template <typename T>
class GarbageCollectedFinalized : public GarbageCollected<T> {
  WTF_MAKE_NONCOPYABLE(GarbageCollectedFinalized);

 protected:
  // finalizeGarbageCollectedObject is called when the object is freed from
  // the heap.  By default finalization means calling the destructor on the
  // object.  finalizeGarbageCollectedObject can be overridden to support
  // calling the destructor of a subclass.  This is useful for objects without
  // vtables that require explicit dispatching.  The name is intentionally a
  // bit long to make name conflicts less likely.
  void FinalizeGarbageCollectedObject() { static_cast<T*>(this)->~T(); }

  GarbageCollectedFinalized() {}
  ~GarbageCollectedFinalized() {}

  template <typename U>
  friend struct HasFinalizer;
  template <typename U, bool>
  friend struct FinalizerTraitImpl;
};

template <typename T,
          bool = WTF::IsSubclassOfTemplate<typename std::remove_const<T>::type,
                                           GarbageCollected>::value>
class NeedsAdjustAndMark;

template <typename T>
class NeedsAdjustAndMark<T, true> {
  static_assert(sizeof(T), "T must be fully defined");

 public:
  static const bool value = false;
};

template <typename T>
class NeedsAdjustAndMark<T, false> {
  static_assert(sizeof(T), "T must be fully defined");

 public:
  static const bool value =
      IsGarbageCollectedMixin<typename std::remove_const<T>::type>::value;
};

// TODO(sof): migrate to wtf/TypeTraits.h
template <typename T>
class IsFullyDefined {
  using TrueType = char;
  struct FalseType {
    char dummy[2];
  };

  template <typename U, size_t sz = sizeof(U)>
  static TrueType IsSizeofKnown(U*);
  static FalseType IsSizeofKnown(...);
  static T& t_;

 public:
  static const bool value = sizeof(TrueType) == sizeof(IsSizeofKnown(&t_));
};

}  // namespace blink

#endif
