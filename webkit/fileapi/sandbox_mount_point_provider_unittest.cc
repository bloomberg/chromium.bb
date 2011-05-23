// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/sandbox_mount_point_provider.h"

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_callback_factory.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/scoped_temp_dir.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/file_system_util.h"

namespace fileapi {

class MockFileSystemPathManager : public FileSystemPathManager {
 public:
  MockFileSystemPathManager(const FilePath& filesystem_path)
      : FileSystemPathManager(base::MessageLoopProxy::CreateForCurrentThread(),
                              filesystem_path, NULL, false, true) {}
};

class SandboxMountPointProviderOriginEnumeratorTest : public testing::Test {
 public:
  void SetUp() {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    path_manager_.reset(new MockFileSystemPathManager(data_dir_.path()));
    enumerator_.reset(
        path_manager_->sandbox_provider()->CreateOriginEnumerator());
  }

  SandboxMountPointProvider::OriginEnumerator* enumerator() const {
    return enumerator_.get();
  }

 protected:
  void CreateOriginTypeDirectory(const GURL& origin,
                                 fileapi::FileSystemType type) {
    FilePath target = path_manager_->sandbox_provider()->
        GetBaseDirectoryForOriginAndType(origin, type, true);
    file_util::CreateDirectory(target);
    ASSERT_TRUE(file_util::DirectoryExists(target));
  }

  ScopedTempDir data_dir_;
  scoped_ptr<FileSystemPathManager> path_manager_;
  scoped_ptr<SandboxMountPointProvider::OriginEnumerator> enumerator_;
};

TEST_F(SandboxMountPointProviderOriginEnumeratorTest, Empty) {
  ASSERT_TRUE(enumerator()->Next().is_empty());
}

TEST_F(SandboxMountPointProviderOriginEnumeratorTest, EnumerateOrigins) {
  const char* temporary_origins[] = {
    "http://www.bar.com/",
    "http://www.foo.com/",
    "http://www.foo.com:1/",
    "http://www.example.com:8080/",
    "http://www.google.com:80/",
  };
  const char* persistent_origins[] = {
    "http://www.bar.com/",
    "http://www.foo.com:8080/",
    "http://www.foo.com:80/",
  };
  size_t temporary_size = ARRAYSIZE_UNSAFE(temporary_origins);
  size_t persistent_size = ARRAYSIZE_UNSAFE(persistent_origins);
  std::set<GURL> temporary_set, persistent_set;
  for (size_t i = 0; i < temporary_size; ++i) {
    CreateOriginTypeDirectory(GURL(temporary_origins[i]),
        fileapi::kFileSystemTypeTemporary);
    temporary_set.insert(GURL(temporary_origins[i]));
  }
  for (size_t i = 0; i < persistent_size; ++i) {
    CreateOriginTypeDirectory(GURL(persistent_origins[i]),
        kFileSystemTypePersistent);
    persistent_set.insert(GURL(persistent_origins[i]));
  }

  size_t temporary_actual_size = 0;
  size_t persistent_actual_size = 0;
  GURL current;
  while (!(current = enumerator()->Next()).is_empty()) {
    SCOPED_TRACE(testing::Message() << "EnumerateOrigin " << current.spec());
    if (enumerator()->HasFileSystemType(kFileSystemTypeTemporary)) {
      ASSERT_TRUE(temporary_set.find(current) != temporary_set.end());
      ++temporary_actual_size;
    }
    if (enumerator()->HasFileSystemType(kFileSystemTypePersistent)) {
      ASSERT_TRUE(persistent_set.find(current) != persistent_set.end());
      ++persistent_actual_size;
    }
  }

  ASSERT_EQ(temporary_size, temporary_actual_size);
  ASSERT_EQ(persistent_size, persistent_actual_size);
}

}  // namespace fileapi
