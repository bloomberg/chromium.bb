// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/escape.h"

TEST(Escape, Ninja) {
  EscapeOptions opts;
  opts.mode = ESCAPE_NINJA;
  std::string result = EscapeString("asdf: \"$\\bar", opts, NULL);
  EXPECT_EQ("asdf$:$ \"$$\\bar", result);
}

TEST(Escape, Shell) {
  EscapeOptions opts;
  opts.mode = ESCAPE_SHELL;
  std::string result = EscapeString("asdf: \"$\\bar", opts, NULL);
#if defined(OS_WIN)
  // Windows shell doesn't escape backslashes, but it does backslash-escape
  // quotes.
  EXPECT_EQ("\"asdf: \\\"$\\bar\"", result);
#else
  EXPECT_EQ("\"asdf: \\\"$\\\\bar\"", result);
#endif
}

TEST(Escape, UsedQuotes) {
  EscapeOptions shell_options;
  shell_options.mode = ESCAPE_SHELL;

  EscapeOptions ninja_options;
  ninja_options.mode = ESCAPE_NINJA;

  EscapeOptions ninja_shell_options;
  ninja_shell_options.mode = ESCAPE_NINJA_SHELL;

  // Shell escaping with quoting inhibited.
  bool used_quotes = false;
  shell_options.inhibit_quoting = true;
  EXPECT_EQ("foo bar", EscapeString("foo bar", shell_options, &used_quotes));
  EXPECT_TRUE(used_quotes);

  // Shell escaping with regular quoting.
  used_quotes = false;
  shell_options.inhibit_quoting = false;
  EXPECT_EQ("\"foo bar\"",
            EscapeString("foo bar", shell_options, &used_quotes));
  EXPECT_TRUE(used_quotes);

  // Ninja shell escaping should be the same.
  used_quotes = false;
  EXPECT_EQ("\"foo$ bar\"",
            EscapeString("foo bar", ninja_shell_options, &used_quotes));
  EXPECT_TRUE(used_quotes);

  // Ninja escaping shouldn't use quoting.
  used_quotes = false;
  EXPECT_EQ("foo$ bar", EscapeString("foo bar", ninja_options, &used_quotes));
  EXPECT_FALSE(used_quotes);

  // Used quotes not reset if it's already true.
  used_quotes = true;
  EscapeString("foo", ninja_options, &used_quotes);
  EXPECT_TRUE(used_quotes);
}
