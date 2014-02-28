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
#include <libaddressinput/storage.h>
#include <libaddressinput/util/scoped_ptr.h>

#include <cstddef>
#include <ctime>
#include <string>

#include <gtest/gtest.h>

#include "fake_downloader.h"
#include "fake_storage.h"
#include "region_data_constants.h"
#include "time_to_string.h"

namespace i18n {
namespace addressinput {

namespace {

const char kKey[] = "data/CA";

// Empty data that the downloader can return.
const char kEmptyData[] = "{}";

// The MD5 checksum for kEmptyData. Retriever uses MD5 to validate data
// integrity.
const char kEmptyDataChecksum[] = "99914b932bd37a50b983c5e7c90ae93b";

scoped_ptr<std::string> Wrap(const std::string& data,
                             const std::string& checksum,
                             const std::string& timestamp) {
  return make_scoped_ptr(new std::string(
      data + "\n" + "checksum=" + checksum + "\n" + "timestamp=" + timestamp));
}

}  // namespace

// Tests for Retriever object.
class RetrieverTest : public testing::TestWithParam<std::string> {
 protected:
  RetrieverTest()
      : storage_(NULL),
        retriever_(),
        success_(false),
        key_(),
        data_(),
        reject_empty_data_(false) {
    ResetRetriever(FakeDownloader::kFakeDataUrl);
  }

  virtual ~RetrieverTest() {}

  scoped_ptr<Retriever::Callback> BuildCallback() {
    return ::i18n::addressinput::BuildCallback(
        this, &RetrieverTest::OnDataReady);
  }

  void ResetRetriever(const std::string& url) {
    storage_ = new FakeStorage;
    retriever_.reset(
        new Retriever(url,
                      scoped_ptr<Downloader>(new FakeDownloader),
                      scoped_ptr<Storage>(storage_)));
  }

  std::string GetUrlForKey(const std::string& key) {
    return retriever_->GetUrlForKey(key);
  }

  std::string GetKeyForUrl(const std::string& url) {
    return retriever_->GetKeyForUrl(url);
  }

  FakeStorage* storage_;  // Owned by |retriever_|.
  scoped_ptr<Retriever> retriever_;
  bool success_;
  std::string key_;
  std::string data_;
  bool reject_empty_data_;

