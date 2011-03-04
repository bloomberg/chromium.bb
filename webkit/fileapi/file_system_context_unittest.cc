// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_context.h"

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/quota/special_storage_policy.h"

using namespace fileapi;

namespace {

static const char* const kTestOrigins[] = {
  "https://a.com/",
  "http://b.com/",
  "http://c.com:1/",
  "file:///",
};

class TestSpecialStoragePolicy : public quota::SpecialStoragePolicy {
 public:
  virtual bool IsStorageProtected(const GURL& origin) {
    return false;
  }

  virtual bool IsStorageUnlimited(const GURL& origin) {
    return origin == GURL(kTestOrigins[1]);
  }
};

scoped_refptr<FileSystemContext> NewFileSystemContext(
    bool allow_file_access,
    bool unlimited_quota,
    scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy) {
  return new FileSystemContext(base::MessageLoopProxy::CreateForCurrentThread(),
                               base::MessageLoopProxy::CreateForCurrentThread(),
                               special_storage_policy,
                               FilePath(), false /* is_incognito */,
                               allow_file_access, unlimited_quota);
}

}  // anonymous namespace

TEST(FileSystemContextTest, IsStorageUnlimited) {
  // Regular cases.
  scoped_refptr<FileSystemContext> context(
      NewFileSystemContext(false, false, NULL));
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestOrigins); ++i) {
    SCOPED_TRACE(testing::Message() << "IsStorageUnlimited w/o policy #"
                 << i << " " << kTestOrigins[i]);
    EXPECT_FALSE(context->IsStorageUnlimited(GURL(kTestOrigins[i])));
  }

  // With allow_file_access=true cases.
  context = NewFileSystemContext(true, false, NULL);
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestOrigins); ++i) {
    SCOPED_TRACE(testing::Message() << "IsStorageUnlimited /w "
                 "allow_file_access=true #" << i << " " << kTestOrigins[i]);
    GURL origin(kTestOrigins[i]);
    EXPECT_EQ(origin.SchemeIsFile(), context->IsStorageUnlimited(origin));
  }

  // With unlimited_quota=true cases.
  context = NewFileSystemContext(false, true, NULL);
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestOrigins); ++i) {
    SCOPED_TRACE(testing::Message() << "IsStorageUnlimited /w "
                 "unlimited_quota=true #" << i << " " << kTestOrigins[i]);
    EXPECT_TRUE(context->IsStorageUnlimited(GURL(kTestOrigins[i])));
  }

  // With SpecialStoragePolicy.
  scoped_refptr<TestSpecialStoragePolicy> policy(new TestSpecialStoragePolicy);
  context = NewFileSystemContext(false, false, policy);
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestOrigins); ++i) {
    SCOPED_TRACE(testing::Message() << "IsStorageUnlimited /w policy #"
                 << i << " " << kTestOrigins[i]);
    GURL origin(kTestOrigins[i]);
    EXPECT_EQ(policy->IsStorageUnlimited(origin),
              context->IsStorageUnlimited(origin));
  }
}
