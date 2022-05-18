// Copyright 2021 The Tint Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/tint/transform/remove_unreachable_statements.h"

#include "src/tint/transform/test_helper.h"

namespace tint::transform {
namespace {

using RemoveUnreachableStatementsTest = TransformTest;

TEST_F(RemoveUnreachableStatementsTest, ShouldRunEmptyModule) {
    auto* src = R"()";

    EXPECT_FALSE(ShouldRun<RemoveUnreachableStatements>(src));
}

TEST_F(RemoveUnreachableStatementsTest, ShouldRunHasNoUnreachable) {
    auto* src = R"(
fn f() {
  if (true) {
    var x = 1;
  }
}
)";

    EXPECT_FALSE(ShouldRun<RemoveUnreachableStatements>(src));
}

TEST_F(RemoveUnreachableStatementsTest, ShouldRunHasUnreachable) {
    auto* src = R"(
fn f() {
  return;
  if (true) {
    var x = 1;
  }
}
)";

    EXPECT_TRUE(ShouldRun<RemoveUnreachableStatements>(src));
}

TEST_F(RemoveUnreachableStatementsTest, EmptyModule) {
    auto* src = "";
    auto* expect = "";

    auto got = Run<RemoveUnreachableStatements>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(RemoveUnreachableStatementsTest, Return) {
    auto* src = R"(
fn f() {
  return;
  var remove_me = 1;
  if (true) {
    var remove_me_too = 1;
  }
}
)";

    auto* expect = R"(
fn f() {
  return;
}
)";

    auto got = Run<RemoveUnreachableStatements>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(RemoveUnreachableStatementsTest, NestedReturn) {
    auto* src = R"(
fn f() {
  {
    {
      return;
    }
  }
  var remove_me = 1;
  if (true) {
    var remove_me_too = 1;
  }
}
)";

    auto* expect = R"(
fn f() {
  {
    {
      return;
    }
  }
}
)";

    auto got = Run<RemoveUnreachableStatements>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(RemoveUnreachableStatementsTest, Discard) {
    auto* src = R"(
fn f() {
  discard;
  var remove_me = 1;
  if (true) {
    var remove_me_too = 1;
  }
}
)";

    auto* expect = R"(
fn f() {
  discard;
}
)";

    auto got = Run<RemoveUnreachableStatements>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(RemoveUnreachableStatementsTest, NestedDiscard) {
    auto* src = R"(
fn f() {
  {
    {
      discard;
    }
  }
  var remove_me = 1;
  if (true) {
    var remove_me_too = 1;
  }
}
)";

    auto* expect = R"(
fn f() {
  {
    {
      discard;
    }
  }
}
)";

    auto got = Run<RemoveUnreachableStatements>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(RemoveUnreachableStatementsTest, CallToFuncWithDiscard) {
    auto* src = R"(
fn DISCARD() {
  discard;
}

fn f() {
  DISCARD();
  var remove_me = 1;
  if (true) {
    var remove_me_too = 1;
  }
}
)";

    auto* expect = R"(
fn DISCARD() {
  discard;
}

fn f() {
  DISCARD();
}
)";

    auto got = Run<RemoveUnreachableStatements>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(RemoveUnreachableStatementsTest, CallToFuncWithIfDiscard) {
    auto* src = R"(
fn DISCARD() {
  if (true) {
    discard;
  }
}

fn f() {
  DISCARD();
  var preserve_me = 1;
  if (true) {
    var preserve_me_too = 1;
  }
}
)";

    auto* expect = src;

    auto got = Run<RemoveUnreachableStatements>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(RemoveUnreachableStatementsTest, IfDiscardElseDiscard) {
    auto* src = R"(
fn f() {
  if (true) {
    discard;
  } else {
    discard;
  }
  var remove_me = 1;
  if (true) {
    var remove_me_too = 1;
  }
}
)";

    auto* expect = R"(
fn f() {
  if (true) {
    discard;
  } else {
    discard;
  }
}
)";

    auto got = Run<RemoveUnreachableStatements>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(RemoveUnreachableStatementsTest, IfDiscardElseReturn) {
    auto* src = R"(
fn f() {
  if (true) {
    discard;
  } else {
    return;
  }
  var remove_me = 1;
  if (true) {
    var remove_me_too = 1;
  }
}
)";

    auto* expect = R"(
