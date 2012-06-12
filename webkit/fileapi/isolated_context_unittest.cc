// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/isolated_context.h"

#include <string>

#include "base/basictypes.h"
#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

#define FPL(x) FILE_PATH_LITERAL(x)

#if defined(FILE_PATH_USES_DRIVE_LETTERS)
#define DRIVE FPL("C:")
#else
#define DRIVE
#endif

namespace fileapi {

namespace {

const FilePath kTestPaths[] = {
  FilePath(DRIVE FPL("/a/b")),
  FilePath(DRIVE FPL("/c/d/e")),
  FilePath(DRIVE FPL("/h/")),
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
  FilePath(DRIVE FPL("\\foo\\bar")),
#endif
};

}  // namespace

class IsolatedContextTest : public testing::Test {
 public:
  IsolatedContextTest() {
    for (size_t i = 0; i < arraysize(kTestPaths); ++i)
      fileset_.insert(kTestPaths[i].NormalizePathSeparators());
  }

  void SetUp() {
    id_ = IsolatedContext::GetInstance()->RegisterIsolatedFileSystem(fileset_);
    ASSERT_FALSE(id_.empty());
  }

  void TearDown() {
    IsolatedContext::GetInstance()->RevokeIsolatedFileSystem(id_);
  }

  IsolatedContext* isolated_context() const {
    return IsolatedContext::GetInstance();
  }

 protected:
  std::string id_;
  std::set<FilePath> fileset_;

 private:
  DISALLOW_COPY_AND_ASSIGN(IsolatedContextTest);
};

TEST_F(IsolatedContextTest, RegisterAndRevokeTest) {
  // See if the returned top-level entries match with what we registered.
  std::vector<FilePath> toplevels;
  ASSERT_TRUE(isolated_context()->GetTopLevelPaths(id_, &toplevels));
  ASSERT_EQ(fileset_.size(), toplevels.size());
  for (size_t i = 0; i < toplevels.size(); ++i) {
    ASSERT_TRUE(fileset_.find(toplevels[i]) != fileset_.end());
  }

  // See if the basename of each registered kTestPaths (that is what we
  // register in SetUp() by RegisterIsolatedFileSystem) is properly cracked as
  // a valid virtual path in the isolated filesystem.
  for (size_t i = 0; i < arraysize(kTestPaths); ++i) {
    FilePath virtual_path = isolated_context()->CreateVirtualPath(
        id_, kTestPaths[i].BaseName());
    std::string cracked_id;
    FilePath root_path, cracked_path;
    ASSERT_TRUE(isolated_context()->CrackIsolatedPath(
        virtual_path, &cracked_id, &root_path, &cracked_path));
    ASSERT_EQ(kTestPaths[i].NormalizePathSeparators().value(),
              cracked_path.value());
    ASSERT_TRUE(fileset_.find(root_path.NormalizePathSeparators())
                != fileset_.end());
    ASSERT_EQ(id_, cracked_id);
  }

  // Revoking the current one and registering a new (empty) one.
  isolated_context()->RevokeIsolatedFileSystem(id_);
  std::string id2 = isolated_context()->RegisterIsolatedFileSystem(
      std::set<FilePath>());

  // Make sure the GetTopLevelPaths returns true only for the new one.
  ASSERT_TRUE(isolated_context()->GetTopLevelPaths(id2, &toplevels));
  ASSERT_FALSE(isolated_context()->GetTopLevelPaths(id_, &toplevels));

  isolated_context()->RevokeIsolatedFileSystem(id2);
}

TEST_F(IsolatedContextTest, CrackWithRelativePaths) {
  const struct {
    FilePath::StringType path;
    bool valid;
  } relatives[] = {
    { FPL("foo"), true },
    { FPL("foo/bar"), true },
    { FPL(".."), false },
    { FPL("foo/.."), false },
    { FPL("foo/../bar"), false },
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
# define SHOULD_FAIL_WITH_WIN_SEPARATORS false
#else
# define SHOULD_FAIL_WITH_WIN_SEPARATORS true
#endif
    { FPL("foo\\..\\baz"), SHOULD_FAIL_WITH_WIN_SEPARATORS },
    { FPL("foo/..\\baz"), SHOULD_FAIL_WITH_WIN_SEPARATORS },
  };

  for (size_t i = 0; i < arraysize(kTestPaths); ++i) {
    for (size_t j = 0; j < ARRAYSIZE_UNSAFE(relatives); ++j) {
      SCOPED_TRACE(testing::Message() << "Testing "
                   << kTestPaths[i].value() << " " << relatives[j].path);
      FilePath virtual_path = isolated_context()->CreateVirtualPath(
          id_, kTestPaths[i].BaseName().Append(relatives[j].path));
      std::string cracked_id;
      FilePath root_path, cracked_path;
      if (!relatives[j].valid) {
        ASSERT_FALSE(isolated_context()->CrackIsolatedPath(
            virtual_path, &cracked_id, &root_path, &cracked_path));
        continue;
      }
      ASSERT_TRUE(isolated_context()->CrackIsolatedPath(
          virtual_path, &cracked_id, &root_path, &cracked_path));
      ASSERT_TRUE(fileset_.find(root_path.NormalizePathSeparators())
                  != fileset_.end());
      ASSERT_EQ(kTestPaths[i].Append(relatives[j].path)
                    .NormalizePathSeparators().value(),
                cracked_path.value());
      ASSERT_EQ(id_, cracked_id);
    }
  }
}

TEST_F(IsolatedContextTest, TestWithVirtualRoot) {
  std::string cracked_id;
  FilePath root_path, cracked_path;
  const FilePath root(FPL("/"));

  // Trying to crack virtual root "/" returns true but with empty cracked path
  // as "/" of the isolated filesystem is a pure virtual directory
  // that has no corresponding platform directory.
  FilePath virtual_path = isolated_context()->CreateVirtualPath(id_, root);
  ASSERT_TRUE(isolated_context()->CrackIsolatedPath(
      virtual_path, &cracked_id, &root_path, &cracked_path));
  ASSERT_EQ(FPL(""), cracked_path.value());
  ASSERT_EQ(id_, cracked_id);

  // Trying to crack "/foo" should fail (because "foo" is not the one
  // included in the kTestPaths).
  virtual_path = isolated_context()->CreateVirtualPath(
      id_, FilePath(FPL("foo")));
  ASSERT_FALSE(isolated_context()->CrackIsolatedPath(
      virtual_path, &cracked_id, &root_path, &cracked_path));
}

TEST_F(IsolatedContextTest, Writable) {
  // By default the file system must be read-only.
  ASSERT_FALSE(isolated_context()->IsWritable(id_));

  // Set writable.
  ASSERT_TRUE(isolated_context()->SetWritable(id_, true));
  ASSERT_TRUE(isolated_context()->IsWritable(id_));

  // Set non-writable.
  ASSERT_TRUE(isolated_context()->SetWritable(id_, false));
  ASSERT_FALSE(isolated_context()->IsWritable(id_));

  // Set writable again, and revoke the filesystem.
  ASSERT_TRUE(isolated_context()->SetWritable(id_, true));
  isolated_context()->RevokeIsolatedFileSystem(id_);

  // IsWritable should return false for non-registered file system.
  ASSERT_FALSE(isolated_context()->IsWritable(id_));
  // SetWritable should also return false for non-registered file system
  // (no matter what value we give).
  ASSERT_FALSE(isolated_context()->SetWritable(id_, true));
  ASSERT_FALSE(isolated_context()->SetWritable(id_, false));
}

}  // namespace fileapi
