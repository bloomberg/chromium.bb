//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++98, c++03

// <experimental/filesystem>

// class path

// path(path&&) noexcept

#include "filesystem_include.hpp"
#include <type_traits>
#include <cassert>

#include "test_macros.h"
#include "count_new.hpp"


int main() {
  using namespace fs;
  static_assert(std::is_nothrow_move_constructible<path>::value, "");
  assert(globalMemCounter.checkOutstandingNewEq(0));
  const std::string s("we really really really really really really really "
                      "really really long string so that we allocate");
  assert(globalMemCounter.checkOutstandingNewEq(1));
  path p(s);
  {
    DisableAllocationGuard g;
    path p2(std::move(p));
    assert(p2.native() == s);
    assert(p.native() != s); // Testing moved from state
  }
}
