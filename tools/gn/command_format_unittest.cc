// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/commands.h"
#include "tools/gn/setup.h"

namespace commands {
bool FormatFileToString(Setup* setup,
                        const SourceFile& file,
                        bool dump_tree,
                        std::string* output);
}  // namespace commands

#define FORMAT_TEST(n)                                                 \
  TEST(Format, n) {                                                    \
    ::Setup setup;                                                     \
    std::string out;                                                   \
    std::string expected;                                              \
    EXPECT_TRUE(commands::FormatFileToString(                          \
        &setup,                                                        \
        SourceFile("//tools/gn/format_test_data/" #n ".gn"),           \
        false,                                                         \
        &out));                                                        \
    ASSERT_TRUE(base::ReadFileToString(                                \
        base::FilePath(FILE_PATH_LITERAL("tools/gn/format_test_data/") \
                           FILE_PATH_LITERAL(#n)                       \
                               FILE_PATH_LITERAL(".golden")),          \
        &expected));                                                   \
    EXPECT_EQ(expected, out);                                          \
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
FORMAT_TEST(015)
FORMAT_TEST(016)
FORMAT_TEST(017)
FORMAT_TEST(018)
FORMAT_TEST(019)
FORMAT_TEST(020)
FORMAT_TEST(021)
FORMAT_TEST(022)
FORMAT_TEST(023)
FORMAT_TEST(024)
FORMAT_TEST(025)
FORMAT_TEST(026)
FORMAT_TEST(027)
FORMAT_TEST(028)
FORMAT_TEST(029)
FORMAT_TEST(030)
FORMAT_TEST(031)
// TODO(scottmg): Continued conditions aren't aligned properly: FORMAT_TEST(032)
FORMAT_TEST(033)
// TODO(scottmg): args+rebase_path unnecessarily split: FORMAT_TEST(034)
FORMAT_TEST(035)
FORMAT_TEST(036)
// TODO(scottmg): Ugly line breaking: FORMAT_TEST(037)
FORMAT_TEST(038)
// TODO(scottmg): Bad break, exceeding 80 col: FORMAT_TEST(039)
// TODO(scottmg): Bad break, exceeding 80 col: FORMAT_TEST(040)
FORMAT_TEST(041)
