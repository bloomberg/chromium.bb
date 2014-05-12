// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/test_with_scope.h"
#include "tools/gn/value.h"

TEST(Value, ToString) {
  Value strval(NULL, "hi\" $me\\you\\$\\\"");
  EXPECT_EQ("hi\" $me\\you\\$\\\"", strval.ToString(false));
  EXPECT_EQ("\"hi\\\" \\$me\\you\\\\\\$\\\\\\\"\"", strval.ToString(true));

  // Void type.
  EXPECT_EQ("<void>", Value().ToString(false));

  // Test lists, bools, and ints.
  Value listval(NULL, Value::LIST);
  listval.list_value().push_back(Value(NULL, "hi\"me"));
  listval.list_value().push_back(Value(NULL, true));
  listval.list_value().push_back(Value(NULL, false));
  listval.list_value().push_back(Value(NULL, static_cast<int64>(42)));
  // Printing lists always causes embedded strings to be quoted (ignoring the
  // quote flag), or else they wouldn't make much sense.
  EXPECT_EQ("[\"hi\\\"me\", true, false, 42]", listval.ToString(false));
  EXPECT_EQ("[\"hi\\\"me\", true, false, 42]", listval.ToString(true));

  // Scopes.
  TestWithScope setup;
  Scope* scope = new Scope(setup.scope());
  Value scopeval(NULL, scoped_ptr<Scope>(scope));
  EXPECT_EQ("{ }", scopeval.ToString(false));

  scope->SetValue("a", Value(NULL, static_cast<int64>(42)), NULL);
  scope->SetValue("b", Value(NULL, "hello, world"), NULL);
  EXPECT_EQ("{\n  a = 42\n  b = \"hello, world\"\n}", scopeval.ToString(false));
}

