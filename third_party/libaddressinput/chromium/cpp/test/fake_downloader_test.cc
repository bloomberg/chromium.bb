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

#include "fake_downloader.h"

#include <libaddressinput/callback.h>
#include <libaddressinput/downloader.h>
#include <libaddressinput/util/scoped_ptr.h>

#include <string>

#include <gtest/gtest.h>

#include "region_data_constants.h"

namespace i18n {
namespace addressinput {

namespace {

// Tests for FakeDownloader object.
class FakeDownloaderTest : public testing::TestWithParam<std::string> {
 protected:
  FakeDownloaderTest() : downloader_(), success_(false), url_(), data_() {}
  virtual ~FakeDownloaderTest() {}

  scoped_ptr<Downloader::Callback> BuildCallback() {
    return ::i18n::addressinput::BuildScopedPtrCallback(
        this, &FakeDownloaderTest::OnDownloaded);
  }

  FakeDownloader downloader_;
  bool success_;
  std::string url_;
  scoped_ptr<std::string> data_;

 private:
  void OnDownloaded(bool success,
                    const std::string& url,
                    scoped_ptr<std::string> data) {
    success_ = success;
    url_ = url;
    data_ = data.Pass();
  }
};

// Returns testing::AssertionSuccess if |data| is valid downloaded data for
// |key|.
testing::AssertionResult DataIsValid(const std::string& data,
                                     const std::string& key) {
  if (data.empty()) {
    return testing::AssertionFailure() << "empty data";
  }

  static const char kDataBegin[] = "{\"data/";
  static const size_t kDataBeginLength = sizeof kDataBegin - 1;
  if (data.compare(0, kDataBeginLength, kDataBegin, kDataBeginLength) != 0) {
    return testing::AssertionFailure() << data << " does not begin with "
                                       << kDataBegin;
  }

  static const char kDataEnd[] = "\"}}";
  static const size_t kDataEndLength = sizeof kDataEnd - 1;
  if (data.compare(data.length() - kDataEndLength,
                   kDataEndLength,
                   kDataEnd,
                   kDataEndLength) != 0) {
    return testing::AssertionFailure() << data << " does not end with "
                                       << kDataEnd;
  }

  return testing::AssertionSuccess();
}

// Verifies that FakeDownloader downloads valid data for a region code.
TEST_P(FakeDownloaderTest, FakeDownloaderHasValidDataForRegion) {
  std::string key = "data/" + GetParam();
  std::string url = std::string(FakeDownloader::kFakeDataUrl) + key;
  downloader_.Download(url, BuildCallback());

  EXPECT_TRUE(success_);
  EXPECT_EQ(url, url_);
  EXPECT_TRUE(DataIsValid(*data_, key));
};

// Test all region codes.
INSTANTIATE_TEST_CASE_P(
    AllRegions, FakeDownloaderTest,
    testing::ValuesIn(RegionDataConstants::GetRegionCodes()));

// Verifies that downloading a missing key will return "{}".
TEST_F(FakeDownloaderTest, DownloadMissingKeyReturnsEmptyDictionary) {
  static const std::string kJunkUrl =
      std::string(FakeDownloader::kFakeDataUrl) + "junk";
  downloader_.Download(kJunkUrl, BuildCallback());

  EXPECT_TRUE(success_);
  EXPECT_EQ(kJunkUrl, url_);
  EXPECT_EQ("{}", *data_);
}

// Verifies that downloading an empty key will return "{}".
TEST_F(FakeDownloaderTest, DownloadEmptyKeyReturnsEmptyDictionary) {
  static const std::string kPrefixOnlyUrl = FakeDownloader::kFakeDataUrl;
  downloader_.Download(kPrefixOnlyUrl, BuildCallback());

  EXPECT_TRUE(success_);
  EXPECT_EQ(kPrefixOnlyUrl, url_);
  EXPECT_EQ("{}", *data_);
}

// Verifies that downloading a real URL fails.
TEST_F(FakeDownloaderTest, DownloadRealUrlFals) {
  static const std::string kRealUrl = "http://www.google.com/";
  downloader_.Download(kRealUrl, BuildCallback());

  EXPECT_FALSE(success_);
  EXPECT_EQ(kRealUrl, url_);
  EXPECT_TRUE(data_->empty());
}

}  // namespace

}  // namespace addressinput
}  // namespace i18n
