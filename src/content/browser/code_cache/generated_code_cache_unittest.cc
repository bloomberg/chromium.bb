// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/code_cache/generated_code_cache.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_task_environment.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class GeneratedCodeCacheTest : public testing::Test {
 public:
  static const int kMaxSizeInBytes = 1024 * 1024;
  static constexpr char kInitialUrl[] = "http://example.com/script.js";
  static constexpr char kInitialOrigin[] = "http://example.com";
  static constexpr char kInitialData[] = "InitialData";

  GeneratedCodeCacheTest() = default;

  void SetUp() override {
    ASSERT_TRUE(cache_dir_.CreateUniqueTempDir());
    cache_path_ = cache_dir_.GetPath();
  }

  void TearDown() override {
    disk_cache::FlushCacheThreadForTesting();
    scoped_task_environment_.RunUntilIdle();
  }

  // This function initializes the cache and waits till the transaction is
  // finished. When this function returns, the backend is already initialized.
  void InitializeCache(GeneratedCodeCache::CodeCacheType cache_type) {
    // Create code cache
    generated_code_cache_ = std::make_unique<GeneratedCodeCache>(
        cache_path_, kMaxSizeInBytes, cache_type);

    GURL url(kInitialUrl);
    url::Origin origin = url::Origin::Create(GURL(kInitialOrigin));
    WriteToCache(url, origin, kInitialData, base::Time::Now());
    scoped_task_environment_.RunUntilIdle();
  }

  // This function initializes the cache and reopens it. When this function
  // returns, the backend initialization is not complete yet. This is used
  // to test the pending operaions path.
  void InitializeCacheAndReOpen(GeneratedCodeCache::CodeCacheType cache_type) {
    InitializeCache(cache_type);
    generated_code_cache_.reset(
        new GeneratedCodeCache(cache_path_, kMaxSizeInBytes, cache_type));
  }

  void WriteToCache(const GURL& url,
                    const url::Origin& origin,
                    const std::string& data,
                    base::Time response_time) {
    std::vector<uint8_t> vector_data(data.begin(), data.end());
    generated_code_cache_->WriteData(url, origin, response_time, vector_data);
  }

  void DeleteFromCache(const GURL& url, const url::Origin& origin) {
    generated_code_cache_->DeleteEntry(url, origin);
  }

  void FetchFromCache(const GURL& url, const url::Origin& origin) {
    received_ = false;
    GeneratedCodeCache::ReadDataCallback callback = base::BindRepeating(
        &GeneratedCodeCacheTest::FetchEntryCallback, base::Unretained(this));
    generated_code_cache_->FetchEntry(url, origin, callback);
  }

  void ClearCache() {
    generated_code_cache_->ClearCache(base::BindRepeating(
        &GeneratedCodeCacheTest::ClearCacheComplete, base::Unretained(this)));
  }

  void ClearCacheComplete(int rv) {}

  void FetchEntryCallback(const base::Time& response_time,
                          const std::vector<uint8_t>& data) {
    if (data.size() == 0) {
      received_ = true;
      received_null_ = true;
      received_response_time_ = response_time;
      return;
    }
    std::string str(data.begin(), data.end());
    received_ = true;
    received_null_ = false;
    received_data_ = str;
    received_response_time_ = response_time;
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<GeneratedCodeCache> generated_code_cache_;
  base::ScopedTempDir cache_dir_;
  std::string received_data_;
  base::Time received_response_time_;
  bool received_;
  bool received_null_;
  base::FilePath cache_path_;
};

constexpr char GeneratedCodeCacheTest::kInitialUrl[];
constexpr char GeneratedCodeCacheTest::kInitialOrigin[];
constexpr char GeneratedCodeCacheTest::kInitialData[];
const int GeneratedCodeCacheTest::kMaxSizeInBytes;

TEST_F(GeneratedCodeCacheTest, CheckResponseTime) {
  GURL url(kInitialUrl);
  url::Origin origin = url::Origin::Create(GURL(kInitialOrigin));

  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript);
  std::string data = "SerializedCodeForScript";
  base::Time response_time = base::Time::Now();
  WriteToCache(url, origin, data, response_time);
  scoped_task_environment_.RunUntilIdle();
  FetchFromCache(url, origin);
  scoped_task_environment_.RunUntilIdle();

  ASSERT_TRUE(received_);
  EXPECT_EQ(data, received_data_);
  EXPECT_EQ(response_time, received_response_time_);
}

TEST_F(GeneratedCodeCacheTest, FetchEntry) {
  GURL url(kInitialUrl);
  url::Origin origin = url::Origin::Create(GURL(kInitialOrigin));

  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript);
  FetchFromCache(url, origin);
  scoped_task_environment_.RunUntilIdle();

  ASSERT_TRUE(received_);
  EXPECT_EQ(kInitialData, received_data_);
}

