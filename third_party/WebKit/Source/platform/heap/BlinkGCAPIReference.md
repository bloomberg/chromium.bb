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

If you want to make your class garbage-collected and the class needs finalization, your class needs to inherit from
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

Class templates in this section are smart pointers, each carrying a pointer to an on-heap object (think of `RefPtr<T>`
for `RefCounted<T>`). Collectively, they are called *handles*.

On-heap objects must be retained by any of these, depending on the situation.

### Raw pointers

On-stack references to on-heap objects must be raw pointers.

```c++
void someFunction()
{
    SomeGarbageCollectedClass* object = new SomeGarbageCollectedClass; // OK, retained by a pointer.
    ...
}
// OK to leave the object behind. The Blink GC system will free it up when it becomes unused.
```

*** aside
*Transitional only*

`RawPtr<T>` is a simple wrapper of a raw pointer `T*` equipped with common member functions defined in other smart
pointer templates, such as `get()` or `clear()`. `RawPtr<T>` is only meant to be used during the transition period;
it is only used in the forms like `OwnPtrWillBeRawPtr<T>` or `RefPtrWillBeRawPtr<T>` so you can share as much code
as possible in both pre- and post-Oilpan worlds.

`RawPtr<T>` is declared and defined in `wtf/RawPtr.h`.
***

### Member, WeakMember

In a garbage-collected class, on-heap objects must be retained by `Member<T>` or `WeakMember<T>`, depending on
the desired semantics.

`Member<T>` represents a *strong* reference to an object of type `T`, which means that the referred object is kept
alive as long as the owner class instance is alive. Unlike `RefPtr<T>`, it is okay to form a reference cycle with
members (in on-heap objects) and raw pointers (on stack).

`WeakMember<T>` is a *weak* reference to an object of type `T`. Unlike `Member<T>`, `WeakMember<T>` does not keep
the pointed object alive. The pointer in a `WeakMember<T>` can become `nullptr` when the object gets garbage-collected.
It may take some time for the pointer in a `WeakMember<T>` to become `nullptr` after the object actually goes unused,
because this rewrite is only done within Blink GC's garbage collection period.

```c++
class SomeGarbageCollectedClass : public GarbageCollected<GarbageCollectedSomething> {
    ...
private:
    Member<AnotherGarbageCollectedClass> m_another; // OK, retained by Member<T>.
    WeakMember<AnotherGarbageCollectedClass> m_anotherWeak; // OK, weak reference.
};
```

The use of `WeakMember<T>` incurs some overhead in garbage collector's performance. Use it sparingly. Usually, weak
members are not necessary at all, because reference cycles with members are allowed.

More specifically, `WeakMember<T>` should be used only if the owner of a weak member can outlive the pointed object.
Otherwise, `Member<T>` should be used.

You need to trace every `Member<T>` and `WeakMember<T>` in your class. See [Tracing](#Tracing).

### Persistent, WeakPersistent, CrossThreadPersistent, CrossThreadWeakPersistent

In a non-garbage-collected class, on-heap objects must be retained by `Persistent<T>`, `WeakPersistent<T>`,
`CrossThreadPersistent<T>`, or `CrossThreadWeakPersistent<T>`, depending on the situations and the desired semantics.

`Persistent<T>` is the most basic handle in the persistent family, which makes the referred object alive
unconditionally, as long as the persistent handle is alive.

`WeakPersistent<T>` does not make the referred object alive, and becomes `nullptr` when the object gets
garbage-collected, just like `WeakMember<T>`.

`CrossThreadPersistent<T>` and `CrossThreadWeakPersistent<T>` are cross-thread variants of `Persistent<T>` and
`WeakPersistent<T>`, respectively, which can point to an object in a different thread.

```c++
class NonGarbageCollectedClass {
    ...
private:
    Persistent<SomeGarbageCollectedClass> m_something; // OK, the object will be alive while this persistent is alive.
};
```

*** note
**Warning:** `Persistent<T>` and `CrossThreadPersistent<T>` are vulnerable to reference cycles. If a reference cycle
is formed with `Persistent`s, `Member`s, `RefPtr`s and `OwnPtr`s, all the objects in the cycle **will leak**, since
nobody in the cycle can be aware of whether they are ever referred from anyone.

When you are about to add a new persistent, be careful not to create a reference cycle. If a cycle is inevitable, make
sure the cycle is eventually cut by someone outside the cycle.
***

Persistents have small overhead in itself, because they need to maintain the list of all persistents. Therefore, it's
not a good idea to create or keep a lot of persistents at once.

Weak variants have overhead just like `WeakMember<T>`. Use them sparingly.

The need of cross-thread persistents may indicate a poor design in multi-thread object ownership. Think twice if they
are really necessary.

## Tracing

## Heap collections
