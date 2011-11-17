// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "media/base/filters.h"
#include "media/base/mock_callback.h"
#include "media/base/mock_filter_host.h"
#include "media/base/mock_filters.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLError.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLLoader.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLResponse.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "webkit/media/simple_data_source.h"
#include "webkit/mocks/mock_webframeclient.h"
#include "webkit/mocks/mock_weburlloader.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::StrictMock;
using ::testing::WithArgs;

using WebKit::WebURLError;
using WebKit::WebURLLoader;
using WebKit::WebURLRequest;
using WebKit::WebURLResponse;
using WebKit::WebView;

using webkit_glue::MockWebFrameClient;
using webkit_glue::MockWebURLLoader;

namespace webkit_media {

static const int kDataSize = 1024;
static const char kHttpUrl[] = "http://test";
static const char kHttpsUrl[] = "https://test";
static const char kFileUrl[] = "file://test";
static const char kDataUrl[] =
    "data:text/plain;base64,YWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eXoK";
static const char kDataUrlDecoded[] = "abcdefghijklmnopqrstuvwxyz";
static const char kInvalidUrl[] = "whatever://test";
static const char kHttpRedirectToSameDomainUrl1[] = "http://test/ing";
static const char kHttpRedirectToSameDomainUrl2[] = "http://test/ing2";
static const char kHttpRedirectToDifferentDomainUrl1[] = "http://test2";
static const char kHttpRedirectToDifferentDomainUrl2[] = "http://test2/ing";

class SimpleDataSourceTest : public testing::Test {
 public:
  SimpleDataSourceTest()
      : view_(WebView::create(NULL)) {
    view_->initializeMainFrame(&client_);

    for (int i = 0; i < kDataSize; ++i) {
      data_[i] = i;
    }
  }

  virtual ~SimpleDataSourceTest() {
    view_->close();
  }

  void InitializeDataSource(const char* url,
                            const media::PipelineStatusCB& callback) {
    gurl_ = GURL(url);

    url_loader_ = new NiceMock<MockWebURLLoader>();

    data_source_ = new SimpleDataSource(MessageLoop::current(),
                                        view_->mainFrame());

    // There is no need to provide a message loop to data source.
    data_source_->set_host(&host_);
    data_source_->SetURLLoaderForTest(url_loader_);

    data_source_->Initialize(url, callback);
    MessageLoop::current()->RunAllPending();
  }

  void RequestSucceeded(bool is_loaded) {
    WebURLResponse response(gurl_);
    response.setExpectedContentLength(kDataSize);

    data_source_->didReceiveResponse(NULL, response);
    int64 size;
    EXPECT_TRUE(data_source_->GetSize(&size));
    EXPECT_EQ(kDataSize, size);

    for (int i = 0; i < kDataSize; ++i) {
      data_source_->didReceiveData(NULL, data_ + i, 1, 1);
    }

    EXPECT_CALL(host_, SetLoaded(is_loaded));

    InSequence s;
    EXPECT_CALL(host_, SetTotalBytes(kDataSize));
    EXPECT_CALL(host_, SetBufferedBytes(kDataSize));

    data_source_->didFinishLoading(NULL, 0);

    // Let the tasks to be executed.
    MessageLoop::current()->RunAllPending();
  }

  void RequestFailed() {
    InSequence s;

    WebURLError error;
    error.reason = net::ERR_FAILED;
    data_source_->didFail(NULL, error);

    // Let the tasks to be executed.
    MessageLoop::current()->RunAllPending();
  }

  void Redirect(const char* url) {
    GURL redirectUrl(url);
    WebURLRequest newRequest(redirectUrl);
    WebURLResponse redirectResponse(gurl_);

    data_source_->willSendRequest(url_loader_, newRequest, redirectResponse);

    MessageLoop::current()->RunAllPending();
  }

  void DestroyDataSource() {
    data_source_->Stop(media::NewExpectedClosure());
    MessageLoop::current()->RunAllPending();

    data_source_ = NULL;
  }

  void AsyncRead() {
    for (int i = 0; i < kDataSize; ++i) {
      uint8 buffer[1];

      EXPECT_CALL(*this, ReadCallback(1));
      data_source_->Read(
          i, 1, buffer,
          base::Bind(&SimpleDataSourceTest::ReadCallback,
                     base::Unretained(this)));
      EXPECT_EQ(static_cast<uint8>(data_[i]), buffer[0]);
    }
  }

  MOCK_METHOD1(ReadCallback, void(size_t size));

 protected:
  GURL gurl_;
  scoped_ptr<MessageLoop> message_loop_;
  NiceMock<MockWebURLLoader>* url_loader_;
  scoped_refptr<SimpleDataSource> data_source_;
  StrictMock<media::MockFilterHost> host_;

  MockWebFrameClient client_;
  WebView* view_;

  char data_[kDataSize];

