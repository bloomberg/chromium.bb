// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "FileInputType.h"

#include "core/fileapi/FileList.h"
#include <gtest/gtest.h>

namespace blink {

TEST(FileInputTypeTest, createFileList)
{
    Vector<FileChooserFileInfo> files;

    // Natvie file.
    files.append(FileChooserFileInfo(
        "/native/path/native-file",
        "display-name"));

    // Non-native file.
    KURL url(ParsedURLStringTag(), "filesystem:http://example.com/isolated/hash/non-native-file");
    FileMetadata metadata;
    metadata.length = 64;
    metadata.modificationTime = 24 * 60 * 60 /* sec */;
    files.append(FileChooserFileInfo(url, metadata));


    FileList* list = FileInputType::createFileList(files, false);
    ASSERT_TRUE(list);
    ASSERT_EQ(2u, list->length());

    EXPECT_EQ("/native/path/native-file", list->item(0)->path());
    EXPECT_EQ("display-name", list->item(0)->name());
    EXPECT_TRUE(list->item(0)->fileSystemURL().isEmpty());

    EXPECT_TRUE(list->item(1)->path().isEmpty());
    EXPECT_EQ("non-native-file", list->item(1)->name());
    EXPECT_EQ(url, list->item(1)->fileSystemURL());
    EXPECT_EQ(64u, list->item(1)->size());
    EXPECT_EQ(24 * 60 * 60 * 1000 /* ms */, list->item(1)->lastModified());
}

} // namespace blink
