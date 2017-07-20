// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/data_pack.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/data_pack_literal.h"
#include "ui/base/ui_base_paths.h"

namespace ui {

class DataPackTest
    : public testing::TestWithParam<DataPack::TextEncodingType> {
 public:
  DataPackTest() {}
};

TEST(DataPackTest, LoadFromPath) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::FilePath data_path =
      dir.GetPath().Append(FILE_PATH_LITERAL("sample.pak"));

  // Dump contents into the pak file.
  ASSERT_EQ(base::WriteFile(data_path, kSamplePakContentsV4, kSamplePakSizeV4),
            static_cast<int>(kSamplePakSizeV4));

  // Load the file through the data pack API.
  DataPack pack(SCALE_FACTOR_100P);
  ASSERT_TRUE(pack.LoadFromPath(data_path));

  base::StringPiece data;
  ASSERT_TRUE(pack.HasResource(4));
  ASSERT_TRUE(pack.GetStringPiece(4, &data));
  EXPECT_EQ("this is id 4", data);
  ASSERT_TRUE(pack.HasResource(6));
  ASSERT_TRUE(pack.GetStringPiece(6, &data));
  EXPECT_EQ("this is id 6", data);

  // Try reading zero-length data blobs, just in case.
  ASSERT_TRUE(pack.GetStringPiece(1, &data));
  EXPECT_EQ(0U, data.length());
  ASSERT_TRUE(pack.GetStringPiece(10, &data));
  EXPECT_EQ(0U, data.length());

  // Try looking up an invalid key.
  ASSERT_FALSE(pack.HasResource(140));
  ASSERT_FALSE(pack.GetStringPiece(140, &data));
}

TEST(DataPackTest, LoadFromFile) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::FilePath data_path =
      dir.GetPath().Append(FILE_PATH_LITERAL("sample.pak"));

  // Dump contents into the pak file.
  ASSERT_EQ(base::WriteFile(data_path, kSamplePakContentsV4, kSamplePakSizeV4),
            static_cast<int>(kSamplePakSizeV4));

  base::File file(data_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(file.IsValid());

  // Load the file through the data pack API.
  DataPack pack(SCALE_FACTOR_100P);
  ASSERT_TRUE(pack.LoadFromFile(std::move(file)));

  base::StringPiece data;
  ASSERT_TRUE(pack.HasResource(4));
  ASSERT_TRUE(pack.GetStringPiece(4, &data));
  EXPECT_EQ("this is id 4", data);
  ASSERT_TRUE(pack.HasResource(6));
  ASSERT_TRUE(pack.GetStringPiece(6, &data));
  EXPECT_EQ("this is id 6", data);

  // Try reading zero-length data blobs, just in case.
  ASSERT_TRUE(pack.GetStringPiece(1, &data));
  EXPECT_EQ(0U, data.length());
  ASSERT_TRUE(pack.GetStringPiece(10, &data));
  EXPECT_EQ(0U, data.length());

  // Try looking up an invalid key.
  ASSERT_FALSE(pack.HasResource(140));
  ASSERT_FALSE(pack.GetStringPiece(140, &data));
}

TEST(DataPackTest, LoadFromFileRegion) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::FilePath data_path =
      dir.GetPath().Append(FILE_PATH_LITERAL("sample.pak"));

  // Construct a file which has a non page-aligned zero-filled header followed
  // by the actual pak file content.
  const char kPadding[5678] = {0};
  ASSERT_EQ(static_cast<int>(sizeof(kPadding)),
            base::WriteFile(data_path, kPadding, sizeof(kPadding)));
  ASSERT_TRUE(
      base::AppendToFile(data_path, kSamplePakContentsV4, kSamplePakSizeV4));

  base::File file(data_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(file.IsValid());

  // Load the file through the data pack API.
  DataPack pack(SCALE_FACTOR_100P);
  base::MemoryMappedFile::Region region = {sizeof(kPadding), kSamplePakSizeV4};
  ASSERT_TRUE(pack.LoadFromFileRegion(std::move(file), region));

  base::StringPiece data;
  ASSERT_TRUE(pack.HasResource(4));
  ASSERT_TRUE(pack.GetStringPiece(4, &data));
  EXPECT_EQ("this is id 4", data);
  ASSERT_TRUE(pack.HasResource(6));
  ASSERT_TRUE(pack.GetStringPiece(6, &data));
  EXPECT_EQ("this is id 6", data);

  // Try reading zero-length data blobs, just in case.
  ASSERT_TRUE(pack.GetStringPiece(1, &data));
  EXPECT_EQ(0U, data.length());
  ASSERT_TRUE(pack.GetStringPiece(10, &data));
  EXPECT_EQ(0U, data.length());

  // Try looking up an invalid key.
  ASSERT_FALSE(pack.HasResource(140));
  ASSERT_FALSE(pack.GetStringPiece(140, &data));
}

