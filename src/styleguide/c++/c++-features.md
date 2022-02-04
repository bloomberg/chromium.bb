# Modern C++ use in Chromium

_This document is part of the more general
[Chromium C++ style guide](https://chromium.googlesource.com/chromium/src/+/main/styleguide/c++/c++.md).
It summarizes the supported state of new and updated language and library
features in recent C++ standards and the [Abseil](https://abseil.io/about/)
library. This guide applies to both Chromium and its subprojects, though
subprojects can choose to be more restrictive if necessary for toolchain
support._

The C++ language has in recent years received an updated standard every three
years (C++11, C++14, etc.). For various reasons, Chromium does not immediately
allow new features on the publication of such a standard. Instead, once
toolchain support is sufficient, a standard is declared "initially supported",
with new language/library features banned pending discussion.

You can propose changing the status of a feature by sending an email to
[cxx@chromium.org](https://groups.google.com/a/chromium.org/forum/#!forum/cxx).
Include a short blurb on what the feature is and why you think it should or
should not be allowed, along with links to any relevant previous discussion. If
the list arrives at some consensus, send a codereview to change this file
accordingly, linking to your discussion thread.

If an item remains on the TBD list two years after initial support is added,
style arbiters should explicitly move it to an appropriate allowlist or
blocklist, allowing it if there are no obvious reasons to ban.

The current status of existing standards and Abseil features is:

*   **C++11:** _Default allowed; see banned features below_
*   **C++14:** _Default allowed; see banned features below_
*   **C++17:** Initially supported December 23, 2021; see allowed/banned/TBD
    features below
*   **C++20:** _Not yet supported in Chromium_
*   **C++23:** _Not yet standardized_
*   **Abseil:** Initially supported July 31, 2020; see allowed/banned/TBD
    features below
    *   absl::StatusOr: Initially supported September 3, 2020
    *   absl::Cleanup: Initially supported February 4, 2021

[TOC]

## C++11 Banned Language Features {#core-blocklist-11}

The following C++11 language features are not allowed in the Chromium codebase.

### Inline Namespaces <sup>[banned]</sup>

```c++
inline namespace foo { ... }
```

**Description:** Allows better versioning of namespaces.

**Documentation:**
[Inline namespaces](https://en.cppreference.com/w/cpp/language/namespace#Inline_namespaces)

**Notes:**
*** promo
Banned in the
[Google Style Guide](https://google.github.io/styleguide/cppguide.html#Namespaces).
Unclear how it will work with components.
***

### long long Type <sup>[banned]</sup>

```c++
long long var = value;
```

**Description:** An integer of at least 64 bits.

**Documentation:**
[Fundamental types](https://en.cppreference.com/w/cpp/language/types)

**Notes:**
*** promo
Use a stdint.h type if you need a 64-bit number.
[Discussion thread](https://groups.google.com/a/chromium.org/forum/#!topic/chromium-dev/RxugZ-pIDxk)
***

### User-Defined Literals <sup>[banned]</sup>

```c++
type var = literal_value_type;
```

**Description:** Allows user-defined literal expressions.

**Documentation:**
[User-defined literals](https://en.cppreference.com/w/cpp/language/user_literal)

**Notes:**
*** promo
Banned in the
[Google Style Guide](https://google.github.io/styleguide/cppguide.html#Operator_Overloading).
***

### thread_local Storage Class <sup>[banned]</sup>

```c++
thread_local int foo = 1;
```

**Description:** Puts variables into thread local storage.

**Documentation:**
[Storage duration](https://en.cppreference.com/w/cpp/language/storage_duration)

**Notes:**
*** promo
Some surprising effects on Mac
([discussion](https://groups.google.com/a/chromium.org/forum/#!topic/chromium-dev/2msN8k3Xzgs),
[fork](https://groups.google.com/a/chromium.org/forum/#!topic/cxx/h7O5BdtWCZw)).
Use `base::SequenceLocalStorageSlot` for sequence support, and
`base::ThreadLocal`/`base::ThreadLocalStorage` otherwise.
***

## C++11 Banned Library Features {#library-blocklist-11}

The following C++11 library features are not allowed in the Chromium codebase.

### Bind Operations <sup>[banned]</sup>

```c++
std::bind(function, args, ...)
```

**Description:** Declares a function object bound to certain arguments

**Documentation:**
[std::bind](https://en.cppreference.com/w/cpp/utility/functional/bind)

**Notes:**
*** promo
Use `base::Bind` instead. Compared to `std::bind`, `base::Bind` helps prevent
lifetime issues by preventing binding of capturing lambdas and by forcing
callers to declare raw pointers as `Unretained`.
[Discussion thread](https://groups.google.com/a/chromium.org/forum/#!topic/cxx/SoEj7oIDNuA)
***

### C Floating-Point Environment <sup>[banned]</sup>

```c++
#include <cfenv>
#include <fenv.h>
```

**Description:** Provides floating point status flags and control modes for
C-compatible code

**Documentation:**
[Standard library header "cfenv"](https://en.cppreference.com/w/cpp/header/cfenv)

**Notes:**
*** promo
Banned by the
[Google Style Guide](https://google.github.io/styleguide/cppguide.html#C++11)
due to concerns about compiler support.
***

### Date and time utilities <sup>[banned]</sup>

```c++
#include <chrono>
```

**Description:** A standard date and time library

**Documentation:**
[Date and time utilities](https://en.cppreference.com/w/cpp/chrono)

**Notes:**
*** promo
Overlaps with `Time` APIs in `base/`. Keep using the `base/` classes.
***

### Exceptions <sup>[banned]</sup>

```c++
#include <exception>
```

**Description:** Enhancements to exception throwing and handling

**Documentation:**
[Standard library header "exception"](https://en.cppreference.com/w/cpp/header/exception)

**Notes:**
*** promo
Exceptions are banned by the
[Google Style Guide](https://google.github.io/styleguide/cppguide.html#Exceptions)
and disabled in Chromium compiles. However, the `noexcept` specifier is
explicitly allowed.
[Discussion thread](https://groups.google.com/a/chromium.org/forum/#!topic/chromium-dev/8i4tMqNpHhg)
***

### Function Objects <sup>[banned]</sup>

```c++
std::function
```

**Description:** Wraps a standard polymorphic function

**Documentation:**
[std::function](https://en.cppreference.com/w/cpp/utility/functional/function)

**Notes:**
*** promo
Use `base::{Once,Repeating}Callback` instead. Compared to `std::function`,
`base::{Once,Repeating}Callback` directly supports Chromium's refcounting
classes and weak pointers and deals with additional thread safety concerns.
[Discussion thread](https://groups.google.com/a/chromium.org/forum/#!topic/cxx/SoEj7oIDNuA)
***

### Random Number Engines <sup>[banned]</sup>

*** aside
The random number engines defined in `<random>` (see separate item for random
number distributions), e.g.: `linear_congruential_engine`,
`mersenne_twister_engine`, `minstd_rand0`, `mt19937`, `ranlinux48`,
`random_device`
***

**Description:** Random number generation algorithms and utilities.

**Documentation:**
[Pseudo-random number generation](https://en.cppreference.com/w/cpp/numeric/random)

**Notes:**
*** promo
Do not use any random number engines from `<random>`. Instead, use
`base::RandomBitGenerator`.
[Discussion thread](https://groups.google.com/a/chromium.org/forum/#!topic/cxx/16Xmw05C-Y0)
***

### Ratio Template Class <sup>[banned]</sup>

```c++
std::ratio<numerator, denominator>
```

**Description:** Provides compile-time rational numbers

**Documentation:**
[std::ratio](https://en.cppreference.com/w/cpp/numeric/ratio/ratio)

**Notes:**
*** promo
Banned by the
[Google Style Guide](https://google.github.io/styleguide/cppguide.html#C++11)
due to concerns that this is tied to a more template-heavy interface style.
***

### Regular Expressions <sup>[banned]</sup>

```c++
#include <regex>
```

**Description:** A standard regular expressions library

**Documentation:**
[Regular expressions library](https://en.cppreference.com/w/cpp/regex)

**Notes:**
*** promo
Overlaps with many regular expression libraries in Chromium. When in doubt, use
`re2`.
***

### Shared Pointers <sup>[banned]</sup>

```c++
std::shared_ptr
```

**Description:** Allows shared ownership of a pointer through reference counts

**Documentation:**
[std::shared_ptr](https://en.cppreference.com/w/cpp/memory/shared_ptr)

**Notes:**
*** promo
Needs a lot more evaluation for Chromium, and there isn't enough of a push for
this feature.

*   [Google Style Guide](https://google.github.io/styleguide/cppguide.html#Ownership_and_Smart_Pointers).
*   [Discussion Thread](https://groups.google.com/a/chromium.org/forum/#!topic/cxx/aT2wsBLKvzI)
***

### String-Number Conversion Functions <sup>[banned]</sup>

```c++
std::stoi()
std::stol()
std::stoul()
std::stoll
std::stoull()
std::stof()
std::stod()
std::stold()
std::to_string()
```

**Description:** Converts strings to/from numbers

**Documentation:**
*   [std::stoi, std::stol, std::stoll](https://en.cppreference.com/w/cpp/string/basic_string/stol),
*   [std::stoul, std::stoull](https://en.cppreference.com/w/cpp/string/basic_string/stoul),
*   [std::stof, std::stod, std::stold](https://en.cppreference.com/w/cpp/string/basic_string/stof),
*   [std::to_string](https://en.cppreference.com/w/cpp/string/basic_string/to_string)

**Notes:**
*** promo
The string-to-number conversions rely on exceptions to communicate failure,
while the number-to-string conversions have performance concerns and depend on
the locale. Use the routines in `base/strings/string_number_conversions.h`
instead.
***

### Thread Library <sup>[banned]</sup>

*** aside
`<thread>` and related headers, including `<future>`, `<mutex>`,
`<condition_variable>`
***

**Description:** Provides a standard multithreading library using `std::thread`
and associates

**Documentation:**
[Thread support library](https://en.cppreference.com/w/cpp/thread)

**Notes:**
*** promo
Overlaps with many classes in `base/`. Keep using the `base/` classes for now.
`base::Thread` is tightly coupled to `MessageLoop` which would make it hard to
replace. We should investigate using standard mutexes, or unique_lock, etc. to
replace our locking/synchronization classes.
***

### Weak Pointers <sup>[banned]</sup>

```c++
std::weak_ptr
```

**Description:** Allows a weak reference to a `std::shared_ptr`

**Documentation:**
[std::weak_ptr](https://en.cppreference.com/w/cpp/memory/weak_ptr)

**Notes:**
*** promo
Banned because `std::shared_ptr` is banned.  Use `base::WeakPtr` instead.
***

## C++14 Banned Library Features {#library-blocklist-14}

The following C++14 library features are not allowed in the Chromium codebase.

### std::chrono literals <sup>[banned]</sup>

```c++
using namespace std::chrono_literals;
auto timeout = 30s;
```

**Description:** Allows `std::chrono` types to be more easily constructed.

**Documentation:**
[std::literals::chrono_literals::operator""s](https://en.cppreference.com/w/cpp/chrono/operator%22%22s)

**Notes:**
*** promo
Banned because `<chrono>` is banned.
***

## C++17 Allowed Language Features {#core-allowlist-17}

The following C++17 language features are allowed in the Chromium codebase.

### Nested namespaces <sup>[allowed]</sup>

```c++
namespace A::B::C { ...
```

**Description:** Using the namespace resolution operator to create nested
namespace definitions.

**Documentation:**
[Namespaces](https://en.cppreference.com/w/cpp/language/namespace)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/gLdR3apDSmg/)
***

### Template argument deduction for class templates <sup>[allowed]</sup>

```c++
template <typename T>
struct MyContainer {
  MyContainer(T val) : val{val} {}
  // ...
};
MyContainer c1(1);  // Type deduced to be `int`.
```

**Description:** Automatic template argument deduction much like how it's done
for functions, but now including class constructors.

**Documentation:**
[Class template argument deduction](https://en.cppreference.com/w/cpp/language/class_template_argument_deduction)

**Notes:**
*** promo
Usage is governed by the
[Google Style Guide](https://google.github.io/styleguide/cppguide.html#CTAD).
***

### Selection statements with initializer <sup>[allowed]</sup>

```c++
if (int a = Func(); a < 3) { ...
switch (int a = Func(); a) { ...
```

**Description:** New versions of the if and switch statements which simplify
common code patterns and help users keep scopes tight.

**Documentation:**
[if statement](https://en.cppreference.com/w/cpp/language/if),
[switch statement](https://en.cppreference.com/w/cpp/language/switch)

**Notes:**
*** promo
[@cxx discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/4GP43nftePE)
***

### fallthrough attribute <sup>[allowed]</sup>

```c++
case 1:
  DoSomething();
  [[fallthrough]];
case 2:
  break;
```

**Description:**
The `[[fallthrough]]` attribute can be used in switch statements to indicate
when intentionally falling through to the next case.

**Documentation:**
[C++ attribute: fallthrough](https://en.cppreference.com/w/cpp/language/attributes/fallthrough)

**Notes:**
*** promo
See [discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/JrvyFd243QI).

See [migration task](https://bugs.chromium.org/p/chromium/issues/detail?id=1283907).
***

### constexpr if <sup>[allowed]</sup>

```c++
if constexpr (cond) { ...
```

**Description:** Write code that is instantiated depending on a compile-time
condition.

**Documentation:**
[if statement](https://en.cppreference.com/w/cpp/language/if)

**Notes:**
*** promo
See [discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/op2ePZnjP0w).
***

### nodiscard attribute <sup>[allowed]</sup>

```c++
struct [[nodiscard]] ErrorOrValue;
[[nodiscard]] bool DoSomething();
```

**Description:**
The `[[nodiscard]]` attribute can be used to indicate that

  - the return value of a function should not be ignored
  - values of annotated classes/structs/enums returned from functions should not
    be ignored

**Documentation:**
[C++ attribute: nodiscard](https://en.cppreference.com/w/cpp/language/attributes/nodiscard)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/nH7Ar8pZ1Dw/m/c90vGChvAAAJ)
***

### maybe_unused attribute <sup>[allowed]</sup>

```c++
struct [[maybe_unused]] MyUnusedThing;
[[maybe_unused]] int x;
```

**Description:**
The `[[maybe_unused]]` attribute can be used to indicate that individual
variables, functions, or fields of a class/struct/enum can be left unused.

**Documentation:**
[C++ attribute: maybe_unused](https://en.cppreference.com/w/cpp/language/attributes/maybe_unused)

**Notes:**
*** promo
See [discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/jPLfU5eRg8M/).
***

### Structured bindings <sup>[allowed]</sup>

```c++
const auto [x, y] = FuncReturningStdPair();
```

**Description:** Allows writing `auto [x, y, z] = expr;` where the type of
`expr` is a tuple-like object, whose elements are bound to the variables `x`,
`y`, and `z` (which this construct declares). Tuple-like objects include
`std::tuple`, `std::pair`, `std::array`, and aggregate structures.

**Documentation:**
[Structured binding declaration](https://en.cppreference.com/w/cpp/language/structured_binding)
[Explanation of structured binding types](https://jguegant.github.io/blogs/tech/structured-bindings.html)

**Notes:**
*** promo
In C++17, structured bindings don't work with lambda captures.
[C++20 will allow capturing structured bindings by value](https://wg21.link/p1091r3).

This feature forces omitting type names. Its use should follow
[the guidance around `auto` in Google C++ Style guide](https://google.github.io/styleguide/cppguide.html#Type_deduction).

See [discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/ExfSorNLNf4).
***

### Inline variables

```c++
struct S {
  static constexpr int kZero = 0;  // constexpr implies inline here.
};

constexpr inline int kOne = 1;  // Explicit inline needed here.
```

**Description:** The `inline` specifier can be applied to variables as well as
to functions. A variable declared inline has the same semantics as a function
declared inline. It can also be used to declare and define a static member
variable, such that it does not need to be initialized in the source file.

**Documentation:**
[inline specifier](https://en.cppreference.com/w/cpp/language/inline)

**Notes:**
*** promo
Inline variables in anonymous namespaces in header files will still get one copy
per translation unit, so they must be outside of an anonymous namespace to be
effective.

Mutable inline variables and taking the address of inline variables are banned
since these will break the component build.

See [discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/hmyGFD80ocE/m/O4AXC93vAQAJ).
***

## C++17 Allowed Library Features {#library-allowlist-17}

The following C++17 language features are allowed in the Chromium codebase.

### Allocation functions with explicit alignment <sup>[allowed]</sup>

```c++
class alignas(32) Vec3d {
  double x, y, z;
};
auto p_vec = new Vec3d[10];  // 32-byte aligned in C++17, maybe not previously
```

**Description:** Performs heap allocation of objects whose alignment
requirements exceed `__STDCPP_DEFAULT_NEW_ALIGNMENT__`.

**Documentation:**
[operator new](https://en.cppreference.com/w/cpp/memory/new/operator_new)

**Notes:**
*** promo
None
***

### Type trait variable templates <sup>[tbd]</sup>

```c++
bool b = std::is_same_v<int, std::int32_t>;
```

**Description:** Syntactic sugar to provide convenient access to `::value`
members by simply adding `_v`.

**Documentation:**
[Type support](https://en.cppreference.com/w/cpp/types)

**Notes:**
*** promo
[Discussion thread](Non://groups.google.com/a/chromium.org/g/cxx/c/KEa-0AOGRNY/m/IV_S3_pvAAAJ)
***

### std::map::try_emplace <sup>[allowed]</sup>

```c++
std::map<std::string, std::string> m;
m.try_emplace("c", 10, 'c');
m.try_emplace("c", "Won't be inserted");
```

**Description:** Like `emplace`, but does not move from rvalue arguments if the
insertion does not happen.

**Documentation:**
[std::map::try_emplace](https://en.cppreference.com/w/cpp/container/map/try_emplace),

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/Uv2tUfIwUfQ/m/ffMxCk9uAAAJ)
***

### std::map::insert_or_assign <sup>[allowed]</sup>

```c++
std::map<std::string, std::string> m;
m.insert_or_assign("c", "cherry");
m.insert_or_assign("c", "clementine");
```

**Description:** Like `operator[]`, but returns more information and does not
require default-constructibility of the mapped type.

**Documentation:**
[std::map::insert_or_assign](https://en.cppreference.com/w/cpp/container/map/insert_or_assign)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/Uv2tUfIwUfQ/m/ffMxCk9uAAAJ)
***

## C++17 Banned Library Features {#library-blocklist-17}

The following C++17 library features are not allowed in the Chromium codebase.

### std::any <sup>[banned]</sup>

```c++
std::any x = 5;
```

**Description:** A type-safe container for single values of any type.

**Documentation:**
[std::any](https://en.cppreference.com/w/cpp/utility/any)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/KEa-0AOGRNY/m/IV_S3_pvAAAJ)

Banned since workaround for lack of RTTI isn't compatible with the component
build ([Bug](https://crbug.com/1096380)). Also see `absl::any`.
***

### std::filesystem <sup>[banned]</sup>

```c++
#include <filesystem>
```

**Description:** A standard way to manipulate files, directories, and paths in a
filesystem.

**Documentation:**
[Filesystem library](https://en.cppreference.com/w/cpp/filesystem)

**Notes:**
*** promo
Banned by the [Google Style Guide](https://google.github.io/styleguide/cppguide.html#Other_Features).
***

### weak_from_this <sup>[banned]</sup>

```c++
auto weak_ptr = weak_from_this();
```

**Description:** Returns a `std::weak_ptr<T>` that tracks ownership of `*this`
by all existing `std::shared_ptr`s that refer to `*this`.

**Documentation:**
[std::enable_shared_from_this<T>::weak_from_this](https://en.cppreference.com/w/cpp/memory/enable_shared_from_this/weak_from_this)

**Notes:**
*** promo
Banned since `std::shared_ptr` and `std::weak_ptr` are banned.
***

### Transparent std::owner_less <sup>[banned]</sup>

```c++
std::map<std::weak_ptr<T>, U, std::owner_less<>>
```

**Description:** Function object providing mixed-type owner-based ordering of
shared and weak pointers, regardless of the type of the pointee.

**Documentation:**
[std::owner_less](https://en.cppreference.com/w/cpp/memory/owner_less)

**Notes:**
*** promo
Banned since `std::shared_ptr` and `std::weak_ptr` are banned.
***

### Array support for std::shared_ptr <sup>[banned]</sup>

```c++
std::shared_ptr<int[]> p(new int[10]{0,1,2,3,4,5,6,7,8,9});
std::cout << p[3];  // "3"
```

**Description:** Supports memory management of arrays via `std::shared_ptr`.

**Documentation:**
[std::shared_ptr](https://en.cppreference.com/w/cpp/memory/shared_ptr)

**Notes:**
*** promo
Banned since `std::shared_ptr` is banned.
***

### std::uncaught_exceptions <sup>[banned]</sup>

```c++
int count = std::uncaught_exceptions();
```

**Description:** Determines whether there are live exception objects.

**Documentation:**
[std::uncaught_exceptions](https://en.cppreference.com/w/cpp/error/uncaught_exception)

**Notes:**
*** promo
Banned because exceptions are banned.
***

### Rounding functions for duration and time_point <sup>[banned]</sup>

```c++
std::chrono::ceil<std::chrono::seconds>(dur);
std::chrono::ceil<std::chrono::seconds>(time_pt);
std::chrono::floor<std::chrono::seconds>(dur);
std::chrono::floor<std::chrono::seconds>(time_pt);
std::chrono::round<std::chrono::seconds>(dur);
std::chrono::round<std::chrono::seconds>(time_pt);
```

**Description:** Converts durations and time_points by rounding.

**Documentation:**
[std::chrono::duration](https://en.cppreference.com/w/cpp/chrono/duration),
[std::chrono::time_point](https://en.cppreference.com/w/cpp/chrono/time_point)

**Notes:**
*** promo
Banned since `std::chrono` is banned.
***

## C++17 TBD Language Features {#core-review-17}

The following C++17 language features are not allowed in the Chromium codebase.
See the top of this page on how to propose moving a feature from this list into
the allowed or banned sections.

### Declaring non-type template parameters with auto <sup>[tbd]</sup>

```c++
template <auto... seq>
struct my_integer_sequence {
  // ...
};
auto seq = my_integer_sequence<0, 1, 2>();  // Type deduced to be `int`.
```

**Description:** Following the deduction rules of `auto`, while respecting the
non-type template parameter list of allowable types, template arguments can be
deduced from the types of its arguments.

**Documentation:**
[Template parameters](https://en.cppreference.com/w/cpp/language/template_parameters)

**Notes:**
*** promo
None
***

### Fold expressions <sup>[tbd]</sup>

```c++
template <typename... Args>
auto sum(Args... args) {
  return (... + args);
}
```

**Description:** A fold expression performs a fold of a template parameter pack
over a binary operator.

**Documentation:**
[Fold expression](https://en.cppreference.com/w/cpp/language/fold)

**Notes:**
*** promo
None
***

### constexpr lambda <sup>[tbd]</sup>

```c++
auto identity = [](int n) constexpr { return n; };
static_assert(identity(123) == 123);
```

**Description:** Compile-time lambdas using constexpr.

**Documentation:**
[Lambda expressions](https://en.cppreference.com/w/cpp/language/lambda)

**Notes:**
*** promo
None
***

### Lambda capture this by value <sup>[tbd]</sup>

```c++
const auto l = [*this] { return member_; }
```

**Description:** `*this` captures the current object by copy, while `this`
continues to capture by reference.

**Documentation:**
[Lambda capture](https://en.cppreference.com/w/cpp/language/lambda#Lambda_capture)

**Notes:**
*** promo
None
***

### UTF-8 character literals <sup>[tbd]</sup>

```c++
char x = u8'x';
```

**Description:** A character literal that begins with `u8` is a character
literal of type `char`. The value of a UTF-8 character literal is equal to its
ISO 10646 code point value.

**Documentation:**
[Character literal](https://en.cppreference.com/w/cpp/language/character_literal)

**Notes:**
*** promo
C++20 changes the type to `char8_t`, causing migration hazards for code using
this.
***

### using declaration for attributes <sup>[tbd]</sup>

```c++
[[using CC: opt(1), debug]]  // same as [[CC:opt(1), CC::debug]]
```

**Description:** Specifies a common namespace for a list of attributes.

**Documentation:**
[Attribute specifier sequence](https://en.cppreference.com/w/cpp/language/attributes)

**Notes:**
*** promo
See similar attribute macros in base/compiler_specific.h.
***

### __has_include <sup>[tbd]</sup>

```c++
#if __has_include(<optional>) ...
```

**Description:** Checks whether a file is available for inclusion, i.e. the file
exists.

**Documentation:**
[Source file inclusion](https://en.cppreference.com/w/cpp/preprocessor/include)

**Notes:**
*** promo
None
***

## C++17 TBD Library Features {#library-review-17}

The following C++17 library features are not allowed in the Chromium codebase.
See the top of this page on how to propose moving a feature from this list into
the allowed or banned sections.

### std::variant <sup>[tbd]</sup>

```c++
std::variant<int, double> v = 12;
```

**Description:** The class template `std::variant` represents a type-safe
`union`. An instance of `std::variant` at any given time holds a value of one of
its alternative types (it's also possible for it to be valueless).

**Documentation:**
[std::variant](https://en.cppreference.com/w/cpp/utility/variant)

**Notes:**
*** promo
See also `absl::variant`.
***

### std::optional <sup>[tbd]</sup>

```c++
std::optional<std::string> s;
```

**Description:** The class template `std::optional` manages an optional
contained value, i.e. a value that may or may not be present. A common use case
for optional is the return value of a function that may fail.

**Documentation:**
[std::optional](https://en.cppreference.com/w/cpp/utility/optional)

**Notes:**
*** promo
See also `absl::optional`.
***

### std::string_view <sup>[tbd]</sup>

```c++
std::string_view str = "foo";
```

**Description:** A non-owning reference to a string. Useful for providing an
abstraction on top of strings (e.g. for parsing).

**Documentation:**
[std::basic_string_view](https://en.cppreference.com/w/cpp/string/basic_string_view)

**Notes:**
*** promo
See also `absl::string_view` and `base::StringPiece`.
***

### std::invoke <sup>[tbd]</sup>

```c++
static_assert(std::invoke(std::plus<>(), 1, 2) == 3);
```

**Description:** Invokes a `Callable` object with parameters. An example of a
`Callable` object is `base::Callback` where an object can be called similarly to
a regular function.

**Documentation:**
[std::invoke](https://en.cppreference.com/w/cpp/utility/functional/invoke)

**Notes:**
*** promo
See also `base::invoke`.
***

### std::apply <sup>[tbd]</sup>

```c++
static_assert(std::apply(std::plus<>(), std::make_tuple(1, 2)) == 3);
```

**Description:** Invokes a `Callable` object with a tuple of arguments.

**Documentation:**
[std::apply](https://en.cppreference.com/w/cpp/utility/apply)

**Notes:**
*** promo
See also `absl::apply` and `base::apply`.
***

### std::byte <sup>[tbd]</sup>

```c++
std::byte b = 0xFF;
int i = std::to_integer<int>(b);  // 0xFF
```

**Description:** A standard way of representing data as a byte. `std::byte` is
neither a character type nor an arithmetic type, and the only operator overloads
available are bitwise operations.

**Documentation:**
[std::byte](https://en.cppreference.com/w/cpp/types/byte)

**Notes:**
*** promo
None
***

### Splicing for maps and sets <sup>[tbd]</sup>

```c++
std::map<...>::extract
std::map<...>::merge
std::set<...>::extract
std::set<...>::merge
```

**Description:** Moving nodes and merging containers without the overhead of
expensive copies, moves, or heap allocations/deallocations.

**Documentation:**
[std::map::extract](https://en.cppreference.com/w/cpp/container/map/extract),
[std::map::merge](https://en.cppreference.com/w/cpp/container/map/merge)

**Notes:**
*** promo
None
***

### Parallel algorithms <sup>[tbd]</sup>

```c++
auto it = std::find(std::execution::par, std::begin(vec), std::end(vec), 2);
```

**Description:** Many of the STL algorithms, such as the `copy`, `find` and
`sort` methods, now support the parallel execution policies: `seq`, `par`, and
`par_unseq` which translate to "sequentially", "parallel" and
"parallel unsequenced".

**Documentation:**
[execution_policy_tag_t](https://en.cppreference.com/w/cpp/algorithm/execution_policy_tag_t)

**Notes:**
*** promo
None
***

### std::make_from_tuple <sup>[tbd]</sup>

```c++
// Calls Foo(int, double):
auto foo = std::make_from_tuple<Foo>(std::make_tuple(1, 3.5));
```

**Description:** Constructs an object from a tuple of arguments.

**Documentation:**
[std::make_from_tuple](https://en.cppreference.com/w/cpp/utility/make_from_tuple)

**Notes:**
*** promo
See also `absl::make_from_tuple`.
***

### Searchers <sup>[tbd]</sup>

```c++
auto it = std::search(haystack.begin(), haystack.end(),
                      std::boyer_moore_searcher(needle.begin(), needle.end()));
```

**Description:** Alternate string searching algorithms.

**Documentation:**
[Searchers](https://en.cppreference.com/w/cpp/utility/functional#Searchers)

**Notes:**
*** promo
None
***

### std::as_const <sup>[tbd]</sup>

```c++
auto&& const_ref = std::as_const(mutable_obj);
```

**Description:** Forms reference to const T.

**Documentation:**
[std::as_const](https://en.cppreference.com/w/cpp/utility/as_const)

**Notes:**
*** promo
See also `base::as_const`.
***

### std::not_fn <sup>[tbd]</sup>

```c++
auto nonwhite = std::find_if(str.begin(), str.end(), std::not_fn(IsWhitespace));
```

**Description:** Creates a forwarding call wrapper that returns the negation of
the callable object it holds.

**Documentation:**
[std::not_fn](https://en.cppreference.com/w/cpp/utility/functional/not_fn)

**Notes:**
*** promo
See also `base::not_fn`.
***

### Uninitialized memory algorithms <sup>[tbd]</sup>

```c++
std::destroy_at(ptr);
std::destroy(ptr, ptr + 8);
std::destroy_n(ptr, 8);
std::uninitialized_move(src.begin(), src.end(), dest.begin());
std::uninitialized_value_construct(std::begin(storage), std::end(storage));
```

**Description:** Replaces direct constructor and destructor calls when manually
managing memory.

**Documentation:**
[std::destroy_at](https://en.cppreference.com/w/cpp/memory/destroy_at),
[std::destroy](https://en.cppreference.com/w/cpp/memory/destroy),
[std::destroy_n](https://en.cppreference.com/w/cpp/memory/destroy_n),
[std::uninitialized_move](https://en.cppreference.com/w/cpp/memory/uninitialized_move),
[std::uninitialized_value_construct](https://en.cppreference.com/w/cpp/memory/uninitialized_value_construct)

**Notes:**
*** promo
None
***

### std::pmr::memory_resource and std::polymorphic_allocator <sup>[tbd]</sup>

```c++
#include <memory_resource>
```

**Description:** Manages memory allocations using runtime polymorphism.

**Documentation:**
[std::pmr::memory_resource](https://en.cppreference.com/w/cpp/memory/memory_resource),
[std::pmr::polymorphic_allocator](https://en.cppreference.com/w/cpp/memory/polymorphic_allocator),

**Notes:**
*** promo
May not be supported in libc++, according to the
[library features table](https://en.cppreference.com/w/cpp/17)
***

### std::aligned_alloc <sup>[tbd]</sup>

```c++
int* p2 = static_cast<int*>(std::aligned_alloc(1024, 1024));
```

**Description:** Allocates uninitialized storage with the specified alignment.

**Documentation:**
[std::aligned_alloc](https://en.cppreference.com/w/cpp/memory/c/aligned_alloc)

**Notes:**
*** promo
None
***

### std::conjunction/std::disjunction/std::negation <sup>[tbd]</sup>

```c++
template<typename T, typename... Ts>
std::enable_if_t<std::conjunction_v<std::is_same<T, Ts>...>>
func(T, Ts...) { ...
```

**Description:** Performs logical operations on type traits.

**Documentation:**
[std::conjunction](https://en.cppreference.com/w/cpp/types/conjunction),
[std::disjunction](https://en.cppreference.com/w/cpp/types/disjunction),
[std::negation](https://en.cppreference.com/w/cpp/types/negation)

**Notes:**
*** promo
None
***

### std::is_swappable <sup>[tbd]</sup>

```c++
std::is_swappable<T>
std::is_swappable_with_v<T, U>
```

**Description:** Checks whether classes may be swapped.

**Documentation:**
[std::is_swappable](https://en.cppreference.com/w/cpp/types/is_swappable)

**Notes:**
*** promo
None
***

### std::is_invocable <sup>[tbd]</sup>

```c++
std::is_invocable_v<Fn, 1, "Hello">
```

**Description:** Checks whether a function may be invoked with the given
argument types.  The `_r` variant also evaluates whether the result is
convertible to a given type.

**Documentation:**
[std::is_invocable](https://en.cppreference.com/w/cpp/types/is_invocable)

**Notes:**
*** promo
None
***

### std::is_aggregate <sup>[tbd]</sup>

```c++
if constexpr(std::is_aggregate_v<T>) { ...
```

**Description:** Checks wither the given type is an aggregate type.

**Documentation:**
[std::is_aggregate](https://en.cppreference.com/w/cpp/types/is_aggregate)

**Notes:**
*** promo
None
***

### std::has_unique_object_representations <sup>[tbd]</sup>

```c++
std::has_unique_object_representations_v<foo>
```

**Description:** Checks wither the given type is trivially copyable and any two
objects with the same value have the same object representation.

**Documentation:**
[std::has_unique_object_representations](https://en.cppreference.com/w/cpp/types/has_unique_object_representations)

**Notes:**
*** promo
None
***

### std::clamp <sup>[tbd]</sup>

```c++
int x = base::clamp(inp, 0, 100);
```

**Description:** Clamps a value between a minimum and a maximum.

**Documentation:**
[std::clamp](https://en.cppreference.com/w/cpp/algorithm/clamp)

**Notes:**
*** promo
See also `base::clamp`.
***

### std::reduce <sup>[tbd]</sup>

```c++
std::reduce(std::execution::par, v.cbegin(), v.cend());
```

**Description:** Like `std::accumulate` except the elements of the range may be
grouped and rearranged in arbitrary order.

**Documentation:**
[std::reduce](https://en.cppreference.com/w/cpp/algorithm/reduce)

**Notes:**
*** promo
Makes the most sense in conjunction with `std::execution::par`.
***

### std::inclusive_scan <sup>[tbd]</sup>

```c++
std::inclusive_scan(data.begin(), data.end(), output.begin());
```

**Description:** Like `std::accumulate` but writes the result at each step into
the output range.

**Documentation:**
[std::inclusive_scan](https://en.cppreference.com/w/cpp/algorithm/inclusive_scan)

**Notes:**
*** promo
None
***

### std::exclusive_scan <sup>[tbd]</sup>

```c++
std::exclusive_scan(data.begin(), data.end(), output.begin());
```

**Description:** Like `std::inclusive_scan` but omits the current element from
the written output at each step; that is, results are "one value behind" those
of `std::inclusive_scan`.

**Documentation:**
[std::exclusive_scan](https://en.cppreference.com/w/cpp/algorithm/exclusive_scan)

**Notes:**
*** promo
None
***

### std::gcd <sup>[tbd]</sup>

```c++
static_assert(std::gcd(12, 18) == 6);
```

**Description:** Computes the greatest common divisor of its arguments.

**Documentation:**
[std::gcd](https://en.cppreference.com/w/cpp/numeric/gcd)

**Notes:**
*** promo
None
***

### std::lcm <sup>[tbd]</sup>

```c++
static_assert(std::lcm(12, 18) == 36);
```

**Description:** Computes the least common multiple of its arguments.

**Documentation:**
[std::lcm](https://en.cppreference.com/w/cpp/numeric/lcm)

**Notes:**
*** promo
None
***

### Non-member std::size/std::empty/std::data <sup>[tbd]</sup>

```c++
for (std::size_t i = 0; i < std::size(c); ++i) { ...
if (!std::empty(c)) { ...
std::strcpy(arr, std::data(str));
```

**Description:** Non-member versions of what are normally member functions, for
symmetrical use with things like arrays and initializer_lists.

**Documentation:**
[std::size](https://en.cppreference.com/w/cpp/iterator/size),
[std::empty](https://en.cppreference.com/w/cpp/iterator/empty),
[std::data](https://en.cppreference.com/w/cpp/iterator/data)

**Notes:**
*** promo
See `base::size`, `base::empty`, and `base::data`.
***

### Mathematical special functions <sup>[tbd]</sup>

```c++
std::assoc_laguerre()
std::assoc_legendre()
std::beta()
std::comp_ellint_1()
std::comp_ellint_2()
std::comp_ellint_3()
std::cyl_bessel_i()
std::cyl_bessel_j()
std::cyl_bessel_k()
std::cyl_neumann()
std::ellint_1()
std::ellint_2()
std::ellint_3()
std::expint()
std::hermite()
std::legendre()
std::laguerre()
std::riemann_zeta()
std::sph_bessel()
std::sph_legendre()
std::sph_neumann()
```

**Description:** A variety of mathematical functions.

**Documentation:**
[Mathematical special functions](https://en.cppreference.com/w/cpp/numeric/special_functions)

**Notes:**
*** promo
May not be supported in libc++, according to the
[library features table](https://en.cppreference.com/w/cpp/17)
***

### 3D std::hypot <sup>[tbd]</sup>

```c++
double dist = std::hypot(1.0, 2.5, 3.7);
```

**Description:** Computes the distance from the origin in 3D space.

**Documentation:**
[std::hypot](https://en.cppreference.com/w/cpp/numeric/math/hypot)

**Notes:**
*** promo
None
***

### Cache line interface <sup>[tbd]</sup>

```c++
alignas(std::hardware_destructive_interference_size) std::atomic<int> cat;
static_assert(sizeof(S) <= std::hardware_constructive_interference_size);
```

**Description:** A portable way to access the L1 data cache line size.

**Documentation:**
[Hardware interference size](https://en.cppreference.com/w/cpp/thread/hardware_destructive_interference_size)

**Notes:**
*** promo
May not be supported in libc++, according to the
[library features table](https://en.cppreference.com/w/cpp/17)
***

### std::launder <sup>[tbd]</sup>

```c++
struct Y { int z; };
alignas(Y) std::byte s[sizeof(Y)];
Y* q = new(&s) Y{2};
const int h = std::launder(reinterpret_cast<Y*>(&s))->z;
```

**Description:** When used to wrap a pointer, makes it valid to access the
resulting object in cases it otherwise wouldn't have been, in a very limited set
of circumstances.

**Documentation:**
[std::launder](https://en.cppreference.com/w/cpp/utility/launder)

**Notes:**
*** promo
None
***

### std::to_chars/std::from_chars <sup>[tbd]</sup>

```c++
std::to_chars(str.data(), str.data() + str.size(), 42);
std::from_chars(str.data(), str.data() + str.size(), result);
```

**Description:** Locale-independent, non-allocating, non-throwing functions to
convert values to/from character strings, designed for use in high-throughput
contexts.

**Documentation:**
[std::to_chars](https://en.cppreference.com/w/cpp/utility/to_chars),
[std::from_chars](https://en.cppreference.com/w/cpp/utility/from_chars)

**Notes:**
*** promo
None
***

### std::atomic<T>::is_always_lock_free <sup>[tbd]</sup>

```c++
template <typename T>
struct is_lock_free_impl
: std::integral_constant<bool, std::atomic<T>::is_always_lock_free> {};
```

**Description:** True when the given atomic type is always lock-free.

**Documentation:**
[std::atomic<T>::is_always_lock_free](https://en.cppreference.com/w/cpp/atomic/atomic/is_always_lock_free)

**Notes:**
*** promo
None
***

### std::scoped_lock <sup>[tbd]</sup>

```c++
std::scoped_lock lock(e1.m, e2.m);
```

**Description:** Provides an RAII-style mechanism for owning one or more mutexes
for the duration of a scoped block.

**Documentation:**
[std::scoped_lock](https://en.cppreference.com/w/cpp/thread/scoped_lock)

**Notes:**
*** promo
See also `base::AutoLock`.
***

### std::timespec_get <sup>[tbd]</sup>

```c++
std::timespec ts;
std::timespec_get(&ts, TIME_UTC);
```

**Description:** Gets the current calendar time in the given time base.

**Documentation:**
[std::timespec_get](https://en.cppreference.com/w/cpp/chrono/c/timespec_get)

**Notes:**
*** promo
None
***

## Abseil Allowed Library Features {#absl-allowlist}

The following Abseil library features are allowed in the Chromium codebase.

### Optional <sup>[allowed]</sup>

```c++
absl::optional
```

**Description:** Early adaptation of C++17 `std::optional`.

**Documentation:**
[std::optional](https://en.cppreference.com/w/cpp/utility/optional)

**Notes:**
*** promo
Replaces `base::Optional`.
[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/zUGqagX1NFU)
***

### Status <sup>[allowed]</sup>

```c++
absl::Status
```

**Description:** Type for returning detailed errors.

**Documentation:**
[status.h](https://source.chromium.org/chromium/chromium/src/+/main:third_party/abseil-cpp/absl/status/status.h)

**Notes:**
*** promo
Approved for use inside a wrapper type. Use
[abseil_string_conversions.h](https://source.chromium.org/chromium/chromium/src/+/main:base/strings/abseil_string_conversions.h)
to convert to and from
[absl::string_view](https://source.chromium.org/chromium/chromium/src/+/main:third_party/abseil-cpp/absl/strings/string_view.h)
so the wrapper can expose
[base::StringPiece](https://source.chromium.org/chromium/chromium/src/+/main:base/strings/string_piece.h).
Use
[absl::Cord](https://source.chromium.org/chromium/chromium/src/+/main:third_party/abseil-cpp/absl/strings/cord.h)
directly as minimally necessary to interface; do not expose in the wrapper type
API.

[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/ImdFCSZ-NMA)
***

### Variant <sup>[allowed]</sup>

```c++
absl::variant
```

**Description:** Early adaptation of C++17 `std::variant`.

**Documentation:**
[std::variant](https://en.cppreference.com/w/cpp/utility/variant)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/DqvG-TpvMyU)
***

## Abseil Banned Library Features {#absl-blocklist}

The following Abseil library features are not allowed in the Chromium codebase.

### Any <sup>[banned]</sup>

```c++
absl::any a = int{5};
EXPECT_THAT(absl::any_cast<int>(&a), Pointee(5));
EXPECT_EQ(absl::any_cast<size_t>(&a), nullptr);
```

**Description:** Early adaptation of C++17 `std::any`.

**Documentation:** [std::any](https://en.cppreference.com/w/cpp/utility/any)

**Notes:**
*** promo
Banned since workaround for lack of RTTI isn't compatible with the component
build ([Bug](https://crbug.com/1096380)). Also see `std::any`.
***

### Command line flags <sup>[banned]</sup>

```c++
ABSL_FLAG(bool, logs, false, "print logs to stderr");
app --logs=true;
```

**Description:** Allows programmatic access to flag values passed on the
command-line to binaries.

**Documentation:** [Flags Library](https://abseil.io/docs/cpp/guides/flags)

**Notes:**
*** promo
Banned since workaround for lack of RTTI isn't compatible with the component
build. ([Bug](https://crbug.com/1096380)) Use `base::CommandLine` instead.
***

### Span <sup>[banned]</sup>

```c++
absl::Span
```

**Description:** Early adaptation of C++20 `std::span`.

**Documentation:** [Using absl::Span](https://abseil.io/tips/93)

**Notes:**
*** promo
Banned due to being less std::-compliant than `base::span`. Keep using
`base::span`.
***

### string_view <sup>[banned]</sup>

```c++
absl::string_view
```

**Description:** Early adaptation of C++17 `std::string_view`.

**Documentation:** [absl::string_view](https://abseil.io/tips/1)

**Notes:**
*** promo
Banned due to only working with 8-bit characters. Keep using
`base::StringPiece` from `base/strings/`.
***

## Abseil TBD Features {#absl-review}

The following Abseil library features are not allowed in the Chromium codebase.
See the top of this page on how to propose moving a feature from this list into
the allowed or banned sections.

### 128bit integer <sup>[tbd]</sup>

```c++
uint64_t a;
absl::uint128 v = a;
```

**Description:** Signed and unsigned 128-bit integer types meant to mimic
intrinsic types as closely as possible.

**Documentation:**
[Numerics](https://abseil.io/docs/cpp/guides/numeric)

**Notes:**
*** promo
None
***

### bind_front <sup>[tbd]</sup>

```c++
absl::bind_front
```

**Description:** Binds the first N arguments of an invocable object and stores them by value.

**Documentation:**
*   [bind_front.h](https://source.chromium.org/chromium/chromium/src/+/main:third_party/abseil-cpp/absl/functional/bind_front.h)
*   [Avoid std::bind](https://abseil.io/tips/108)

**Notes:**
*** promo
Overlaps with `base::Bind`.
***

### Cleanup <sup>[tbd]</sup>

```c++
FILE* sink_file = fopen(sink_path, "w");
auto sink_closer = absl::MakeCleanup([sink_file] { fclose(sink_file); });
```

**Description:** Implements the scope guard idiom, invoking the contained
callback's `operator()() &&` on scope exit.

**Documentation:**
[cleanup.h](https://source.chromium.org/chromium/chromium/src/+/main:third_party/abseil-cpp/absl/cleanup/cleanup.h)

**Notes:**
*** promo
Similar to `defer` in Golang.
***

### Containers <sup>[tbd]</sup>

```c++
absl::flat_hash_map
absl::flat_hash_set
absl::node_hash_map
absl::node_hash_set
absl::btree_map
absl::btree_set
absl::btree_multimap
absl::btree_multiset
absl::InlinedVector
absl::FixedArray
```

**Description:** Alternatives to STL containers designed to be more efficient
in the general case.

**Documentation:**
*   [Containers](https://abseil.io/docs/cpp/guides/container)
*   [Hash](https://abseil.io/docs/cpp/guides/hash)

**Notes:**
*** promo
Supplements `base/containers/`.
***

### Container utilities <sup>[tbd]</sup>

```c++
auto it = absl::c_find(container, value);
```

**Description:** Container-based versions of algorithmic functions within C++
standard library.

**Documentation:**
[container.h](https://source.chromium.org/chromium/chromium/src/+/main:third_party/abseil-cpp/absl/algorithm/container.h)

**Notes:**
*** promo
Overlaps with `base/ranges/algorithm.h`.
***

### FunctionRef <sup>[tbd]</sup>

```c++
absl::FunctionRef
```

**Description:** Type for holding a non-owning reference to an object of any
invocable type.

**Documentation:**
[function_ref.h](https://source.chromium.org/chromium/chromium/src/+/main:third_party/abseil-cpp/absl/functional/function_ref.h)

**Notes:**
*** promo
None
***

### Random <sup>[tbd]</sup>

```c++
absl::BitGen bitgen;
size_t index = absl::Uniform(bitgen, 0u, elems.size());
```

**Description:** Functions and utilities for generating pseudorandom data.

**Documentation:** [Random library](https://abseil.io/docs/cpp/guides/random)

**Notes:**
*** promo
Overlaps with `base/rand_util.h`.
***

### StatusOr <sup>[tbd]</sup>

```c++
absl::StatusOr<T>
```

**Description:** An object that is either a usable value, or an error Status
explaining why such a value is not present.

**Documentation:**
[statusor.h](https://source.chromium.org/chromium/chromium/src/+/main:third_party/abseil-cpp/absl/status/statusor.h)

**Notes:**
*** promo
None
***

### String Formatting <sup>[tbd]</sup>

```c++
absl::StrFormat
```

**Description:** A typesafe replacement for the family of printf() string
formatting routines.

**Documentation:**
[String Formatting](https://abseil.io/docs/cpp/guides/format)

**Notes:**
*** promo
None
***

### Strings Library <sup>[tbd]</sup>

```c++
absl::StrSplit
absl::StrJoin
absl::StrCat
absl::StrAppend
absl::Substitute
absl::StrContains
```

**Description:** Classes and utility functions for manipulating and comparing
strings.

**Documentation:**
[String Utilities](https://abseil.io/docs/cpp/guides/strings)

**Notes:**
*** promo
Overlaps with `base/strings`.
***

### Synchronization <sup>[tbd]</sup>

```c++
absl::Mutex
```

**Description:** Primitives for managing tasks across different threads.

**Documentation:**
[Synchronization](https://abseil.io/docs/cpp/guides/synchronization)

**Notes:**
*** promo
Overlaps with `Lock` in `base/synchronization/`.
***

### Time library <sup>[tbd]</sup>

```c++
absl::Duration
absl::Time
absl::TimeZone
absl::CivilDay
```

**Description:** Abstractions for holding time values, both in terms of
absolute time and civil time.

**Documentation:** [Time](https://abseil.io/docs/cpp/guides/time)

**Notes:**
*** promo
Overlaps with `Time` APIs in `base/`.
***
