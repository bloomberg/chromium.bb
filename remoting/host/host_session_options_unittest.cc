// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_session_options.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(HostSessionOptionsTest, ShouldBeAbleToAppendOptions) {
  HostSessionOptions options;
  options.Import("A:, B C :1, DE:2, EF");
  ASSERT_TRUE(options.Get("A"));
  ASSERT_EQ(*options.Get("B C "), "1");
  ASSERT_EQ(*options.Get("DE"), "2");
  ASSERT_FALSE(options.Get("EF"));
  ASSERT_FALSE(options.Get(" EF"));
  ASSERT_FALSE(options.Get("--FF"));

  options.Append("A", "100");
  options.Append("--FF", "3");
  ASSERT_EQ(*options.Get("A"), "100");
  ASSERT_EQ(*options.Get("--FF"), "3");
}

TEST(HostSessionOptionsTest, ShouldRemoveEmptyKeys) {
  HostSessionOptions options;
  options.Import("A:1,:,B:");
  ASSERT_TRUE(options.Get("A"));
  ASSERT_TRUE(options.Get("B"));
  ASSERT_FALSE(options.Get(""));
}

TEST(HostSessionOptionsTest, ShouldRemoveNonASCIIKeyOrValue) {
  HostSessionOptions options;
  options.Import("\xE9\x9B\xAA:value,key:\xE9\xA3\x9E,key2:value2");
  ASSERT_FALSE(options.Get("\xE9\x9B\xAA"));
  ASSERT_FALSE(options.Get("key"));
  ASSERT_EQ(*options.Get("key2"), "value2");
}

TEST(HostSessionOptionsTest, ImportAndExport) {
  HostSessionOptions options;
  options.Import("A:,B:,C:D,E:V");
  std::string result = options.Export();

  HostSessionOptions other;
  other.Append("C", "X");
  other.Import(result);
  ASSERT_EQ(options.Export(), other.Export());
}

}  // namespace remoting
