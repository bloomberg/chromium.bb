// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/wtf/Optional.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {
namespace {

struct IntBox {
  IntBox(int n) : number(n) {}
  int number;
};

class DestructionNotifier {
 public:
  DestructionNotifier(bool& flag) : flag_(flag) {}
  ~DestructionNotifier() { flag_ = true; }

 private:
  bool& flag_;
};

TEST(OptionalTest, BooleanTest) {
  Optional<int> optional;
  EXPECT_FALSE(optional);
  optional.emplace(0);
  EXPECT_TRUE(optional);
}

TEST(OptionalTest, Dereference) {
  Optional<int> optional;
  optional.emplace(1);
  EXPECT_EQ(1, *optional);

  Optional<IntBox> optional_intbox;
  optional_intbox.emplace(42);
  EXPECT_EQ(42, optional_intbox->number);
}

TEST(OptionalTest, DestructorCalled) {
  // Destroying a disengaged optional shouldn't do anything.
  { Optional<DestructionNotifier> optional; }

  // Destroying an engaged optional should call the destructor.
  bool is_destroyed = false;
  {
    Optional<DestructionNotifier> optional;
    optional.emplace(is_destroyed);
    EXPECT_FALSE(is_destroyed);
  }
  EXPECT_TRUE(is_destroyed);
}

}  // namespace
}  // namespace WTF
