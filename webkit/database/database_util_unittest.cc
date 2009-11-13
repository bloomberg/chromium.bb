// Copyright (c) 2009 The Chromium Authos. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/database/database_util.h"

using webkit_database::DatabaseUtil;

static void TestVfsFilePath(bool expected_result,
                            const char* vfs_file_path,
                            const char* expected_origin_identifier = "",
                            const char* expected_database_name = "",
                            const char* expected_sqlite_suffix = "") {
  string16 origin_identifier;
  string16 database_name;
  string16 sqlite_suffix;
  EXPECT_EQ(expected_result,
            DatabaseUtil::CrackVfsFilePath(ASCIIToUTF16(vfs_file_path),
                                           &origin_identifier,
                                           &database_name,
                                           &sqlite_suffix));
  EXPECT_EQ(ASCIIToUTF16(expected_origin_identifier), origin_identifier);
  EXPECT_EQ(ASCIIToUTF16(expected_database_name), database_name);
  EXPECT_EQ(ASCIIToUTF16(expected_sqlite_suffix), sqlite_suffix);
}

namespace webkit_database {

// Test DatabaseUtil::CrackVfsFilePath on various inputs.
TEST(DatabaseUtilTest, CrackVfsFilePathTest) {
  TestVfsFilePath(true, "origin/#", "origin", "", "");
  TestVfsFilePath(true, "origin/#suffix", "origin", "", "suffix");
  TestVfsFilePath(true, "origin/db_name#", "origin", "db_name", "");
  TestVfsFilePath(true, "origin/db_name#suffix", "origin", "db_name", "suffix");
  TestVfsFilePath(false, "origindb_name#");
  TestVfsFilePath(false, "origindb_name#suffix");
  TestVfsFilePath(false, "origin/db_name");
  TestVfsFilePath(false, "origin#db_name/suffix");
  TestVfsFilePath(false, "/db_name#");
  TestVfsFilePath(false, "/db_name#suffix");
}

}  // namespace webkit_database
