// Copyright (C) 2013 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "retriever.h"

#include <libaddressinput/callback.h>
#include <libaddressinput/downloader.h>
#include <libaddressinput/util/scoped_ptr.h>

#include <string>

#include <gtest/gtest.h>

#include "fake_downloader.h"
#include "fake_storage.h"
#include "region_data_constants.h"

namespace i18n {
namespace addressinput {

const char kKey[] = "data/CA";

// Empty data that the downloader can return.
const char kEmptyData[] = "{}";

// Tests for Retriever object.
class RetrieverTest : public testing::Test {
 protected:
  RetrieverTest()
      : success_(false),
        key_(),
        data_() {
    ResetRetriever(FakeDownloader::kFakeDataUrl);
  }

  virtual ~RetrieverTest() {}

  scoped_ptr<Retriever::Callback> BuildCallback() {
    return ::i18n::addressinput::BuildCallback(
        this, &RetrieverTest::OnDataReady);
  }

  void ResetRetriever(const std::string& url) {
    retriever_.reset(
        new Retriever(url,
                      scoped_ptr<Downloader>(new FakeDownloader),
                      scoped_ptr<Storage>(new FakeStorage)));
  }

  std::string GetUrlForKey(const std::string& key) {
    return retriever_->GetUrlForKey(key);
  }

  std::string GetKeyForUrl(const std::string& url) {
    return retriever_->GetKeyForUrl(url);
  }

  scoped_ptr<Retriever> retriever_;
  bool success_;
  std::string key_;
  std::string data_;

 private:
  void OnDataReady(bool success,
                   const std::string& key,
                   const std::string& data) {
    success_ = success;
    key_ = key;
    data_ = data;
  }
};

TEST_F(RetrieverTest, RetrieveData) {
  retriever_->Retrieve(kKey, BuildCallback());

  EXPECT_TRUE(success_);
  EXPECT_EQ(kKey, key_);
  EXPECT_FALSE(data_.empty());
  EXPECT_NE(kEmptyData, data_);
}

TEST_F(RetrieverTest, ReadDataFromStorage) {
  retriever_->Retrieve(kKey, BuildCallback());
  retriever_->Retrieve(kKey, BuildCallback());

  EXPECT_TRUE(success_);
  EXPECT_EQ(kKey, key_);
  EXPECT_FALSE(data_.empty());
  EXPECT_NE(kEmptyData, data_);
}

TEST_F(RetrieverTest, MissingKeyReturnsEmptyData) {
  static const char kMissingKey[] = "junk";

  retriever_->Retrieve(kMissingKey, BuildCallback());

  EXPECT_TRUE(success_);
  EXPECT_EQ(kMissingKey, key_);
  EXPECT_EQ(kEmptyData, data_);
}

// The downloader that always fails.
class FaultyDownloader : public Downloader {
 public:
  FaultyDownloader() {}
  virtual ~FaultyDownloader() {}

  // Downloader implementation.
  virtual void Download(const std::string& url,
                        scoped_ptr<Callback> downloaded) {
    (*downloaded)(false, url, "garbage");
  }
};

TEST_F(RetrieverTest, FaultyDownloader) {
  Retriever bad_retriever(FakeDownloader::kFakeDataUrl,
                          scoped_ptr<Downloader>(new FaultyDownloader),
                          scoped_ptr<Storage>(new FakeStorage));
  bad_retriever.Retrieve(kKey, BuildCallback());

  EXPECT_FALSE(success_);
  EXPECT_EQ(kKey, key_);
  EXPECT_TRUE(data_.empty());
}

TEST_F(RetrieverTest, FaultyDownloaderFallback) {
  Retriever bad_retriever(FakeDownloader::kFakeDataUrl,
                          scoped_ptr<Downloader>(new FaultyDownloader),
                          scoped_ptr<Storage>(new FakeStorage));
  const char kFallbackDataKey[] = "data/US";
  bad_retriever.Retrieve(kFallbackDataKey, BuildCallback());

  EXPECT_TRUE(success_);
  EXPECT_EQ(kFallbackDataKey, key_);
  EXPECT_FALSE(data_.empty());
  EXPECT_NE(kEmptyData, data_);
}

// The downloader that doesn't get back to you.
class HangingDownloader : public Downloader {
 public:
  HangingDownloader() {}
  virtual ~HangingDownloader() {}

  // Downloader implementation.
  virtual void Download(const std::string& url,
                        scoped_ptr<Callback> downloaded) {}
};

TEST_F(RetrieverTest, RequestsDontStack) {
  Retriever slow_retriever(FakeDownloader::kFakeDataUrl,
                           scoped_ptr<Downloader>(new HangingDownloader),
                           scoped_ptr<Storage>(new FakeStorage));

  slow_retriever.Retrieve(kKey, BuildCallback());
  EXPECT_FALSE(success_);
  EXPECT_TRUE(key_.empty());

  EXPECT_NO_FATAL_FAILURE(slow_retriever.Retrieve(kKey, BuildCallback()));
}

TEST_F(RetrieverTest, GetUrlForKey) {
  ResetRetriever("test:///");
  EXPECT_EQ("test:///", GetUrlForKey(""));
  EXPECT_EQ("test:///data", GetUrlForKey("data"));
  EXPECT_EQ("test:///data/US", GetUrlForKey("data/US"));
  EXPECT_EQ("test:///data/CA--fr", GetUrlForKey("data/CA--fr"));
}

TEST_F(RetrieverTest, GetKeyForUrl) {
  ResetRetriever("test:///");
  EXPECT_EQ("", GetKeyForUrl("test://"));
  EXPECT_EQ("", GetKeyForUrl("http://www.google.com/"));
  EXPECT_EQ("", GetKeyForUrl(""));
  EXPECT_EQ("", GetKeyForUrl("test:///"));
  EXPECT_EQ("data", GetKeyForUrl("test:///data"));
  EXPECT_EQ("data/US", GetKeyForUrl("test:///data/US"));
  EXPECT_EQ("data/CA--fr", GetKeyForUrl("test:///data/CA--fr"));
}

}  // namespace addressinput
}  // namespace i18n
