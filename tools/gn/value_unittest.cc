// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/value.h"

TEST(Value, ToString) {
  Value strval(NULL, "hi\" $me\\you\\$\\\"");
  EXPECT_EQ("hi\" $me\\you\\$\\\"", strval.ToString(false));
  EXPECT_EQ("\"hi\\\" \\$me\\you\\\\\\$\\\\\\\"\"", strval.ToString(true));

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

  // Some weird types, we may want to enhance or change printing of these, but
  // here we test the current behavior.
  EXPECT_EQ("<void>", Value().ToString(false));
  EXPECT_EQ("<scope>", Value(NULL, Value::SCOPE).ToString(false));
}

