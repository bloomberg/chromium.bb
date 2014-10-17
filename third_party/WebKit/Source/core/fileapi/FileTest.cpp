// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "File.h"

#include <gtest/gtest.h>

namespace blink {

TEST(FileTest, nativeFile)
{
    File* const file = File::create("/native/path");
    EXPECT_TRUE(file->hasBackingFile());
    EXPECT_EQ("/native/path", file->path());
    EXPECT_TRUE(file->fileSystemURL().isEmpty());
}

TEST(FileTest, blobBackingFile)
{
    const RefPtr<BlobDataHandle> blobDataHandle = BlobDataHandle::create();
    File* const file = File::create("name", 0.0, blobDataHandle);
    EXPECT_FALSE(file->hasBackingFile());
    EXPECT_TRUE(file->path().isEmpty());
    EXPECT_TRUE(file->fileSystemURL().isEmpty());
}

TEST(FileTest, fileSystemFileWithNativeSnapshot)
{
    FileMetadata metadata;
    metadata.platformPath = "/native/snapshot";
    File* const file = File::createForFileSystemFile("name", metadata, File::IsUserVisible);
    EXPECT_TRUE(file->hasBackingFile());
    EXPECT_EQ("/native/snapshot", file->path());
    EXPECT_TRUE(file->fileSystemURL().isEmpty());
}

TEST(FileTest, fileSystemFileWithoutNativeSnapshot)
{
    KURL url(ParsedURLStringTag(), "filesystem:http://example.com/isolated/hash/non-native-file");
    FileMetadata metadata;
    File* const file = File::createForFileSystemFile(url, metadata);
    EXPECT_FALSE(file->hasBackingFile());
    EXPECT_TRUE(file->path().isEmpty());
    EXPECT_EQ(url, file->fileSystemURL());
}

} // namespace blink