 private:
  bool OnDataReady(bool success,
                   const std::string& key,
                   const std::string& data) {
    success_ = success;
    key_ = key;
    data_ = data;
    return !reject_empty_data_ || data_ != kEmptyData;
  }
};

TEST_P(RetrieverTest, RegionHasData) {
  std::string key = "data/" + GetParam();
  retriever_->Retrieve(key, BuildCallback());

  EXPECT_TRUE(success_);
  EXPECT_EQ(key, key_);
  EXPECT_FALSE(data_.empty());
  EXPECT_NE(kEmptyData, data_);
}

INSTANTIATE_TEST_CASE_P(
    AllRegions, RetrieverTest,
    testing::ValuesIn(RegionDataConstants::GetRegionCodes()));

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
    (*downloaded)(false, url, make_scoped_ptr(new std::string("garbage")));
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

TEST_F(RetrieverTest, NoChecksumAndTimestampWillRedownload) {
  storage_->Put(kKey, make_scoped_ptr(new std::string(kEmptyData)));
  retriever_->Retrieve(kKey, BuildCallback());
  EXPECT_TRUE(success_);
  EXPECT_EQ(kKey, key_);
  EXPECT_FALSE(data_.empty());
  EXPECT_NE(kEmptyData, data_);
}

TEST_F(RetrieverTest, ChecksumAndTimestampWillNotRedownload) {
  storage_->Put(kKey,
                Wrap(kEmptyData, kEmptyDataChecksum, TimeToString(time(NULL))));
  retriever_->Retrieve(kKey, BuildCallback());
  EXPECT_TRUE(success_);
  EXPECT_EQ(kKey, key_);
  EXPECT_EQ(kEmptyData, data_);
}

TEST_F(RetrieverTest, OldTimestampWillRedownload) {
  storage_->Put(kKey, Wrap(kEmptyData, kEmptyDataChecksum, "0"));
  retriever_->Retrieve(kKey, BuildCallback());
  EXPECT_TRUE(success_);
  EXPECT_EQ(kKey, key_);
  EXPECT_FALSE(data_.empty());
  EXPECT_NE(kEmptyData, data_);
}

TEST_F(RetrieverTest, JunkDataRedownloads) {
  ResetRetriever(std::string(FakeDownloader::kFakeDataUrl));
  storage_->Put(kKey,
                Wrap(kEmptyData, kEmptyDataChecksum, TimeToString(time(NULL))));
  reject_empty_data_ = true;
  retriever_->Retrieve(kKey, BuildCallback());
  EXPECT_TRUE(success_);
  EXPECT_EQ(kKey, key_);
  EXPECT_FALSE(data_.empty());
  EXPECT_NE(kEmptyData, data_);

  // After verifying it's correct, it's saved in storage.
  EXPECT_EQ(data_, storage_->SynchronousGet(kKey).substr(0, data_.size()));
}

TEST_F(RetrieverTest, JunkDataIsntStored) {
  // Data the retriever accepts is stored in |storage_|.
  ResetRetriever("test:///");
  const std::string not_a_key("foobar");
  retriever_->Retrieve(not_a_key, BuildCallback());
  EXPECT_TRUE(success_);
  EXPECT_EQ(not_a_key, key_);
  EXPECT_FALSE(data_.empty());
  EXPECT_EQ(kEmptyData, data_);
  EXPECT_EQ(
      kEmptyData,
      storage_->SynchronousGet(not_a_key).substr(0, sizeof kEmptyData - 1));

  // Try again, but this time reject the data.
  reject_empty_data_ = true;
  ResetRetriever("test:///");
  EXPECT_EQ("", storage_->SynchronousGet(not_a_key));
  retriever_->Retrieve(not_a_key, BuildCallback());

  // Falls back to the fallback, which doesn't have data for Canada.
  EXPECT_FALSE(success_);
  EXPECT_EQ(not_a_key, key_);
  EXPECT_TRUE(data_.empty());

  // Since the retriever is rejecting empty data, it shouldn't be stored.
  EXPECT_EQ("", storage_->SynchronousGet(not_a_key));
}

TEST_F(RetrieverTest, OldTimestampOkIfDownloadFails) {
  storage_ = new FakeStorage;
  Retriever bad_retriever(FakeDownloader::kFakeDataUrl,
                          scoped_ptr<Downloader>(new FaultyDownloader),
                          scoped_ptr<Storage>(storage_));
  storage_->Put(kKey, Wrap(kEmptyData, kEmptyDataChecksum, "0"));
  bad_retriever.Retrieve(kKey, BuildCallback());
  EXPECT_TRUE(success_);
  EXPECT_EQ(kKey, key_);
  EXPECT_EQ(kEmptyData, data_);
}

TEST_F(RetrieverTest, WrongChecksumWillRedownload) {
  static const char kNonEmptyData[] = "{\"non-empty\": \"data\"}";
  storage_->Put(
      kKey,
      Wrap(kNonEmptyData, kEmptyDataChecksum, TimeToString(time(NULL))));
  retriever_->Retrieve(kKey, BuildCallback());
  EXPECT_TRUE(success_);
  EXPECT_EQ(kKey, key_);
  EXPECT_FALSE(data_.empty());
  EXPECT_NE(kNonEmptyData, data_);
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

TEST_F(RetrieverTest, NullCallbackNoCrash) {
  ASSERT_NO_FATAL_FAILURE(
      retriever_->Retrieve(kKey, scoped_ptr<Retriever::Callback>()));
  ASSERT_NO_FATAL_FAILURE(
      retriever_->Retrieve(kKey, scoped_ptr<Retriever::Callback>()));
}

}  // namespace addressinput
}  // namespace i18n
