// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/string_piece.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/data_pack.h"

namespace ui {

TEST(DataPackTest, Load) {
  FilePath data_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &data_path);
  data_path = data_path.Append(
      FILE_PATH_LITERAL("ui/base/test/data/data_pack_unittest/sample.pak"));

  DataPack pack;
  ASSERT_TRUE(pack.Load(data_path));

  base::StringPiece data;
  ASSERT_TRUE(pack.GetStringPiece(4, &data));
  EXPECT_EQ("this is id 4", data);
  ASSERT_TRUE(pack.GetStringPiece(6, &data));
  EXPECT_EQ("this is id 6", data);

  // Try reading zero-length data blobs, just in case.
  ASSERT_TRUE(pack.GetStringPiece(1, &data));
  EXPECT_EQ(0U, data.length());
  ASSERT_TRUE(pack.GetStringPiece(10, &data));
  EXPECT_EQ(0U, data.length());

  // Try looking up an invalid key.
  ASSERT_FALSE(pack.GetStringPiece(140, &data));
}

TEST(DataPackTest, Write) {
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath file = dir.path().Append(FILE_PATH_LITERAL("data.pak"));

  std::string one("one");
  std::string two("two");
  std::string three("three");
  std::string four("four");
  std::string fifteen("fifteen");

  std::map<uint32, base::StringPiece> resources;
  resources.insert(std::make_pair(1, base::StringPiece(one)));
  resources.insert(std::make_pair(2, base::StringPiece(two)));
  resources.insert(std::make_pair(15, base::StringPiece(fifteen)));
  resources.insert(std::make_pair(3, base::StringPiece(three)));
  resources.insert(std::make_pair(4, base::StringPiece(four)));
  ASSERT_TRUE(DataPack::WritePack(file, resources));

  // Now try to read the data back in.
  DataPack pack;
  ASSERT_TRUE(pack.Load(file));

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

}  // namespace ui
