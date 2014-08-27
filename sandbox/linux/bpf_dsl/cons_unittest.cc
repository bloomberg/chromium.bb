// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/bpf_dsl/cons.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {
namespace {

std::string Join(Cons<char>::List char_list) {
  std::string res;
  for (Cons<char>::List it = char_list; it.get(); it = it->tail()) {
    res.push_back(it->head());
  }
  return res;
}

TEST(ConsListTest, Basic) {
  Cons<char>::List ba =
      Cons<char>::Make('b', Cons<char>::Make('a', Cons<char>::List()));
  EXPECT_EQ("ba", Join(ba));

  Cons<char>::List cba = Cons<char>::Make('c', ba);
  Cons<char>::List dba = Cons<char>::Make('d', ba);
  EXPECT_EQ("cba", Join(cba));
  EXPECT_EQ("dba", Join(dba));
}

}  // namespace
}  // namespace sandbox
