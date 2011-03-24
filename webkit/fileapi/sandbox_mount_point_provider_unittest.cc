// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/sandbox_mount_point_provider.h"

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/ref_counted.h"
#include "base/scoped_callback_factory.h"
#include "base/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/file_system_util.h"

using namespace fileapi;

class SandboxMountPointProviderOriginEnumeratorTest : public testing::Test {
 public:
  void SetUp() {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    enumerator_.reset(new SandboxMountPointProvider::OriginEnumerator(
        data_dir_.path()));
  }

  SandboxMountPointProvider::OriginEnumerator* enumerator() const {
    return enumerator_.get();
  }

 protected:
  void CreateOriginTypeDirectory(const std::string& origin_identifier,
                                 fileapi::FileSystemType type) {
    std::string type_string =
        FileSystemPathManager::GetFileSystemTypeString(type);
    ASSERT_TRUE(!type_string.empty());
    FilePath target = data_dir_.path().AppendASCII(origin_identifier)
                                      .AppendASCII(type_string);
    file_util::CreateDirectory(target);
    ASSERT_TRUE(file_util::DirectoryExists(target));
  }

  ScopedTempDir data_dir_;
  scoped_ptr<SandboxMountPointProvider::OriginEnumerator> enumerator_;
};

TEST_F(SandboxMountPointProviderOriginEnumeratorTest, Empty) {
  ASSERT_TRUE(enumerator()->Next().empty());
}

TEST_F(SandboxMountPointProviderOriginEnumeratorTest, EnumerateOrigins) {
  const char* temporary_origins[] = {
    "http_www.bar.com_0",
    "http_www.foo.com_0",
    "http_www.foo.com_80",
    "http_www.example.com_8080",
    "http_www.google.com_80",
  };
  const char* persistent_origins[] = {
    "http_www.bar.com_0",
    "http_www.foo.com_8080",
    "http_www.foo.com_80",
  };
  size_t temporary_size = ARRAYSIZE_UNSAFE(temporary_origins);
  size_t persistent_size = ARRAYSIZE_UNSAFE(persistent_origins);
  std::set<std::string> temporary_set, persistent_set;
  for (size_t i = 0; i < temporary_size; ++i) {
    CreateOriginTypeDirectory(temporary_origins[i],
        fileapi::kFileSystemTypeTemporary);
    temporary_set.insert(temporary_origins[i]);
  }
  for (size_t i = 0; i < persistent_size; ++i) {
    CreateOriginTypeDirectory(persistent_origins[i], kFileSystemTypePersistent);
    persistent_set.insert(persistent_origins[i]);
  }

  size_t temporary_actual_size = 0;
  size_t persistent_actual_size = 0;
  std::string current;
  while (!(current = enumerator()->Next()).empty()) {
    SCOPED_TRACE(testing::Message() << "EnumerateOrigin " << current);
    if (enumerator()->HasTemporary()) {
      ASSERT_TRUE(temporary_set.find(current) != temporary_set.end());
      ++temporary_actual_size;
    }
    if (enumerator()->HasPersistent()) {
      ASSERT_TRUE(persistent_set.find(current) != persistent_set.end());
      ++persistent_actual_size;
    }
  }

  ASSERT_EQ(temporary_size, temporary_actual_size);
  ASSERT_EQ(persistent_size, persistent_actual_size);
}
