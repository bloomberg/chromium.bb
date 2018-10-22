//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <strstream>

// There was an overflow in the dylib on older macOS versions
// UNSUPPORTED: availability=macosx10.8
// UNSUPPORTED: availability=macosx10.7

// class strstreambuf

// int overflow(int c);

#include <iostream>
#include <string>
#include <strstream>

int main() {
  std::ostrstream oss;
  std::string s;

  for (int i = 0; i < 4096; ++i)
    s.push_back((i % 16) + 'a');

  oss << s << std::ends;
  std::cout << oss.str();
  oss.freeze(false);

  return 0;
}
