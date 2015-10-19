# Blink GC API reference

Work in progress.

[TOC]

## Header file

Unless otherwise noted, any of the primitives explained in this page requires the following `#include` statement:

```c++
#include "platform/heap/Handle.h"
```

## Base class templates

### GarbageCollected

A class that wants the lifetime management of its instances to be managed by Blink GC (Oilpan), it must inherit from
`GarbageCollected<YourClass>`.

```c++
class YourClass : public GarbageCollected<YourClass> {
    // ...
};
```

Instances of such classes are said to be *on Oilpan heap*, or *on heap* for short, while instances of other classes
are called *off heap*. In the rest of this document, the terms "on heap" or "on-heap objects" are used to mean the
objects on Oilpan heap instead of on normal (default) dynamic allocator's heap space.

You can create an instance of your class normally through `new`, while you may not free the object with `delete`,
as the Blink GC system is responsible for deallocating the object once it determines the object is unreachable.

You may not allocate an on-heap object on stack.

Your class may need to have a tracing method. See [Tracing](#Tracing) for details.

If your class needs finalization (i.e. some work needs to be done on destruction), use
[GarbageCollectedFinalized](#GarbageCollectedFinalized) instead.

`GarbageCollected<T>` or any class deriving from `GarbageCollected<T>`, directly or indirectly, must be the first
element in a base class list (left-most deviation rule).

```c++
class A : public GarbageCollected<A>, public P { // OK, GarbageCollected<A> is left-most.
};

class B : public A, public Q { // OK, A is left-most.
};

// class C : public R, public A { // BAD, A must be the first base class.
// };
```

### GarbageCollectedFinalized

If you want to make your class garbage collected and the class needs finalization, your class needs to inherit from
`GarbageCollectedFinalized<YourClass>` instead of `GarbageCollected<YourClass>`.

A class is said to *need finalization* when it meets either of the following criteria:

*   It has non-empty destructor; or
*   It has a member that needs finalization.

```c++
class YourClass : public GarbageCollectedFinalized<YourClass> {
public:
    ~YourClass() { ... } // Non-empty destructor means finalization is needed.

private:
    RefPtr<Something> m_something; // RefPtr<> has non-empty destructor, so finalization is needed.
};
```

Note that finalization is done at an arbitrary time after the object becomes unreachable.

Any destructor executed within the finalization period must not touch any other on-heap object, because destructors
can be executed in any order. If there is a need of having such destructor, consider using
[EAGERLY_FINALIZE](#EAGERLY_FINALIZE).

Because `GarbageCollectedFinalized<T>` is a special case of `GarbageCollected<T>`, all other restrictions that apply
to `GarbageCollected<T>` objects also apply to `GarbageCollectedFinalized<T>`.

### GarbageCollectedMixin

## Class properties

### EAGERLY_FINALIZE

### USING_GARBAGE_COLLECTED_MIXIN

### ALLOW_ONLY_INLINE_ALLOCATION

### STACK_ALLOCATED

## Handles

### Member

### WeakMember

### Persistent

### WeakPersistent

### CrossThreadPersistent

### CrossThreadWeakPersistent

## Tracing

## Heap collections
