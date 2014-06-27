// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/libaddressinput/chromium/chrome_downloader_impl.h"

#include "base/message_loop/message_loop_proxy.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

static const char kFakeUrl[] = "https://example.com";
static const char kFakeInsecureUrl[] = "http://example.com";

class ChromeDownloaderImplTest : public testing::Test {
 public:
  ChromeDownloaderImplTest()
      : fake_factory_(&factory_),
        success_(false) {}
  virtual ~ChromeDownloaderImplTest() {}

 protected:
  // Sets the response for the download.
  void SetFakeResponse(const std::string& payload, net::HttpStatusCode code) {
    fake_factory_.SetFakeResponse(url_,
                                  payload,
                                  code,
                                  net::URLRequestStatus::SUCCESS);
  }

  // Kicks off the download.
  void Download() {
    scoped_refptr<net::TestURLRequestContextGetter> getter(
        new net::TestURLRequestContextGetter(
            base::MessageLoopProxy::current()));
    ChromeDownloaderImpl impl(getter);
    impl.Download(url_.spec(), BuildCallback());
    base::MessageLoop::current()->RunUntilIdle();
  }

  void set_url(const GURL& url) { url_ = url; }
  const std::string& data() { return *data_; }
  bool success() { return success_; }

 private:
  scoped_ptr<ChromeDownloaderImpl::Callback> BuildCallback() {
    return ::i18n::addressinput::BuildScopedPtrCallback(
        this, &ChromeDownloaderImplTest::OnDownloaded);
  }

  // Callback for when download is finished.
  void OnDownloaded(bool success,
                    const std::string& url,
                    scoped_ptr<std::string> data) {
    success_ = success;
    data_ = data.Pass();
  }

  base::MessageLoop loop_;
  net::URLFetcherImplFactory factory_;
  net::FakeURLFetcherFactory fake_factory_;
  GURL url_;
  scoped_ptr<std::string> data_;
  bool success_;
};

TEST_F(ChromeDownloaderImplTest, Success) {
  const char kFakePayload[] = "ham hock";
  set_url(GURL(kFakeUrl));
  SetFakeResponse(kFakePayload, net::HTTP_OK);
  Download();
  EXPECT_TRUE(success());
  EXPECT_EQ(kFakePayload, data());
}

TEST_F(ChromeDownloaderImplTest, Failure) {
  const char kFakePayload[] = "ham hock";
  set_url(GURL(kFakeUrl));
  SetFakeResponse(kFakePayload, net::HTTP_INTERNAL_SERVER_ERROR);
  Download();
  EXPECT_FALSE(success());
  EXPECT_EQ(std::string(), data());
}

TEST_F(ChromeDownloaderImplTest, RejectsInsecureScheme) {
  const char kFakePayload[] = "ham hock";
  set_url(GURL(kFakeInsecureUrl));
  SetFakeResponse(kFakePayload, net::HTTP_OK);
  Download();
  EXPECT_FALSE(success());
  EXPECT_EQ(std::string(), data());
}

}  // namespace autofill
