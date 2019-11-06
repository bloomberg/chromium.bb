// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/json/json_writer.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/base/error.h"

namespace openscreen {

TEST(JsonWriterTest, NullValueReturnsError) {
  JsonWriter writer;

  const auto null_value = Json::Value();
  const auto actual = writer.Write(null_value);

  EXPECT_TRUE(actual.is_error());
  EXPECT_EQ(actual.error().code(), Error::Code::kJsonWriteError);
}

TEST(JsonWriterTest, ValidValueReturnsString) {
  JsonWriter writer;

  const Json::Int64 value = 31337;
  const auto actual = writer.Write(value);

  EXPECT_TRUE(actual.is_value());
  EXPECT_EQ(actual.value(), "31337\n");
}
}  // namespace openscreen