TEST_F(GeneratedCodeCacheTest, WriteEntry) {
  GURL new_url("http://example1.com/script.js");
  url::Origin origin = url::Origin::Create(GURL(kInitialOrigin));

  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript);
  std::string data = "SerializedCodeForScript";
  base::Time response_time = base::Time::Now();
  WriteToCache(new_url, origin, data, response_time);
  scoped_task_environment_.RunUntilIdle();
  FetchFromCache(new_url, origin);
  scoped_task_environment_.RunUntilIdle();

  ASSERT_TRUE(received_);
  EXPECT_EQ(data, received_data_);
  EXPECT_EQ(response_time, received_response_time_);
}

TEST_F(GeneratedCodeCacheTest, DeleteEntry) {
  GURL url(kInitialUrl);
  url::Origin origin = url::Origin::Create(GURL(kInitialOrigin));

  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript);
  DeleteFromCache(url, origin);
  FetchFromCache(url, origin);
  scoped_task_environment_.RunUntilIdle();

  ASSERT_TRUE(received_);
  ASSERT_TRUE(received_null_);
}

TEST_F(GeneratedCodeCacheTest, WriteEntryWithEmptyData) {
  GURL url(kInitialUrl);
  url::Origin origin = url::Origin::Create(GURL(kInitialOrigin));

  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript);
  base::Time response_time = base::Time::Now();
  WriteToCache(url, origin, std::string(), response_time);
  scoped_task_environment_.RunUntilIdle();
  FetchFromCache(url, origin);
  scoped_task_environment_.RunUntilIdle();

  ASSERT_TRUE(received_);
  ASSERT_TRUE(received_null_);
  EXPECT_EQ(response_time, received_response_time_);
}

TEST_F(GeneratedCodeCacheTest, FetchEntryPendingOp) {
  GURL url(kInitialUrl);
  url::Origin origin = url::Origin::Create(GURL(kInitialOrigin));

  InitializeCacheAndReOpen(GeneratedCodeCache::CodeCacheType::kJavaScript);
  FetchFromCache(url, origin);
  scoped_task_environment_.RunUntilIdle();

  ASSERT_TRUE(received_);
  EXPECT_EQ(kInitialData, received_data_);
}

TEST_F(GeneratedCodeCacheTest, WriteEntryPendingOp) {
  GURL new_url("http://example1.com/script1.js");
  url::Origin origin = url::Origin::Create(GURL(kInitialOrigin));

  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript);
  std::string data = "SerializedCodeForScript";
  base::Time response_time = base::Time::Now();
  WriteToCache(new_url, origin, data, response_time);
  scoped_task_environment_.RunUntilIdle();
  FetchFromCache(new_url, origin);
  scoped_task_environment_.RunUntilIdle();

  ASSERT_TRUE(received_);
  EXPECT_EQ(data, received_data_);
  EXPECT_EQ(response_time, received_response_time_);
}

TEST_F(GeneratedCodeCacheTest, DeleteEntryPendingOp) {
  GURL url(kInitialUrl);
  url::Origin origin = url::Origin::Create(GURL(kInitialOrigin));

  InitializeCacheAndReOpen(GeneratedCodeCache::CodeCacheType::kJavaScript);
  DeleteFromCache(url, origin);
  FetchFromCache(url, origin);
  scoped_task_environment_.RunUntilIdle();

  ASSERT_TRUE(received_);
  ASSERT_TRUE(received_null_);
}

TEST_F(GeneratedCodeCacheTest, UpdateDataOfExistingEntry) {
  GURL url(kInitialUrl);
  url::Origin origin = url::Origin::Create(GURL(kInitialOrigin));

  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript);
  std::string new_data = "SerializedCodeForScriptOverwrite";
  base::Time response_time = base::Time::Now();
  WriteToCache(url, origin, new_data, response_time);
  scoped_task_environment_.RunUntilIdle();
  FetchFromCache(url, origin);
  scoped_task_environment_.RunUntilIdle();

  ASSERT_TRUE(received_);
  EXPECT_EQ(new_data, received_data_);
  EXPECT_EQ(response_time, received_response_time_);
}

TEST_F(GeneratedCodeCacheTest, FetchFailsForNonexistingOrigin) {
  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript);
  url::Origin new_origin = url::Origin::Create(GURL("http://not-example.com"));
  FetchFromCache(GURL(kInitialUrl), new_origin);
  scoped_task_environment_.RunUntilIdle();

  ASSERT_TRUE(received_);
  ASSERT_TRUE(received_null_);
}