fn f() {
  if (true) {
    discard;
  } else {
    return;
  }
}
)";

    auto got = Run<RemoveUnreachableStatements>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(RemoveUnreachableStatementsTest, IfDiscard) {
    auto* src = R"(
fn f() {
  if (true) {
    discard;
  }
  var preserve_me = 1;
  if (true) {
    var preserve_me_too = 1;
  }
}
)";

    auto* expect = src;

    auto got = Run<RemoveUnreachableStatements>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(RemoveUnreachableStatementsTest, IfReturn) {
    auto* src = R"(
fn f() {
  if (true) {
    return;
  }
  var preserve_me = 1;
  if (true) {
    var preserve_me_too = 1;
  }
}
)";

    auto* expect = src;

    auto got = Run<RemoveUnreachableStatements>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(RemoveUnreachableStatementsTest, IfElseDiscard) {
    auto* src = R"(
fn f() {
  if (true) {
  } else {
    discard;
  }
  var preserve_me = 1;
  if (true) {
    var preserve_me_too = 1;
  }
}
)";

    auto* expect = src;

    auto got = Run<RemoveUnreachableStatements>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(RemoveUnreachableStatementsTest, IfElseReturn) {
    auto* src = R"(
fn f() {
  if (true) {
  } else {
    return;
  }
  var preserve_me = 1;
  if (true) {
    var preserve_me_too = 1;
  }
}
)";

    auto* expect = src;

    auto got = Run<RemoveUnreachableStatements>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(RemoveUnreachableStatementsTest, LoopWithDiscard) {
    auto* src = R"(
fn f() {
  loop {
    var a = 1;
    discard;

    continuing {
      var b = 2;
    }
  }
  var remove_me = 1;
  if (true) {
    var remove_me_too = 1;
  }
}
)";

    auto* expect = R"(
fn f() {
  loop {
    var a = 1;
    discard;

    continuing {
      var b = 2;
    }
  }
}
)";

    auto got = Run<RemoveUnreachableStatements>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(RemoveUnreachableStatementsTest, LoopWithConditionalBreak) {
    auto* src = R"(
fn f() {
  loop {
    var a = 1;
    if (true) {
      break;
    }

    continuing {
      var b = 2;
    }
  }
  var preserve_me = 1;
  if (true) {
    var preserve_me_too = 1;
  }
}
)";

    auto* expect = src;

    auto got = Run<RemoveUnreachableStatements>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(RemoveUnreachableStatementsTest, LoopWithConditionalBreakInContinuing) {
    auto* src = R"(
fn f() {
  loop {

    continuing {
      if (true) {
        break;
      }
    }
  }
  var preserve_me = 1;
  if (true) {
    var preserve_me_too = 1;
  }
}
)";

    auto* expect = src;

    auto got = Run<RemoveUnreachableStatements>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(RemoveUnreachableStatementsTest, SwitchDefaultDiscard) {
    auto* src = R"(
fn f() {
  switch(1) {
    default: {
      discard;
    }
  }
  var remove_me = 1;
  if (true) {
    var remove_me_too = 1;
  }
}
)";

    auto* expect = R"(
fn f() {
  switch(1) {
    default: {
      discard;
    }
  }
}
)";

    auto got = Run<RemoveUnreachableStatements>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(RemoveUnreachableStatementsTest, SwitchCaseReturnDefaultDiscard) {
    auto* src = R"(
fn f() {
  switch(1) {
    case 0: {
      return;
    }
    default: {
      discard;
    }
  }
  var remove_me = 1;
  if (true) {
    var remove_me_too = 1;
  }
}
)";

    auto* expect = R"(
fn f() {
  switch(1) {
    case 0: {
      return;
    }
    default: {
      discard;
    }
  }
}
)";

    auto got = Run<RemoveUnreachableStatements>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(RemoveUnreachableStatementsTest, SwitchCaseBreakDefaultDiscard) {
    auto* src = R"(
fn f() {
  switch(1) {
    case 0: {
      break;
    }
    default: {
      discard;
    }
  }
  var preserve_me = 1;
  if (true) {
    var preserve_me_too = 1;
  }
}
)";

    auto* expect = src;

    auto got = Run<RemoveUnreachableStatements>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(RemoveUnreachableStatementsTest, SwitchCaseReturnDefaultBreak) {
    auto* src = R"(
fn f() {
  switch(1) {
    case 0: {
      return;
    }
    default: {
      break;
    }
  }
  var preserve_me = 1;
  if (true) {
    var preserve_me_too = 1;
  }
}
)";

    auto* expect = src;

    auto got = Run<RemoveUnreachableStatements>(src);

    EXPECT_EQ(expect, str(got));
}

}  // namespace
}  // namespace tint::transform
