# Open Screen Library Style Guide

The Open Screen Library follows the
[Chromium C++ coding style](https://chromium.googlesource.com/chromium/src/+/master/styleguide/c++/c++.md).
We also follow the
[Chromium C++ Do's and Don'ts](https://sites.google.com/a/chromium.org/dev/developers/coding-style/cpp-dos-and-donts).

C++14 language and library features are allowed in the Open Screen Library
according to the
[C++14 use in Chromium](https://chromium-cpp.appspot.com#core-whitelist) guidelines.

## Modifications to the Chromium C++ Guidelines

- `<functional>` and `std::function` objects are allowed.
- `<chrono>` is allowed and encouraged for representation of time.
- Abseil types are allowed based on the whitelist in [DEPS](https://chromium.googlesource.com/openscreen/+/refs/heads/master/DEPS).
- However, Abseil types **must not be used in public APIs**.
- `<thread>` and `<mutex>` are allowed, but discouraged from general use as the
  library only needs to handle threading in very specific places;
  see [threading.md](threading.md).

## Open Screen Library Features

- For public API functions that return values or errors, please return
  [`ErrorOr<T>`](https://chromium.googlesource.com/openscreen/+/master/platform/base/error.h).
- In the implementation of public APIs invoked by the embedder, use
  `OSP_DCHECK(TaskRunner::IsRunningOnTaskRunner())` to catch thread safety
  problems early.

## Code style

- Braces are optional for single-line if statements; follow the style of
  surrounding code.

## Copy and Move Operators

Use the following guidelines when deciding on copy and move semantics for
objects.

- Objects with data members greater than 32 bytes should be move-able.
- Known large objects (I/O buffers, etc.) should be be move-only.
- Variable length objects should be move-able
  (since they may be arbitrarily large in size) and, if possible, move-only.
- Inherently non-copyable objects (like sockets) should be move-only.

We [prefer the use of `default` and `delete`](https://sites.google.com/a/chromium.org/dev/developers/coding-style/cpp-dos-and-donts#TOC-Prefer-to-use-default)
to declare the copy and move semantics of objects.  See
[Stoustrop's C++ FAQ](http://www.stroustrup.com/C++11FAQ.html#default)
for details on how to do that.

### User Defined Copy and Move Operators

Classes should follow the [rule of
three/five/zero](https://en.cppreference.com/w/cpp/language/rule_of_three),
meaning that if it has a custom destructor, copy contructor, or copy
assignment operator:

- All three operators must be defined (and not defaulted).
- It must also either:
    - Have a custom move constructor *and* move assignment operator;
    - Delete both of them if move semantics are not desired (in rare cases).
- Polymorphic base classes with virtual destructors should declare all
  contructors, destructors and assignment operators as defaulted.

## Noexcept

We prefer to use `noexcept` on move constructors.  Although exceptions are not
allowed, this declaration [enables STL optimizations](https://en.cppreference.com/w/cpp/language/noexcept_spec).

Additionally, GCC requires that any type using a defaulted `noexcept` move
constructor/operator= has a `noexcept` copy or move constructor/operator= for
all of its members.

## Disallowed Styles and Features

Blink style is *not allowed* anywhere in the Open Screen Library.

C++17-only features are currently *not allowed* in the Open Screen Library.

GCC does not support designated initializers for non-trivial types.  This means
that the `.member = value` struct initialization syntax is not supported unless
all struct members are primitive types or structs of primitive types (i.e. no
unions, complex constructors, etc.).

## OSP_CHECK and OSP_DCHECK

These are provided in base/logging.h and act as run-time assertions (i.e., they
test an expression, and crash the program if it evaluates as false). They are
not only useful in determining correctness, but also serve as inline
documentation of the assumptions being made in the code. They should be used in
cases where they would fail only due to current or future coding errors.

These should *not* be used to sanitize non-const data, or data otherwise derived
from external inputs. Instead, one should code proper error-checking and
handling for such things.

OSP_CHECKs are "turned on" for all build types. However, OSP_DCHECKs are only
"turned on" in Debug builds, or in any build where the "dcheck_always_on=true"
GN argument is being used. In fact, at any time during development (including
Release builds), it is highly recommended to use "dcheck_always_on=true" to
catch bugs.

When OSP_DCHECKs are "turned off" they effectively become code comments: All
supported compilers will not generate any code, and they will automatically
strip-out unused functions and constants referenced in OSP_DCHECK expressions
(unless they are "extern" to the local module); and so there is absolutely no
run-time/space overhead when the program runs. For this reason, a developer need
not explicitly sprinkle "#if OSP_DCHECK_IS_ON()" guards all around any
functions, variables, etc. that will be unused in "DCHECK off" builds.
