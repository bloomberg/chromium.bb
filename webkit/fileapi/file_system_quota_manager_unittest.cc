// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_quota_manager.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace fileapi;

class FileSystemQuotaManagerTest : public testing::Test {
 public:
  FileSystemQuotaManagerTest() { }

  void SetUp() {
    quota_.reset(new FileSystemQuotaManager(false, false));
  }

  FileSystemQuotaManager* quota() const { return quota_.get(); }

 protected:
  scoped_ptr<FileSystemQuotaManager> quota_;
  DISALLOW_COPY_AND_ASSIGN(FileSystemQuotaManagerTest);
};

namespace {

static const char* const kTestOrigins[] = {
  "https://a.com/",
  "http://b.com/",
  "http://c.com:1/",
  "file:///",
};

}  // anonymous namespace

TEST_F(FileSystemQuotaManagerTest, CheckOriginQuotaNotAllowed) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestOrigins); ++i) {
    SCOPED_TRACE(testing::Message() << "CheckOriginQuotaNotAllowed #"
                 << i << " " << kTestOrigins[i]);
    // Should fail no matter how much size is requested.
    EXPECT_FALSE(quota()->CheckOriginQuota(GURL(kTestOrigins[i]), -1));
    EXPECT_FALSE(quota()->CheckOriginQuota(GURL(kTestOrigins[i]), 0));
    EXPECT_FALSE(quota()->CheckOriginQuota(GURL(kTestOrigins[i]), 100));
  }
}

TEST_F(FileSystemQuotaManagerTest, CheckOriginQuotaUnlimited) {
  // Tests if SetOriginQuotaUnlimited and ResetOriginQuotaUnlimited
  // are working as expected.
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestOrigins); ++i) {
    SCOPED_TRACE(testing::Message() << "CheckOriginQuotaUnlimited #"
                 << i << " " << kTestOrigins[i]);
    GURL url(kTestOrigins[i]);
    EXPECT_FALSE(quota()->CheckIfOriginGrantedUnlimitedQuota(url));
    EXPECT_FALSE(quota()->CheckOriginQuota(url, 0));

    quota()->SetOriginQuotaUnlimited(url);
    EXPECT_TRUE(quota()->CheckIfOriginGrantedUnlimitedQuota(url));
    EXPECT_TRUE(quota()->CheckOriginQuota(url, -1));
    EXPECT_TRUE(quota()->CheckOriginQuota(url, 0));
    EXPECT_TRUE(quota()->CheckOriginQuota(url, 100));

    quota()->ResetOriginQuotaUnlimited(url);
    EXPECT_FALSE(quota()->CheckIfOriginGrantedUnlimitedQuota(url));
    EXPECT_FALSE(quota()->CheckOriginQuota(url, -1));
    EXPECT_FALSE(quota()->CheckOriginQuota(url, 0));
    EXPECT_FALSE(quota()->CheckOriginQuota(url, 100));
  }
}

TEST_F(FileSystemQuotaManagerTest, CheckOriginQuotaWithMixedSet) {
  // Tests setting unlimited quota for some urls doesn't affect
  // other urls.
  GURL test_url1("http://foo.bar.com/");
  GURL test_url2("http://example.com/");
  quota()->SetOriginQuotaUnlimited(test_url1);
  quota()->SetOriginQuotaUnlimited(test_url2);

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestOrigins); ++i) {
    SCOPED_TRACE(testing::Message() << "CheckOriginQuotaMixedSet #"
                 << i << " " << kTestOrigins[i]);
    GURL url(kTestOrigins[i]);
    EXPECT_FALSE(quota()->CheckOriginQuota(url, 0));
    EXPECT_FALSE(quota()->CheckIfOriginGrantedUnlimitedQuota(url));
  }
}

TEST_F(FileSystemQuotaManagerTest, CheckOriginQuotaMixedWithDifferentScheme) {
  // Tests setting unlimited quota for urls doesn't affect
  // pages in the same hosts but with different scheme.
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestOrigins); ++i) {
    GURL url(kTestOrigins[i]);
    if (url.SchemeIsFile())
      continue;
    ASSERT_TRUE(url == url.GetOrigin());
    std::string new_scheme = "https";
    if (url.SchemeIsSecure())
      new_scheme = "http";
    else
      ASSERT_TRUE(url.SchemeIs("http"));
    std::string new_url_string = new_scheme + "://" + url.host();
    if (url.has_port())
      new_url_string += ":" + url.port();
    quota()->SetOriginQuotaUnlimited(GURL(new_url_string));
  }

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestOrigins); ++i) {
    SCOPED_TRACE(testing::Message()
                 << "CheckOriginQuotaMixedWithDifferentScheme #"
                 << i << " " << kTestOrigins[i]);
    GURL url(kTestOrigins[i]);
    EXPECT_FALSE(quota()->CheckOriginQuota(url, 0));
    EXPECT_FALSE(quota()->CheckIfOriginGrantedUnlimitedQuota(url));
  }
}

TEST_F(FileSystemQuotaManagerTest, CheckOriginQuotaMixedWithDifferentPort) {
  // Tests setting unlimited quota for urls doesn't affect
  // pages in the same scheme/hosts but with different port number.
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestOrigins); ++i) {
    GURL url(kTestOrigins[i]);
    if (url.SchemeIsFile())
      continue;
    ASSERT_TRUE(url == url.GetOrigin());
    int port = 81;
    if (url.has_port())
      port = url.IntPort() + 1;
    GURL new_url(url.scheme() + "://" + url.host() + ":" +
                 base::IntToString(port));
    quota()->SetOriginQuotaUnlimited(new_url);
  }

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestOrigins); ++i) {
    SCOPED_TRACE(testing::Message()
                 << "CheckOriginQuotaMixedWithDifferentPort #"
                 << i << " " << kTestOrigins[i]);
    GURL url(kTestOrigins[i]);
    EXPECT_FALSE(quota()->CheckOriginQuota(url, 0));
    EXPECT_FALSE(quota()->CheckIfOriginGrantedUnlimitedQuota(url));
  }
}
