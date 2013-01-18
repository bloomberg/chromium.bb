// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/syncable/syncable_file_system_util.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/external_mount_points.h"
#include "webkit/fileapi/syncable/canned_syncable_file_system.h"
#include "webkit/fileapi/syncable/local_file_sync_context.h"

namespace fileapi {

namespace {

const char kSyncableFileSystemRootURI[] =
    "filesystem:http://www.example.com/external/service/";
const char kNonRegisteredFileSystemRootURI[] =
    "filesystem:http://www.example.com/external/non_registered/";
const char kNonSyncableFileSystemRootURI[] =
    "filesystem:http://www.example.com/temporary/";

const char kOrigin[] = "http://www.example.com/";
const char kServiceName[] = "service";
const FilePath::CharType kPath[] = FILE_PATH_LITERAL("dir/file");

FileSystemURL CreateFileSystemURL(const std::string& url) {
  return FileSystemURL(GURL(url));
}

FilePath CreateNormalizedFilePath(const FilePath::CharType* path) {
  return FilePath(path).NormalizePathSeparators();
}

}  // namespace

TEST(SyncableFileSystemUtilTest, GetSyncableFileSystemRootURI) {
  const GURL root = GetSyncableFileSystemRootURI(GURL(kOrigin), kServiceName);
  EXPECT_TRUE(root.is_valid());
  EXPECT_EQ(GURL(kSyncableFileSystemRootURI), root);
}

TEST(SyncableFileSystemUtilTest, CreateSyncableFileSystemURL) {
  ScopedExternalFileSystem scoped_fs(
      kServiceName, kFileSystemTypeSyncable, FilePath());

  const FilePath path(kPath);
  const FileSystemURL expected_url =
      CreateFileSystemURL(kSyncableFileSystemRootURI + path.AsUTF8Unsafe());
  const FileSystemURL url =
      CreateSyncableFileSystemURL(GURL(kOrigin), kServiceName, path);

  EXPECT_TRUE(url.is_valid());
  EXPECT_EQ(expected_url, url);
}

TEST(SyncableFileSystemUtilTest,
       SerializeAndDesirializeSyncableFileSystemURL) {
  ScopedExternalFileSystem scoped_fs(
      kServiceName, kFileSystemTypeSyncable, FilePath());

  const std::string expected_url_str = kSyncableFileSystemRootURI +
      CreateNormalizedFilePath(kPath).AsUTF8Unsafe();
  const FileSystemURL expected_url = CreateFileSystemURL(expected_url_str);
  const FileSystemURL url =
      CreateSyncableFileSystemURL(GURL(kOrigin), kServiceName, FilePath(kPath));

  std::string serialized;
  EXPECT_TRUE(SerializeSyncableFileSystemURL(url, &serialized));
  EXPECT_EQ(expected_url_str, serialized);

  FileSystemURL deserialized;
  EXPECT_TRUE(DeserializeSyncableFileSystemURL(serialized, &deserialized));
  EXPECT_TRUE(deserialized.is_valid());
  EXPECT_EQ(expected_url, deserialized);
}

TEST(SyncableFileSystemUtilTest,
     FailInSerializingAndDeserializingSyncableFileSystemURL) {
  ScopedExternalFileSystem scoped_fs(
      kServiceName, kFileSystemTypeSyncable, FilePath());

  const FilePath normalized_path = CreateNormalizedFilePath(kPath);
  const std::string non_registered_url =
      kNonRegisteredFileSystemRootURI + normalized_path.AsUTF8Unsafe();
  const std::string non_syncable_url =
      kNonSyncableFileSystemRootURI + normalized_path.AsUTF8Unsafe();

  // Expected to fail in serializing URLs of non-registered filesystem and
  // non-syncable filesystem.
  std::string serialized;
  EXPECT_FALSE(SerializeSyncableFileSystemURL(
      CreateFileSystemURL(non_registered_url), &serialized));
  EXPECT_FALSE(SerializeSyncableFileSystemURL(
      CreateFileSystemURL(non_syncable_url), &serialized));

  // Expected to fail in deserializing a string that represents URLs of
  // non-registered filesystem and non-syncable filesystem.
  FileSystemURL deserialized;
  EXPECT_FALSE(DeserializeSyncableFileSystemURL(
      non_registered_url, &deserialized));
  EXPECT_FALSE(DeserializeSyncableFileSystemURL(
      non_syncable_url, &deserialized));
}

TEST(SyncableFileSystemUtilTest, SerializeBeforeOpenFileSystem) {
  const std::string serialized = kSyncableFileSystemRootURI +
      CreateNormalizedFilePath(kPath).AsUTF8Unsafe();
  FileSystemURL deserialized;
  MessageLoop message_loop;

  // Setting up a full syncable filesystem environment.
  CannedSyncableFileSystem file_system(GURL(kOrigin), kServiceName,
                                       base::MessageLoopProxy::current(),
                                       base::MessageLoopProxy::current());
  file_system.SetUp();
  scoped_refptr<LocalFileSyncContext> sync_context =
      new LocalFileSyncContext(base::MessageLoopProxy::current(),
                               base::MessageLoopProxy::current());

  // Before calling initialization we would not be able to get a valid
  // deserialized URL.
  EXPECT_FALSE(DeserializeSyncableFileSystemURL(serialized, &deserialized));
  EXPECT_FALSE(deserialized.is_valid());

  ASSERT_EQ(SYNC_STATUS_OK,
            file_system.MaybeInitializeFileSystemContext(sync_context));

  // After initialization this should be ok (even before opening the file
  // system).
  EXPECT_TRUE(DeserializeSyncableFileSystemURL(serialized, &deserialized));
  EXPECT_TRUE(deserialized.is_valid());

  // Shutting down.
  file_system.TearDown();
  RevokeSyncableFileSystem(kServiceName);
  sync_context->ShutdownOnUIThread();
  sync_context = NULL;
  MessageLoop::current()->RunUntilIdle();
}

}  // namespace fileapi
