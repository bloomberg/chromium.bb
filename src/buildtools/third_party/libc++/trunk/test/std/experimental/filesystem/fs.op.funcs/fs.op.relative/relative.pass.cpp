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

// path proximate(const path& p, error_code &ec)
// path proximate(const path& p, const path& base = current_path())
// path proximate(const path& p, const path& base, error_code& ec);

#include "filesystem_include.hpp"
#include <type_traits>
#include <vector>
#include <iostream>
#include <cassert>

#include "test_macros.h"
#include "test_iterators.h"
#include "count_new.hpp"
#include "rapid-cxx-test.hpp"
#include "filesystem_test_helper.hpp"


TEST_SUITE(filesystem_proximate_path_test_suite)

TEST_CASE(test_signature) {

}
int main() {
  // clang-format off
  struct {
    std::string input;
    std::string expect;
  } TestCases[] = {
      {"", fs::current_path()},
      {".", fs::current_path()},
      {StaticEnv::File, StaticEnv::File},
      {StaticEnv::Dir, StaticEnv::Dir},
      {StaticEnv::SymlinkToDir, StaticEnv::Dir},
      {StaticEnv::SymlinkToDir / "dir2/.", StaticEnv::Dir / "dir2"},
      // FIXME? If the trailing separator occurs in a part of the path that exists,
      // it is ommitted. Otherwise it is added to the end of the result.
      {StaticEnv::SymlinkToDir / "dir2/./", StaticEnv::Dir / "dir2"},
      {StaticEnv::SymlinkToDir / "dir2/DNE/./", StaticEnv::Dir / "dir2/DNE/"},
      {StaticEnv::SymlinkToDir / "dir2", StaticEnv::Dir2},
      {StaticEnv::SymlinkToDir / "dir2/../dir2/DNE/..", StaticEnv::Dir2 / ""},
      {StaticEnv::SymlinkToDir / "dir2/dir3/../DNE/DNE2", StaticEnv::Dir2 / "DNE/DNE2"},
      {StaticEnv::Dir / "../dir1", StaticEnv::Dir},
      {StaticEnv::Dir / "./.", StaticEnv::Dir},
      {StaticEnv::Dir / "DNE/../foo", StaticEnv::Dir / "foo"}
  };
  // clang-format on
  int ID = 0;
  bool Failed = false;
  for (auto& TC : TestCases) {
    ++ID;
    fs::path p(TC.input);
    const fs::path output = fs::weakly_canonical(p);
    if (output != TC.expect) {
      Failed = true;
      std::cerr << "TEST CASE #" << ID << " FAILED: \n";
      std::cerr << "  Input: '" << TC.input << "'\n";
      std::cerr << "  Expected: '" << TC.expect << "'\n";
      std::cerr << "  Output: '" << output.native() << "'";
      std::cerr << std::endl;
    }
  }
  return Failed;
}

TEST_SUITE_END()
