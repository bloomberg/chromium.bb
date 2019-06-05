// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp_base/json/json_reader.h"

#include <string>

#include "osp_base/error.h"
#include "third_party/googletest/src/googlemock/include/gmock/gmock.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace openscreen {
namespace {
template <typename Value>
void AssertError(ErrorOr<Value> error_or, Error::Code code) {
  EXPECT_EQ(error_or.error().code(), code);
}
}  // namespace

TEST(JsonReaderTest, MalformedDocumentReturnsParseError) {
  JsonReader reader;

  const std::array<std::string, 4> kMalformedDocuments{
      {"", "{", "{ foo: bar }", R"({"foo": "bar", "foo": baz})"}};

  for (auto& document : kMalformedDocuments) {
    AssertError(reader.Read(document), Error::Code::kJsonParseError);
  }
}

TEST(JsonReaderTest, ValidEmptyDocumentParsedCorrectly) {
  JsonReader reader;

  const auto actual = reader.Read("{}");

  EXPECT_TRUE(actual.is_value());
  EXPECT_EQ(actual.value().getMemberNames().size(), 0);
}

// Jsoncpp has its own suite of tests ensure that things are parsed correctly,
// so we only do some rudimentary checks here to make sure we didn't mangle
// the value.
TEST(JsonReaderTest, ValidDocumentParsedCorrectly) {
  JsonReader reader;

  const auto actual = reader.Read(R"({"foo": "bar", "baz": 1337})");

  EXPECT_TRUE(actual.is_value());
  EXPECT_EQ(actual.value().getMemberNames().size(), 2);
}

}  // namespace openscreen
