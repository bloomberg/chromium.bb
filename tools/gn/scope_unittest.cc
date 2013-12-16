// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/input_file.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/scope.h"
#include "tools/gn/test_with_scope.h"

TEST(Scope, NonRecursiveMergeTo) {
  TestWithScope setup;

  // Make a pretend parse node with proper tracking that we can blame for the
  // given value.
  InputFile input_file(SourceFile("//foo"));
  Token assignment_token(Location(&input_file, 1, 1), Token::STRING,
      "\"hello\"");
  LiteralNode assignment;
  assignment.set_value(assignment_token);

  Value old_value(&assignment, "hello");
  setup.scope()->SetValue("v", old_value, &assignment);

  // Detect collisions of values' values.
  {
    Scope new_scope(setup.settings());
    Value new_value(&assignment, "goodbye");
    new_scope.SetValue("v", new_value, &assignment);

    Err err;
    EXPECT_FALSE(new_scope.NonRecursiveMergeTo(
        setup.scope(), &assignment, "error", &err));
    EXPECT_TRUE(err.has_error());
  }

  // Don't flag values that technically collide but have the same value.
  {
    Scope new_scope(setup.settings());
    Value new_value(&assignment, "hello");
    new_scope.SetValue("v", new_value, &assignment);

    Err err;
    EXPECT_TRUE(new_scope.NonRecursiveMergeTo(
        setup.scope(), &assignment, "error", &err));
    EXPECT_FALSE(err.has_error());
  }
}