TEST(DataPackTest, LoadFromBufferV4) {
  DataPack pack(SCALE_FACTOR_100P);

  ASSERT_TRUE(pack.LoadFromBuffer(
      base::StringPiece(kSamplePakContentsV4, kSamplePakSizeV4)));

  base::StringPiece data;
  ASSERT_TRUE(pack.HasResource(4));
  ASSERT_TRUE(pack.GetStringPiece(4, &data));
  EXPECT_EQ("this is id 4", data);
  ASSERT_TRUE(pack.HasResource(6));
  ASSERT_TRUE(pack.GetStringPiece(6, &data));
  EXPECT_EQ("this is id 6", data);

  // Try reading zero-length data blobs, just in case.
  ASSERT_TRUE(pack.GetStringPiece(1, &data));
  EXPECT_EQ(0U, data.length());
  ASSERT_TRUE(pack.GetStringPiece(10, &data));
  EXPECT_EQ(0U, data.length());

  // Try looking up an invalid key.
  ASSERT_FALSE(pack.HasResource(140));
  ASSERT_FALSE(pack.GetStringPiece(140, &data));
}

TEST(DataPackTest, LoadFromBufferV5) {
  DataPack pack(SCALE_FACTOR_100P);

  ASSERT_TRUE(pack.LoadFromBuffer(
      base::StringPiece(kSamplePakContentsV5, kSamplePakSizeV5)));

  base::StringPiece data;
  ASSERT_TRUE(pack.HasResource(4));
  ASSERT_TRUE(pack.GetStringPiece(4, &data));
  EXPECT_EQ("this is id 4", data);
  ASSERT_TRUE(pack.HasResource(6));
  ASSERT_TRUE(pack.GetStringPiece(6, &data));
  EXPECT_EQ("this is id 6", data);

  // Try reading zero-length data blobs, just in case.
  ASSERT_TRUE(pack.GetStringPiece(1, &data));
  EXPECT_EQ(0U, data.length());
  ASSERT_TRUE(pack.GetStringPiece(10, &data));
  EXPECT_EQ("this is id 4", data);

  // Try looking up an invalid key.
  ASSERT_FALSE(pack.HasResource(140));
  ASSERT_FALSE(pack.GetStringPiece(140, &data));
}

INSTANTIATE_TEST_CASE_P(WriteBINARY, DataPackTest, ::testing::Values(
    DataPack::BINARY));
INSTANTIATE_TEST_CASE_P(WriteUTF8, DataPackTest, ::testing::Values(
    DataPack::UTF8));
INSTANTIATE_TEST_CASE_P(WriteUTF16, DataPackTest, ::testing::Values(
    DataPack::UTF16));

TEST(DataPackTest, LoadFileWithTruncatedHeader) {
  base::FilePath data_path;
  ASSERT_TRUE(PathService::Get(UI_DIR_TEST_DATA, &data_path));
  data_path = data_path.AppendASCII("data_pack_unittest/truncated-header.pak");

  DataPack pack(SCALE_FACTOR_100P);
  ASSERT_FALSE(pack.LoadFromPath(data_path));
}

TEST_P(DataPackTest, Write) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::FilePath file = dir.GetPath().Append(FILE_PATH_LITERAL("data.pak"));

  std::string one("one");
  std::string two("two");
  std::string three("three");
  std::string four("four");
  std::string fifteen("fifteen");

  std::map<uint16_t, base::StringPiece> resources;
  resources.insert(std::make_pair(1, base::StringPiece(one)));
  resources.insert(std::make_pair(2, base::StringPiece(two)));
  resources.insert(std::make_pair(15, base::StringPiece(fifteen)));
  resources.insert(std::make_pair(3, base::StringPiece(three)));
  resources.insert(std::make_pair(4, base::StringPiece(four)));
  ASSERT_TRUE(DataPack::WritePack(file, resources, GetParam()));

  // Now try to read the data back in.
  DataPack pack(SCALE_FACTOR_100P);
  ASSERT_TRUE(pack.LoadFromPath(file));
  EXPECT_EQ(pack.GetTextEncodingType(), GetParam());

  base::StringPiece data;
  ASSERT_TRUE(pack.GetStringPiece(1, &data));
  EXPECT_EQ(one, data);
  ASSERT_TRUE(pack.GetStringPiece(2, &data));
  EXPECT_EQ(two, data);
  ASSERT_TRUE(pack.GetStringPiece(3, &data));
  EXPECT_EQ(three, data);
  ASSERT_TRUE(pack.GetStringPiece(4, &data));
  EXPECT_EQ(four, data);
  ASSERT_TRUE(pack.GetStringPiece(15, &data));
  EXPECT_EQ(fifteen, data);
}

#if defined(OS_POSIX)
TEST(DataPackTest, ModifiedWhileUsed) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::FilePath data_path =
      dir.GetPath().Append(FILE_PATH_LITERAL("sample.pak"));

  // Dump contents into the pak file.
  ASSERT_EQ(base::WriteFile(data_path, kSamplePakContentsV4, kSamplePakSizeV4),
            static_cast<int>(kSamplePakSizeV4));

  base::File file(data_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(file.IsValid());

  // Load the file through the data pack API.
  DataPack pack(SCALE_FACTOR_100P);
  ASSERT_TRUE(pack.LoadFromFile(std::move(file)));

  base::StringPiece data;
  ASSERT_TRUE(pack.HasResource(10));
  ASSERT_TRUE(pack.GetStringPiece(10, &data));

  ASSERT_EQ(base::WriteFile(data_path, kSampleCorruptPakContents,
                            kSampleCorruptPakSize),
            static_cast<int>(kSampleCorruptPakSize));

  // Reading asset #10 should now fail as it extends past the end of the file.
  ASSERT_TRUE(pack.HasResource(10));
  ASSERT_FALSE(pack.GetStringPiece(10, &data));
}
#endif

}  // namespace ui
