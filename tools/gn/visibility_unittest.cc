// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/err.h"
#include "tools/gn/label.h"
#include "tools/gn/value.h"
#include "tools/gn/visibility.h"

namespace {

struct VisPatternCase {
  const char* input;
  bool success;

  Visibility::VisPattern::Type type;
  const char* dir;
  const char* name;
};

}  // namespace

TEST(Visibility, PatternParse) {
  SourceDir current_dir("//foo/");
  VisPatternCase cases[] = {
    // Missing stuff.
    { "", false, Visibility::VisPattern::MATCH, "", "" },
    { ":", false, Visibility::VisPattern::MATCH, "", "" },
    // Normal things.
    { ":bar", true, Visibility::VisPattern::MATCH, "//foo/", "bar" },
    { "//la:bar", true, Visibility::VisPattern::MATCH, "//la/", "bar" },
    { "*", true, Visibility::VisPattern::RECURSIVE_DIRECTORY, "", "" },
    { ":*", true, Visibility::VisPattern::DIRECTORY, "//foo/", "" },
    { "la:*", true, Visibility::VisPattern::DIRECTORY, "//foo/la/", "" },
    { "la/*:*", true, Visibility::VisPattern::RECURSIVE_DIRECTORY,
      "//foo/la/", "" },
    { "//la:*", true, Visibility::VisPattern::DIRECTORY, "//la/", "" },
    { "./*", true, Visibility::VisPattern::RECURSIVE_DIRECTORY, "//foo/", "" },
    { "foo/*", true, Visibility::VisPattern::RECURSIVE_DIRECTORY,
      "//foo/foo/", "" },
    { "//l/*", true, Visibility::VisPattern::RECURSIVE_DIRECTORY, "//l/", "" },
    // No toolchains allowed.
    { "//foo(//bar)", false, Visibility::VisPattern::MATCH, "", "" },
    // Wildcards in invalid places.
    { "*foo*:bar", false, Visibility::VisPattern::MATCH, "", "" },
    { "foo*:*bar", false, Visibility::VisPattern::MATCH, "", "" },
    { "*foo:bar", false, Visibility::VisPattern::MATCH, "", "" },
    { "foo:bar*", false, Visibility::VisPattern::MATCH, "", "" },
    { "*:*", true, Visibility::VisPattern::RECURSIVE_DIRECTORY, "", "" },
  };

  for (size_t i = 0; i < arraysize(cases); i++) {
    const VisPatternCase& cur = cases[i];
    Err err;
    Visibility::VisPattern result =
        Visibility::GetPattern(current_dir, Value(NULL, cur.input), &err);

    EXPECT_EQ(cur.success, !err.has_error()) << i << " " << cur.input;
    EXPECT_EQ(cur.type, result.type()) << i << " " << cur.input;
    EXPECT_EQ(cur.dir, result.dir().value()) << i << " " << cur.input;
    EXPECT_EQ(cur.name, result.name()) << i << " " << cur.input;
  }
}

TEST(Visibility, CanSeeMe) {
  Value list(NULL, Value::LIST);
  list.list_value().push_back(Value(NULL, "//rec/*"));  // Recursive.
  list.list_value().push_back(Value(NULL, "//dir:*"));  // One dir.
  list.list_value().push_back(Value(NULL, "//my:name"));  // Exact match.

  Err err;
  Visibility vis;
  ASSERT_TRUE(vis.Set(SourceDir("//"), list, &err));

  EXPECT_FALSE(vis.CanSeeMe(Label(SourceDir("//random/"), "thing")));
  EXPECT_FALSE(vis.CanSeeMe(Label(SourceDir("//my/"), "notname")));

  EXPECT_TRUE(vis.CanSeeMe(Label(SourceDir("//my/"), "name")));
  EXPECT_TRUE(vis.CanSeeMe(Label(SourceDir("//rec/"), "anything")));
  EXPECT_TRUE(vis.CanSeeMe(Label(SourceDir("//rec/a/"), "anything")));
  EXPECT_TRUE(vis.CanSeeMe(Label(SourceDir("//rec/b/"), "anything")));
  EXPECT_TRUE(vis.CanSeeMe(Label(SourceDir("//dir/"), "anything")));
  EXPECT_FALSE(vis.CanSeeMe(Label(SourceDir("//dir/a/"), "anything")));
  EXPECT_FALSE(vis.CanSeeMe(Label(SourceDir("//directory/"), "anything")));
}

TEST(Visibility, Public) {
  Err err;
  Visibility vis;
  ASSERT_TRUE(vis.Set(SourceDir("//"), Value(NULL, "*"), &err));

  EXPECT_TRUE(vis.CanSeeMe(Label(SourceDir("//random/"), "thing")));
  EXPECT_TRUE(vis.CanSeeMe(Label(SourceDir("//"), "")));
}

TEST(Visibility, Private) {
  Err err;
  Visibility vis;
  ASSERT_TRUE(vis.Set(SourceDir("//"), Value(NULL, Value::LIST), &err));

  EXPECT_FALSE(vis.CanSeeMe(Label(SourceDir("//random/"), "thing")));
  EXPECT_FALSE(vis.CanSeeMe(Label(SourceDir("//"), "")));
}
