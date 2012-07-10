// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/isolated_context.h"

#include <string>

#include "base/basictypes.h"
#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/isolated_context.h"

#define FPL(x) FILE_PATH_LITERAL(x)

#if defined(FILE_PATH_USES_DRIVE_LETTERS)
#define DRIVE FPL("C:")
#else
#define DRIVE
#endif

namespace fileapi {

typedef IsolatedContext::FileInfo FileInfo;

namespace {

const FilePath kTestPaths[] = {
  FilePath(DRIVE FPL("/a/b.txt")),
  FilePath(DRIVE FPL("/c/d/e")),
  FilePath(DRIVE FPL("/h/")),
  FilePath(DRIVE FPL("/")),
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
  FilePath(DRIVE FPL("\\foo\\bar")),
  FilePath(DRIVE FPL("\\")),
#endif
  // For duplicated base name test.
  FilePath(DRIVE FPL("/")),
  FilePath(DRIVE FPL("/f/e")),
  FilePath(DRIVE FPL("/f/b.txt")),
};

}  // namespace

class IsolatedContextTest : public testing::Test {
 public:
  IsolatedContextTest() {
    for (size_t i = 0; i < arraysize(kTestPaths); ++i)
      fileset_.insert(kTestPaths[i].NormalizePathSeparators());
  }

  void SetUp() {
    IsolatedContext::FileInfoSet files;
    for (size_t i = 0; i < arraysize(kTestPaths); ++i)
      names_.push_back(files.AddPath(kTestPaths[i].NormalizePathSeparators()));
    id_ = IsolatedContext::GetInstance()->RegisterFileSystem(files);
    ASSERT_FALSE(id_.empty());
  }

  void TearDown() {
    IsolatedContext::GetInstance()->RevokeFileSystem(id_);
  }

  IsolatedContext* isolated_context() const {
    return IsolatedContext::GetInstance();
  }

 protected:
  std::string id_;
  std::multiset<FilePath> fileset_;
  std::vector<std::string> names_;

 private:
  DISALLOW_COPY_AND_ASSIGN(IsolatedContextTest);
};

TEST_F(IsolatedContextTest, RegisterAndRevokeTest) {
  // See if the returned top-level entries match with what we registered.
  std::vector<FileInfo> toplevels;
  ASSERT_TRUE(isolated_context()->GetRegisteredFileInfo(id_, &toplevels));
  ASSERT_EQ(fileset_.size(), toplevels.size());
  for (size_t i = 0; i < toplevels.size(); ++i) {
    ASSERT_TRUE(fileset_.find(toplevels[i].path) != fileset_.end());
  }

  // See if the name of each registered kTestPaths (that is what we
  // register in SetUp() by RegisterFileSystem) is properly cracked as
  // a valid virtual path in the isolated filesystem.
  for (size_t i = 0; i < arraysize(kTestPaths); ++i) {
    FilePath virtual_path = isolated_context()->CreateVirtualRootPath(id_)
        .AppendASCII(names_[i]);
    std::string cracked_id;
    FileInfo root_info;
    FilePath cracked_path;
    ASSERT_TRUE(isolated_context()->CrackIsolatedPath(
        virtual_path, &cracked_id, &root_info, &cracked_path));
    ASSERT_EQ(kTestPaths[i].NormalizePathSeparators().value(),
              cracked_path.value());
    ASSERT_TRUE(fileset_.find(root_info.path.NormalizePathSeparators())
                != fileset_.end());
    ASSERT_EQ(id_, cracked_id);
  }

  // Revoking the current one and registering a new (empty) one.
  isolated_context()->RevokeFileSystem(id_);
  std::string id2 = isolated_context()->RegisterFileSystem(
      IsolatedContext::FileInfoSet());

  // Make sure the GetRegisteredFileInfo returns true only for the new one.
  ASSERT_TRUE(isolated_context()->GetRegisteredFileInfo(id2, &toplevels));
  ASSERT_FALSE(isolated_context()->GetRegisteredFileInfo(id_, &toplevels));

  isolated_context()->RevokeFileSystem(id2);
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
      FilePath virtual_path = isolated_context()->CreateVirtualRootPath(id_)
          .AppendASCII(names_[i]).Append(relatives[j].path);
      std::string cracked_id;
      FileInfo root_info;
      FilePath cracked_path;
      if (!relatives[j].valid) {
        ASSERT_FALSE(isolated_context()->CrackIsolatedPath(
            virtual_path, &cracked_id, &root_info, &cracked_path));
        continue;
      }
      ASSERT_TRUE(isolated_context()->CrackIsolatedPath(
          virtual_path, &cracked_id, &root_info, &cracked_path));
      ASSERT_TRUE(fileset_.find(root_info.path.NormalizePathSeparators())
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
  FilePath cracked_path;

  // Trying to crack virtual root "/" returns true but with empty cracked path
  // as "/" of the isolated filesystem is a pure virtual directory
  // that has no corresponding platform directory.
  FilePath virtual_path = isolated_context()->CreateVirtualRootPath(id_);
  ASSERT_TRUE(isolated_context()->CrackIsolatedPath(
      virtual_path, &cracked_id, NULL, &cracked_path));
  ASSERT_EQ(FPL(""), cracked_path.value());
  ASSERT_EQ(id_, cracked_id);

  // Trying to crack "/foo" should fail (because "foo" is not the one
  // included in the kTestPaths).
  virtual_path = isolated_context()->CreateVirtualRootPath(
      id_).AppendASCII("foo");
  ASSERT_FALSE(isolated_context()->CrackIsolatedPath(
      virtual_path, &cracked_id, NULL, &cracked_path));
}

}  // namespace fileapi