TEST_F(GeneratedCodeCacheTest, FetchEntriesFromSameOrigin) {
  GURL url("http://example.com/script.js");
  GURL second_url("http://script.com/one.js");
  url::Origin origin = url::Origin::Create(GURL(kInitialOrigin));

  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript);
  std::string data_first_resource = "SerializedCodeForFirstResource";
  WriteToCache(url, origin, data_first_resource, base::Time());

  std::string data_second_resource = "SerializedCodeForSecondResource";
  WriteToCache(second_url, origin, data_second_resource, base::Time());
  scoped_task_environment_.RunUntilIdle();

  FetchFromCache(url, origin);
  scoped_task_environment_.RunUntilIdle();
  ASSERT_TRUE(received_);
  EXPECT_EQ(data_first_resource, received_data_);

  FetchFromCache(second_url, origin);
  scoped_task_environment_.RunUntilIdle();
  ASSERT_TRUE(received_);
  EXPECT_EQ(data_second_resource, received_data_);
}

TEST_F(GeneratedCodeCacheTest, FetchSucceedsFromDifferentOrigins) {
  GURL url("http://example.com/script.js");
  url::Origin origin = url::Origin::Create(GURL("http://example.com"));
  url::Origin origin1 = url::Origin::Create(GURL("http://example1.com"));

  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript);
  std::string data_origin = "SerializedCodeForFirstOrigin";
  WriteToCache(url, origin, data_origin, base::Time());

  std::string data_origin1 = "SerializedCodeForSecondOrigin";
  WriteToCache(url, origin1, data_origin1, base::Time());
  scoped_task_environment_.RunUntilIdle();

  FetchFromCache(url, origin);
  scoped_task_environment_.RunUntilIdle();
  ASSERT_TRUE(received_);
  EXPECT_EQ(data_origin, received_data_);

  FetchFromCache(url, origin1);
  scoped_task_environment_.RunUntilIdle();
  ASSERT_TRUE(received_);
  EXPECT_EQ(data_origin1, received_data_);
}

TEST_F(GeneratedCodeCacheTest, FetchFailsForUniqueOrigin) {
  GURL url("http://example.com/script.js");
  url::Origin origin =
      url::Origin::Create(GURL("data:text/html,<script></script>"));

  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript);
  std::string data = "SerializedCodeForUniqueOrigin";
  WriteToCache(url, origin, data, base::Time());
  scoped_task_environment_.RunUntilIdle();

  FetchFromCache(url, origin);
  scoped_task_environment_.RunUntilIdle();
  ASSERT_TRUE(received_);
  ASSERT_TRUE(received_null_);
}

TEST_F(GeneratedCodeCacheTest, FetchFailsForInvalidOrigin) {
  GURL url("http://example.com/script.js");
  url::Origin origin = url::Origin::Create(GURL("invalidURL"));

  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript);
  std::string data = "SerializedCodeForInvalidOrigin";
  WriteToCache(url, origin, data, base::Time());
  scoped_task_environment_.RunUntilIdle();

  FetchFromCache(url, origin);
  scoped_task_environment_.RunUntilIdle();

  ASSERT_TRUE(received_);
  ASSERT_TRUE(received_null_);
}

TEST_F(GeneratedCodeCacheTest, FetchFailsForInvalidURL) {
  GURL url("InvalidURL");
  url::Origin origin = url::Origin::Create(GURL("http://example.com"));

  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript);
  std::string data = "SerializedCodeForInvalidURL";
  WriteToCache(url, origin, data, base::Time());
  scoped_task_environment_.RunUntilIdle();

  FetchFromCache(url, origin);
  scoped_task_environment_.RunUntilIdle();
  ASSERT_TRUE(received_);
  ASSERT_TRUE(received_null_);
}

TEST_F(GeneratedCodeCacheTest, ClearCache) {
  GURL url("http://example.com/script.js");
  url::Origin origin = url::Origin::Create(GURL("http://example.com"));

  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript);
  ClearCache();
  scoped_task_environment_.RunUntilIdle();
  FetchFromCache(url, origin);
  scoped_task_environment_.RunUntilIdle();

  ASSERT_TRUE(received_);
  ASSERT_TRUE(received_null_);
}

TEST_F(GeneratedCodeCacheTest, WasmCache) {
  GURL url(kInitialUrl);
  url::Origin origin = url::Origin::Create(GURL(kInitialOrigin));

  InitializeCache(GeneratedCodeCache::CodeCacheType::kWebAssembly);
  FetchFromCache(url, origin);
  scoped_task_environment_.RunUntilIdle();

  ASSERT_TRUE(received_);
  EXPECT_EQ(kInitialData, received_data_);
}
}  // namespace content