  DISALLOW_COPY_AND_ASSIGN(SimpleDataSourceTest);
};

TEST_F(SimpleDataSourceTest, InitializeHTTP) {
  InitializeDataSource(kHttpUrl,
                       media::NewExpectedStatusCB(media::PIPELINE_OK));
  RequestSucceeded(false);
  DestroyDataSource();
}

TEST_F(SimpleDataSourceTest, InitializeHTTPS) {
  InitializeDataSource(kHttpsUrl,
                       media::NewExpectedStatusCB(media::PIPELINE_OK));
  RequestSucceeded(false);
  DestroyDataSource();
}

TEST_F(SimpleDataSourceTest, InitializeFile) {
  InitializeDataSource(kFileUrl,
                       media::NewExpectedStatusCB(media::PIPELINE_OK));
  RequestSucceeded(true);
  DestroyDataSource();
}

TEST_F(SimpleDataSourceTest, InitializeData) {
  url_loader_ = new NiceMock<MockWebURLLoader>();

  data_source_ = new SimpleDataSource(MessageLoop::current(),
                                      view_->mainFrame());
  // There is no need to provide a message loop to data source.
  data_source_->set_host(&host_);
  data_source_->SetURLLoaderForTest(url_loader_);

  EXPECT_CALL(host_, SetLoaded(true));
  EXPECT_CALL(host_, SetTotalBytes(sizeof(kDataUrlDecoded)));
  EXPECT_CALL(host_, SetBufferedBytes(sizeof(kDataUrlDecoded)));

  data_source_->Initialize(kDataUrl,
      media::NewExpectedStatusCB(media::PIPELINE_OK));
  MessageLoop::current()->RunAllPending();

  DestroyDataSource();
}

TEST_F(SimpleDataSourceTest, RequestFailed) {
  InitializeDataSource(kHttpUrl,
      media::NewExpectedStatusCB(media::PIPELINE_ERROR_NETWORK));
  RequestFailed();
  DestroyDataSource();
}

static void OnStatusCB(bool* called, media::PipelineStatus status) {
  *called = true;
}

TEST_F(SimpleDataSourceTest, StopWhenDownloading) {
  // The callback should be deleted, but not executed.
  // TODO(scherkus): should this really be the behaviour?  Seems strange...
  bool was_called = false;
  InitializeDataSource(kHttpUrl, base::Bind(&OnStatusCB, &was_called));

  EXPECT_CALL(*url_loader_, cancel());
  DestroyDataSource();
  EXPECT_FALSE(was_called);
}

TEST_F(SimpleDataSourceTest, AsyncRead) {
  InitializeDataSource(kFileUrl,
                       media::NewExpectedStatusCB(media::PIPELINE_OK));
  RequestSucceeded(true);
  AsyncRead();
  DestroyDataSource();
}

// NOTE: This test will need to be reworked a little once
// http://code.google.com/p/chromium/issues/detail?id=72578
// is fixed.
TEST_F(SimpleDataSourceTest, HasSingleOrigin) {
  // Make sure no redirect case works as expected.
  InitializeDataSource(kHttpUrl,
                       media::NewExpectedStatusCB(media::PIPELINE_OK));
  RequestSucceeded(false);
  EXPECT_TRUE(data_source_->HasSingleOrigin());
  DestroyDataSource();

  // Test redirect to the same domain.
  InitializeDataSource(kHttpUrl,
                       media::NewExpectedStatusCB(media::PIPELINE_OK));
  Redirect(kHttpRedirectToSameDomainUrl1);
  RequestSucceeded(false);
  EXPECT_TRUE(data_source_->HasSingleOrigin());
  DestroyDataSource();

  // Test redirect twice to the same domain.
  InitializeDataSource(kHttpUrl,
                       media::NewExpectedStatusCB(media::PIPELINE_OK));
  Redirect(kHttpRedirectToSameDomainUrl1);
  Redirect(kHttpRedirectToSameDomainUrl2);
  RequestSucceeded(false);
  EXPECT_TRUE(data_source_->HasSingleOrigin());
  DestroyDataSource();

  // Test redirect to a different domain.
  InitializeDataSource(kHttpUrl,
                       media::NewExpectedStatusCB(media::PIPELINE_OK));
  Redirect(kHttpRedirectToDifferentDomainUrl1);
  RequestSucceeded(false);
  EXPECT_FALSE(data_source_->HasSingleOrigin());
  DestroyDataSource();

  // Test redirect to the same domain and then to a different domain.
  InitializeDataSource(kHttpUrl,
                       media::NewExpectedStatusCB(media::PIPELINE_OK));
  Redirect(kHttpRedirectToSameDomainUrl1);
  Redirect(kHttpRedirectToDifferentDomainUrl1);
  RequestSucceeded(false);
  EXPECT_FALSE(data_source_->HasSingleOrigin());
  DestroyDataSource();
}

}  // namespace webkit_media
