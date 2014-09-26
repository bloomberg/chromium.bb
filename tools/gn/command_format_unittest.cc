// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/commands.h"

namespace commands {
bool FormatFileToString(const std::string& input_filename,
                        bool dump_tree,
                        std::string* output);
}  // namespace commands

#define FORMAT_TEST(n)                                                 \
  TEST(Format, n) {                                                    \
    std::string out;                                                   \
    std::string expected;                                              \
    EXPECT_TRUE(commands::FormatFileToString(                          \
        "//tools/gn/format_test_data/" #n ".gn", false, &out));        \
    ASSERT_TRUE(base::ReadFileToString(                                \
        base::FilePath(FILE_PATH_LITERAL("tools/gn/format_test_data/") \
                           FILE_PATH_LITERAL(#n)                       \
                               FILE_PATH_LITERAL(".golden")),          \
        &expected));                                                   \
    EXPECT_EQ(out, expected);                                          \
  }

// These are expanded out this way rather than a runtime loop so that
// --gtest_filter works as expected for individual test running.
FORMAT_TEST(001)
FORMAT_TEST(002)
FORMAT_TEST(003)
FORMAT_TEST(004)
FORMAT_TEST(005)
FORMAT_TEST(006)
FORMAT_TEST(007)
FORMAT_TEST(008)
FORMAT_TEST(009)
FORMAT_TEST(010)
FORMAT_TEST(011)
FORMAT_TEST(012)
FORMAT_TEST(013)
FORMAT_TEST(014)
