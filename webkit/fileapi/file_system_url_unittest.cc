// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_url.h"

#include "base/file_path.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/external_mount_points.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/isolated_context.h"
#include "webkit/fileapi/syncable/syncable_file_system_util.h"

#define FPL FILE_PATH_LITERAL

#if defined(FILE_PATH_USES_DRIVE_LETTERS)
#define DRIVE FPL("C:")
#else
#define DRIVE FPL("/a/")
#endif

namespace fileapi {

namespace {

FileSystemURL CreateFileSystemURL(const std::string& url_string) {
  FileSystemURL url = FileSystemURL::CreateForTest(GURL(url_string));
  switch (url.type()) {
    case kFileSystemTypeExternal:
      return ExternalMountPoints::GetSystemInstance()->
          CreateCrackedFileSystemURL(url.origin(), url.type(), url.path());
    case kFileSystemTypeIsolated:
      return IsolatedContext::GetInstance()->CreateCrackedFileSystemURL(
          url.origin(), url.type(), url.path());
    default:
      return url;
  }
}

FileSystemURL CreateExternalFileSystemURL(const GURL& origin,
                                          FileSystemType type,
                                          const FilePath& path) {
  return ExternalMountPoints::GetSystemInstance()->CreateCrackedFileSystemURL(
      origin, type, path);
}

std::string NormalizedUTF8Path(const FilePath& path) {
  return path.NormalizePathSeparators().AsUTF8Unsafe();
}

}  // namespace

TEST(FileSystemURLTest, ParsePersistent) {
  FileSystemURL url = CreateFileSystemURL(
     "filesystem:http://chromium.org/persistent/directory/file");
  ASSERT_TRUE(url.is_valid());
  EXPECT_EQ("http://chromium.org/", url.origin().spec());
  EXPECT_EQ(kFileSystemTypePersistent, url.type());
  EXPECT_EQ(FPL("file"), VirtualPath::BaseName(url.path()).value());
  EXPECT_EQ(FPL("directory"), url.path().DirName().value());
}

TEST(FileSystemURLTest, ParseTemporary) {
  FileSystemURL url = CreateFileSystemURL(
      "filesystem:http://chromium.org/temporary/directory/file");
  ASSERT_TRUE(url.is_valid());
  EXPECT_EQ("http://chromium.org/", url.origin().spec());
  EXPECT_EQ(kFileSystemTypeTemporary, url.type());
  EXPECT_EQ(FPL("file"), VirtualPath::BaseName(url.path()).value());
  EXPECT_EQ(FPL("directory"), url.path().DirName().value());
}

TEST(FileSystemURLTest, EnsureFilePathIsRelative) {
  FileSystemURL url = CreateFileSystemURL(
      "filesystem:http://chromium.org/temporary/////directory/file");
  ASSERT_TRUE(url.is_valid());
  EXPECT_EQ("http://chromium.org/", url.origin().spec());
  EXPECT_EQ(kFileSystemTypeTemporary, url.type());
  EXPECT_EQ(FPL("file"), VirtualPath::BaseName(url.path()).value());
  EXPECT_EQ(FPL("directory"), url.path().DirName().value());
  EXPECT_FALSE(url.path().IsAbsolute());
}

TEST(FileSystemURLTest, RejectBadSchemes) {
  EXPECT_FALSE(CreateFileSystemURL("http://chromium.org/").is_valid());
  EXPECT_FALSE(CreateFileSystemURL("https://chromium.org/").is_valid());
  EXPECT_FALSE(CreateFileSystemURL("file:///foo/bar").is_valid());
  EXPECT_FALSE(CreateFileSystemURL("foobar:///foo/bar").is_valid());
}

TEST(FileSystemURLTest, UnescapePath) {
  FileSystemURL url = CreateFileSystemURL(
      "filesystem:http://chromium.org/persistent/%7Echromium/space%20bar");
  ASSERT_TRUE(url.is_valid());
  EXPECT_EQ(FPL("space bar"), VirtualPath::BaseName(url.path()).value());
  EXPECT_EQ(FPL("~chromium"), url.path().DirName().value());
}

TEST(FileSystemURLTest, RejectBadType) {
  EXPECT_FALSE(CreateFileSystemURL(
      "filesystem:http://c.org/foobar/file").is_valid());
}

TEST(FileSystemURLTest, RejectMalformedURL) {
  EXPECT_FALSE(CreateFileSystemURL("filesystem:///foobar/file").is_valid());
  EXPECT_FALSE(CreateFileSystemURL("filesystem:foobar/file").is_valid());
}

TEST(FileSystemURLTest, CompareURLs) {
  const GURL urls[] = {
      GURL("filesystem:http://chromium.org/temporary/dir a/file a"),
      GURL("filesystem:http://chromium.org/temporary/dir a/file a"),
      GURL("filesystem:http://chromium.org/temporary/dir a/file b"),
      GURL("filesystem:http://chromium.org/temporary/dir a/file aa"),
      GURL("filesystem:http://chromium.org/temporary/dir b/file a"),
      GURL("filesystem:http://chromium.org/temporary/dir aa/file b"),
      GURL("filesystem:http://chromium.com/temporary/dir a/file a"),
      GURL("filesystem:https://chromium.org/temporary/dir a/file a")
  };

  FileSystemURL::Comparator compare;
  for (size_t i = 0; i < arraysize(urls); ++i) {
    for (size_t j = 0; j < arraysize(urls); ++j) {
      SCOPED_TRACE(testing::Message() << i << " < " << j);
      EXPECT_EQ(urls[i] < urls[j],
                compare(FileSystemURL::CreateForTest(urls[i]),
                        FileSystemURL::CreateForTest(urls[j])));
    }
  }

  const FileSystemURL a = CreateFileSystemURL(
      "filesystem:http://chromium.org/temporary/dir a/file a");
  const FileSystemURL b = CreateFileSystemURL(
      "filesystem:http://chromium.org/persistent/dir a/file a");
  EXPECT_EQ(a.type() < b.type(), compare(a, b));
  EXPECT_EQ(b.type() < a.type(), compare(b, a));
}

TEST(FileSystemURLTest, WithPath) {
  const GURL kURL("filesystem:http://chromium.org/temporary/dir");
  const FilePath::StringType paths[] = {
      FPL("dir a"),
      FPL("dir a/file 1"),
      FPL("dir a/dir b"),
      FPL("dir a/dir b/file 2"),
  };

  const FileSystemURL base = FileSystemURL::CreateForTest(kURL);
  for (size_t i = 0; i < arraysize(paths); ++i) {
    const FileSystemURL url = base.WithPath(FilePath(paths[i]));
    EXPECT_EQ(paths[i], url.path().value());
    EXPECT_EQ(base.origin().spec(), url.origin().spec());
    EXPECT_EQ(base.type(), url.type());
    EXPECT_EQ(base.mount_type(), url.mount_type());
    EXPECT_EQ(base.filesystem_id(), url.filesystem_id());
  }
}

TEST(FileSystemURLTest, WithPathForExternal) {
  const std::string kId = "foo";
  ScopedExternalFileSystem scoped_fs(kId, kFileSystemTypeSyncable, FilePath());
  const FilePath kVirtualRoot = scoped_fs.GetVirtualRootPath();

  const FilePath::CharType kBasePath[] = FPL("dir");
  const FilePath::StringType paths[] = {
      FPL("dir a"),
      FPL("dir a/file 1"),
      FPL("dir a/dir b"),
      FPL("dir a/dir b/file 2"),
  };

  const FileSystemURL base = FileSystemURL::CreateForTest(
      GURL("http://example.com/"),
      kFileSystemTypeExternal,
      kVirtualRoot.Append(kBasePath));

  for (size_t i = 0; i < arraysize(paths); ++i) {
    const FileSystemURL url = base.WithPath(FilePath(paths[i]));
    EXPECT_EQ(paths[i], url.path().value());
    EXPECT_EQ(base.origin().spec(), url.origin().spec());
    EXPECT_EQ(base.type(), url.type());
    EXPECT_EQ(base.mount_type(), url.mount_type());
    EXPECT_EQ(base.filesystem_id(), url.filesystem_id());
  }
}

TEST(FileSystemURLTest, IsParent) {
  ScopedExternalFileSystem scoped1("foo", kFileSystemTypeSyncable, FilePath());
  ScopedExternalFileSystem scoped2("bar", kFileSystemTypeSyncable, FilePath());

  const std::string root1 = GetFileSystemRootURI(
      GURL("http://example.com"), kFileSystemTypeTemporary).spec();
  const std::string root2 = GetSyncableFileSystemRootURI(
      GURL("http://example.com"), "foo").spec();
  const std::string root3 = GetSyncableFileSystemRootURI(
      GURL("http://example.com"), "bar").spec();
  const std::string root4 = GetFileSystemRootURI(
      GURL("http://chromium.org"), kFileSystemTypeTemporary).spec();

  const std::string parent("dir");
  const std::string child("dir/child");
  const std::string other("other");

  // True cases.
  EXPECT_TRUE(CreateFileSystemURL(root1 + parent).IsParent(
      CreateFileSystemURL(root1 + child)));
  EXPECT_TRUE(CreateFileSystemURL(root2 + parent).IsParent(
      CreateFileSystemURL(root2 + child)));

  // False cases: the path is not a child.
  EXPECT_FALSE(CreateFileSystemURL(root1 + parent).IsParent(
      CreateFileSystemURL(root1 + other)));
  EXPECT_FALSE(CreateFileSystemURL(root1 + parent).IsParent(
      CreateFileSystemURL(root1 + parent)));
  EXPECT_FALSE(CreateFileSystemURL(root1 + child).IsParent(
      CreateFileSystemURL(root1 + parent)));

  // False case: different types.
  EXPECT_FALSE(CreateFileSystemURL(root1 + parent).IsParent(
      CreateFileSystemURL(root2 + child)));

  // False case: different filesystem ID.
  EXPECT_FALSE(CreateFileSystemURL(root1 + parent).IsParent(
      CreateFileSystemURL(root3 + child)));

  // False case: different origins.
  EXPECT_FALSE(CreateFileSystemURL(root1 + parent).IsParent(
      CreateFileSystemURL(root4 + child)));
}

TEST(FileSystemURLTest, DebugString) {
  const GURL kOrigin("http://example.com");
  const FilePath kPath(FPL("dir/file"));

  const FileSystemURL kURL1 = FileSystemURL::CreateForTest(
      kOrigin, kFileSystemTypeTemporary, kPath);
  EXPECT_EQ("filesystem:http://example.com/temporary/" +
            NormalizedUTF8Path(kPath),
            kURL1.DebugString());

  const FilePath kRoot(DRIVE FPL("/root"));
  ScopedExternalFileSystem scoped_fs("foo",
                                     kFileSystemTypeNativeLocal,
                                     kRoot.NormalizePathSeparators());
  const FileSystemURL kURL2(CreateExternalFileSystemURL(
      kOrigin,
      kFileSystemTypeExternal,
      scoped_fs.GetVirtualRootPath().Append(kPath)));
  EXPECT_EQ("filesystem:http://example.com/external/" +
            NormalizedUTF8Path(scoped_fs.GetVirtualRootPath().Append(kPath)) +
            " (NativeLocal@foo:" +
            NormalizedUTF8Path(kRoot.Append(kPath)) + ")",
            kURL2.DebugString());
}

}  // namespace fileapi
