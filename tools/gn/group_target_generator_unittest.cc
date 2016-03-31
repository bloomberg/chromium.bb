// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/group_target_generator.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/test_with_scope.h"

namespace {

// Returns true on success, false if write_file signaled an error.
bool ParseWriteRuntimeDeps(Scope* scope, const std::string& value) {
  TestParseInput input(
      "group(\"foo\") { write_runtime_deps = " + value + "}");
  if (input.has_error())
    return false;

  Err err;
  input.parsed()->Execute(scope, &err);
  return !err.has_error();
}

}  // namespace


// Tests that actions can't have output substitutions.
TEST(GroupTargetGenerator, WriteRuntimeDeps) {
  Scheduler scheduler;
  TestWithScope setup;
  Scope::ItemVector items_;
  setup.scope()->set_item_collector(&items_);

  // Should refuse to write files outside of the output dir.
  EXPECT_FALSE(ParseWriteRuntimeDeps(setup.scope(), "\"//foo.txt\""));
  EXPECT_EQ(0U, scheduler.GetWriteRuntimeDepsTargets().size());

  // Should fail for garbage inputs.
  EXPECT_FALSE(ParseWriteRuntimeDeps(setup.scope(), "0"));
  EXPECT_EQ(0U, scheduler.GetWriteRuntimeDepsTargets().size());

  // Should be able to write inside the out dir.
  EXPECT_TRUE(ParseWriteRuntimeDeps(setup.scope(), "\"//out/Debug/foo.txt\""));
  EXPECT_EQ(1U, scheduler.GetWriteRuntimeDepsTargets().size());
}

