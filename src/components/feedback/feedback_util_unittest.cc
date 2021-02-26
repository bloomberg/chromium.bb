// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/feedback_util.h"

#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/rand_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feedback_util {

// Note: This file is excluded from win build.
// See https://crbug.com/1119560.
class FeedbackUtilTest : public ::testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::ScopedTempDir temp_dir_;
};

TEST_F(FeedbackUtilTest, ReadEndOfFileEmpty) {
  std::string read_data("should be erased");

  base::FilePath file_path = temp_dir_.GetPath().Append("test_empty.txt");

  WriteFile(file_path, "", 0);

  EXPECT_TRUE(feedback_util::ReadEndOfFile(file_path, 10, &read_data));
  EXPECT_EQ(0u, read_data.length());
}

TEST_F(FeedbackUtilTest, ReadEndOfFileSmall) {
  const char kTestData[] = "0123456789";  // Length of 10
  std::string read_data;

  base::FilePath file_path = temp_dir_.GetPath().Append("test_small.txt");

  WriteFile(file_path, kTestData, strlen(kTestData));

  read_data.clear();
  EXPECT_TRUE(feedback_util::ReadEndOfFile(file_path, 15, &read_data));
  EXPECT_EQ(kTestData, read_data);

  read_data.clear();
  EXPECT_TRUE(feedback_util::ReadEndOfFile(file_path, 10, &read_data));
  EXPECT_EQ(kTestData, read_data);

  read_data.clear();
  EXPECT_TRUE(feedback_util::ReadEndOfFile(file_path, 2, &read_data));
  EXPECT_EQ("89", read_data);

  read_data.clear();
  EXPECT_TRUE(feedback_util::ReadEndOfFile(file_path, 3, &read_data));
  EXPECT_EQ("789", read_data);

  read_data.clear();
  EXPECT_TRUE(feedback_util::ReadEndOfFile(file_path, 5, &read_data));
  EXPECT_EQ("56789", read_data);
}

TEST_F(FeedbackUtilTest, ReadEndOfFileWithZeros) {
  const size_t test_size = 10;
  std::string test_data("abcd\0\0\0\0hi", test_size);
  std::string read_data;

  base::FilePath file_path = temp_dir_.GetPath().Append("test_zero.txt");

  WriteFile(file_path, test_data.data(), test_size);

  read_data.clear();
  EXPECT_TRUE(feedback_util::ReadEndOfFile(file_path, 15, &read_data));
  EXPECT_EQ(test_data, read_data);

  read_data.clear();
  EXPECT_TRUE(feedback_util::ReadEndOfFile(file_path, 10, &read_data));
  EXPECT_EQ(test_data, read_data);

  read_data.clear();
  EXPECT_TRUE(feedback_util::ReadEndOfFile(file_path, 2, &read_data));
  EXPECT_EQ(test_data.substr(test_size - 2, 2), read_data);

  read_data.clear();
  EXPECT_TRUE(feedback_util::ReadEndOfFile(file_path, 3, &read_data));
  EXPECT_EQ(test_data.substr(test_size - 3, 3), read_data);

  read_data.clear();
  EXPECT_TRUE(feedback_util::ReadEndOfFile(file_path, 5, &read_data));
  EXPECT_EQ(test_data.substr(test_size - 5, 5), read_data);
}

TEST_F(FeedbackUtilTest, ReadEndOfFileMedium) {
  std::string test_data = base::RandBytesAsString(10000);  // 10KB data
  std::string read_data;

  const size_t test_size = test_data.length();

  base::FilePath file_path = temp_dir_.GetPath().Append("test_med.txt");

  WriteFile(file_path, test_data.data(), test_size);

  read_data.clear();
  EXPECT_TRUE(feedback_util::ReadEndOfFile(file_path, 15000, &read_data));
  EXPECT_EQ(test_data, read_data);

  read_data.clear();
  EXPECT_TRUE(feedback_util::ReadEndOfFile(file_path, 10000, &read_data));
  EXPECT_EQ(test_data, read_data);

  read_data.clear();
  EXPECT_TRUE(feedback_util::ReadEndOfFile(file_path, 1000, &read_data));
  EXPECT_EQ(test_data.substr(test_size - 1000, 1000), read_data);

  read_data.clear();
  EXPECT_TRUE(feedback_util::ReadEndOfFile(file_path, 300, &read_data));
  EXPECT_EQ(test_data.substr(test_size - 300, 300), read_data);

  read_data.clear();
  EXPECT_TRUE(feedback_util::ReadEndOfFile(file_path, 175, &read_data));
  EXPECT_EQ(test_data.substr(test_size - 175, 175), read_data);
}

}  // namespace feedback_util
