// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/file_util.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/media/picasa/pmp_column_reader.h"
#include "webkit/fileapi/media/picasa/pmp_constants.h"
#include "webkit/fileapi/media/picasa/pmp_table_reader.h"
#include "webkit/fileapi/media/picasa/pmp_test_helper.h"

namespace {

using picasaimport::PmpTestHelper;

TEST(PmpTableReaderTest, RowCountAndFieldType) {
  PmpTestHelper test_helper;
  ASSERT_TRUE(test_helper.Init());

  std::string table_name = "testtable";

  std::vector<std::string> column_names;
  column_names.push_back("strings");
  column_names.push_back("uint32s");
  column_names.push_back("doubles");

  const std::vector<std::string> strings_vector(10, "Hello");
  const std::vector<uint32> uint32s_vector(30, 42);
  const std::vector<double> doubles_vector(20, 0.5);

  picasaimport::PmpFieldType column_field_types[] = {
    picasaimport::PMP_TYPE_STRING,
    picasaimport::PMP_TYPE_UINT32,
    picasaimport::PMP_TYPE_DOUBLE64
  };

  const uint32 max_rows = uint32s_vector.size();

  base::FilePath indicator_path = test_helper.GetTempDirPath().Append(
      base::FilePath::FromUTF8Unsafe(table_name + "_0"));

  ASSERT_EQ(0, file_util::WriteFile(indicator_path, NULL, 0));
  // Write three column files, one each for strings, uint32s, and doubles.

  ASSERT_TRUE(test_helper.WriteColumnFileFromVector(
      table_name, column_names[0], column_field_types[0], strings_vector));

  ASSERT_TRUE(test_helper.WriteColumnFileFromVector(
      table_name, column_names[1], column_field_types[1], uint32s_vector));

  ASSERT_TRUE(test_helper.WriteColumnFileFromVector(
      table_name, column_names[2], column_field_types[2], doubles_vector));

  picasaimport::PmpTableReader table_reader;
  ASSERT_TRUE(table_reader.Init(
      table_name, test_helper.GetTempDirPath(), column_names));

  EXPECT_EQ(max_rows, table_reader.RowCount());

  const std::vector<const picasaimport::PmpColumnReader*> column_readers =
      table_reader.GetColumns();

  for (int i = 0; i < 3; i++) {
    EXPECT_EQ(column_field_types[i], column_readers[i]->field_type());
  }
}

}  // namespace
