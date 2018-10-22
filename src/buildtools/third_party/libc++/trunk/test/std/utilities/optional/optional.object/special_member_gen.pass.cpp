//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++98, c++03, c++11, c++14
// <optional>


#include <optional>
#include <type_traits>
#include <cassert>

#include "archetypes.hpp"

template <class T>
struct SpecialMemberTest {
    using O = std::optional<T>;

    static_assert(std::is_default_constructible_v<O>,
        "optional is always default constructible.");
    static_assert(std::is_copy_constructible_v<O> == std::is_copy_constructible_v<T>,
        "optional<T> is copy constructible if and only if T is copy constructible.");
    static_assert(std::is_move_constructible_v<O> ==
        (std::is_copy_constructible_v<T> || std::is_move_constructible_v<T>),
        "optional<T> is move constructible if and only if T is copy or move constructible.");
    static_assert(std::is_copy_assignable_v<O> ==
        (std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>),
        "optional<T> is copy assignable if and only if T is both copy "
        "constructible and copy assignable.");
    static_assert(std::is_move_assignable_v<O> ==
        ((std::is_move_constructible_v<T> && std::is_move_assignable_v<T>) ||
         (std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>)),
        "optional<T> is move assignable if and only if T is both move constructible and "
        "move assignable, or both copy constructible and copy assignable.");

    // The following tests are for not-yet-standardized behavior (P0602):
    static_assert(std::is_trivially_destructible_v<O> ==
        std::is_trivially_destructible_v<T>,
        "optional<T> is trivially destructible if and only if T is.");
    static_assert(std::is_trivially_copy_constructible_v<O> ==
        std::is_trivially_copy_constructible_v<T>,
        "optional<T> is trivially copy constructible if and only if T is.");
    static_assert(std::is_trivially_move_constructible_v<O> ==
        std::is_trivially_move_constructible_v<T> ||
        (!std::is_move_constructible_v<T> && std::is_trivially_copy_constructible_v<T>),
        "optional<T> is trivially move constructible if T is trivially move constructible, "
        "or if T is trivially copy constructible and is not move constructible.");
    static_assert(std::is_trivially_copy_assignable_v<O> ==
        (std::is_trivially_destructible_v<T> &&
         std::is_trivially_copy_constructible_v<T> &&
         std::is_trivially_copy_assignable_v<T>),
        "optional<T> is trivially copy assignable if and only if T is trivially destructible, "
        "trivially copy constructible, and trivially copy assignable.");
    static_assert(std::is_trivially_move_assignable_v<O> ==
        (std::is_trivially_destructible_v<T> &&
         ((std::is_trivially_move_constructible_v<T> && std::is_trivially_move_assignable_v<T>) ||
          ((!std::is_move_constructible_v<T> || !std::is_move_assignable_v<T>) &&
           std::is_trivially_copy_constructible_v<T> && std::is_trivially_copy_assignable_v<T>))),
        "optional<T> is trivially move assignable if T is trivially destructible, and either "
        "(1) trivially move constructible and trivially move assignable, or "
        "(2) not move constructible or not move assignable, and "
        "trivially copy constructible and trivially copy assignable.");
};

template <class ...Args> static void sink(Args&&...) {}

template <class ...TestTypes>
struct DoTestsMetafunction {
    DoTestsMetafunction() { sink(SpecialMemberTest<TestTypes>{}...); }
};

struct TrivialMoveNonTrivialCopy {
    TrivialMoveNonTrivialCopy() = default;
    TrivialMoveNonTrivialCopy(const TrivialMoveNonTrivialCopy&) {}
    TrivialMoveNonTrivialCopy(TrivialMoveNonTrivialCopy&&) = default;
    TrivialMoveNonTrivialCopy& operator=(const TrivialMoveNonTrivialCopy&) { return *this; }
    TrivialMoveNonTrivialCopy& operator=(TrivialMoveNonTrivialCopy&&) = default;
};

struct TrivialCopyNonTrivialMove {
    TrivialCopyNonTrivialMove() = default;
    TrivialCopyNonTrivialMove(const TrivialCopyNonTrivialMove&) = default;
    TrivialCopyNonTrivialMove(TrivialCopyNonTrivialMove&&) {}
    TrivialCopyNonTrivialMove& operator=(const TrivialCopyNonTrivialMove&) = default;
    TrivialCopyNonTrivialMove& operator=(TrivialCopyNonTrivialMove&&) { return *this; }
};

int main()
{
    sink(
        ImplicitTypes::ApplyTypes<DoTestsMetafunction>{},
        ExplicitTypes::ApplyTypes<DoTestsMetafunction>{},
        NonLiteralTypes::ApplyTypes<DoTestsMetafunction>{},
        NonTrivialTypes::ApplyTypes<DoTestsMetafunction>{},
        DoTestsMetafunction<TrivialMoveNonTrivialCopy, TrivialCopyNonTrivialMove>{}
    );
}
