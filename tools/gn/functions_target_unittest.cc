// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/scope.h"
#include "tools/gn/test_with_scope.h"


// Checks that we find unused identifiers in targets.
TEST(FunctionsTarget, CheckUnused) {
  Scheduler scheduler;
  TestWithScope setup;

  // The target generator needs a place to put the targets or it will fail.
  Scope::ItemVector item_collector;
  setup.scope()->set_item_collector(&item_collector);

  // Test a good one first.
  TestParseInput good_input(
      "source_set(\"foo\") {\n"
      "}\n");
  ASSERT_FALSE(good_input.has_error());
  Err err;
  good_input.parsed()->Execute(setup.scope(), &err);
  ASSERT_FALSE(err.has_error()) << err.message();

  // Test a source set (this covers everything but component) with an unused
  // variable.
  TestParseInput source_set_input(
      "source_set(\"foo\") {\n"
      "  unused = 5\n"
      "}\n");
  ASSERT_FALSE(source_set_input.has_error());
  err = Err();
  source_set_input.parsed()->Execute(setup.scope(), &err);
  ASSERT_TRUE(err.has_error());

  // Test a component, which is a different code path.
  TestParseInput component_input(
      "component_mode = \"static_library\"\n"
      "component(\"bar\") {\n"
      "  unused = 5\n"
      "}\n");
  ASSERT_FALSE(component_input.has_error());
  err = Err();
  component_input.parsed()->Execute(setup.scope(), &err);
  ASSERT_TRUE(err.has_error()) << err.message();
}
