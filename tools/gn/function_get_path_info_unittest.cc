// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/functions.h"
#include "tools/gn/test_with_scope.h"

namespace {

class GetPathInfoTest : public testing::Test {
 public:
  GetPathInfoTest() : testing::Test() {
    setup_.scope()->set_source_dir(SourceDir("//src/foo/"));
  }

  // Convenience wrapper to call GetLabelInfo.
  std::string Call(const std::string& input, const std::string& what) {
    FunctionCallNode function;

    std::vector<Value> args;
    args.push_back(Value(NULL, input));
    args.push_back(Value(NULL, what));

    Err err;
    Value result = functions::RunGetPathInfo(setup_.scope(), &function,
                                             args, &err);
    if (err.has_error()) {
      EXPECT_TRUE(result.type() == Value::NONE);
      return std::string();
    }
    return result.string_value();
  }

 protected:
  TestWithScope setup_;
};

}  // namespace

TEST_F(GetPathInfoTest, File) {
  EXPECT_EQ("bar.txt", Call("foo/bar.txt", "file"));
  EXPECT_EQ("bar.txt", Call("bar.txt", "file"));
  EXPECT_EQ("bar.txt", Call("/bar.txt", "file"));
  EXPECT_EQ("", Call("foo/", "file"));
  EXPECT_EQ("", Call("//", "file"));
  EXPECT_EQ("", Call("/", "file"));
}

TEST_F(GetPathInfoTest, Name) {
  EXPECT_EQ("bar", Call("foo/bar.txt", "name"));
  EXPECT_EQ("bar", Call("bar.", "name"));
  EXPECT_EQ("", Call("/.txt", "name"));
  EXPECT_EQ("", Call("foo/", "name"));
  EXPECT_EQ("", Call("//", "name"));
  EXPECT_EQ("", Call("/", "name"));
}

TEST_F(GetPathInfoTest, Extension) {
  EXPECT_EQ("txt", Call("foo/bar.txt", "extension"));
  EXPECT_EQ("", Call("bar.", "extension"));
  EXPECT_EQ("txt", Call("/.txt", "extension"));
  EXPECT_EQ("", Call("f.oo/", "extension"));
  EXPECT_EQ("", Call("//", "extension"));
  EXPECT_EQ("", Call("/", "extension"));
}

TEST_F(GetPathInfoTest, Dir) {
  EXPECT_EQ("foo", Call("foo/bar.txt", "dir"));
  EXPECT_EQ(".", Call("bar.txt", "dir"));
  EXPECT_EQ("foo/bar", Call("foo/bar/baz", "dir"));
  EXPECT_EQ("//foo", Call("//foo/", "dir"));
  EXPECT_EQ("//.", Call("//", "dir"));
  EXPECT_EQ("/foo", Call("/foo/", "dir"));
  EXPECT_EQ("/.", Call("/", "dir"));
}

// Note "current dir" is "//src/foo"
TEST_F(GetPathInfoTest, AbsPath) {
  EXPECT_EQ("//src/foo/foo/bar.txt", Call("foo/bar.txt", "abspath"));
  EXPECT_EQ("//src/foo/bar.txt", Call("bar.txt", "abspath"));
  EXPECT_EQ("//src/foo/bar/", Call("bar/", "abspath"));
  EXPECT_EQ("//foo", Call("//foo", "abspath"));
  EXPECT_EQ("//foo/", Call("//foo/", "abspath"));
  EXPECT_EQ("//", Call("//", "abspath"));
  EXPECT_EQ("/foo", Call("/foo", "abspath"));
  EXPECT_EQ("/foo/", Call("/foo/", "abspath"));
  EXPECT_EQ("/", Call("/", "abspath"));
}
