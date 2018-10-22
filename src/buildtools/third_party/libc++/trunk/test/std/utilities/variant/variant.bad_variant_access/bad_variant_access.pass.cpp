// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++98, c++03, c++11, c++14

// XFAIL: with_system_cxx_lib=macosx10.12
// XFAIL: with_system_cxx_lib=macosx10.11
// XFAIL: with_system_cxx_lib=macosx10.10
// XFAIL: with_system_cxx_lib=macosx10.9
// XFAIL: with_system_cxx_lib=macosx10.7
// XFAIL: with_system_cxx_lib=macosx10.8

// <variant>

/*

 class bad_variant_access : public exception {
public:
  bad_variant_access() noexcept;
  virtual const char* what() const noexcept;
};

*/

#include <cassert>
#include <exception>
#include <type_traits>
#include <variant>

int main() {
  static_assert(std::is_base_of<std::exception, std::bad_variant_access>::value,
                "");
  static_assert(noexcept(std::bad_variant_access{}), "must be noexcept");
  static_assert(noexcept(std::bad_variant_access{}.what()), "must be noexcept");
  std::bad_variant_access ex;
  assert(ex.what());
}
