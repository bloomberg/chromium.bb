// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/format_macros.h"
#include "base/stringprintf.h"
#include "net/base/net_errors.h"
#include "net/http/http_util.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrameClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLError.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLResponse.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "webkit/glue/media/buffered_resource_loader.h"
#include "webkit/mocks/mock_webframe.h"
#include "webkit/mocks/mock_weburlloader.h"

using ::testing::_;
using ::testing::Assign;
using ::testing::AtLeast;
using ::testing::DeleteArg;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SetArgumentPointee;
using ::testing::StrictMock;
using ::testing::NiceMock;
using ::testing::WithArgs;

using WebKit::WebURLError;
using WebKit::WebFrameClient;
using WebKit::WebURLResponse;
using WebKit::WebView;

namespace webkit_glue {

static const char* kHttpUrl = "http://test";
static const char kHttpRedirectToSameDomainUrl1[] = "http://test/ing";
static const char kHttpRedirectToSameDomainUrl2[] = "http://test/ing2";
static const char kHttpRedirectToDifferentDomainUrl1[] = "http://test2";
static const char kHttpRedirectToDifferentDomainUrl2[] = "http://test2/ing";

static const int kDataSize = 1024;
static const int kHttpOK = 200;
static const int kHttpPartialContent = 206;

enum NetworkState {
  NONE,
  LOADED,
  LOADING
};

// Submit a request completed event to the resource loader due to request
// being canceled. Pretending the event is from external.
ACTION_P(RequestCanceled, loader) {
  WebURLError error;
  error.reason = net::ERR_ABORTED;
  error.domain = WebString::fromUTF8(net::kErrorDomain);
  loader->didFail(NULL, error);
}

class BufferedResourceLoaderTest : public testing::Test {
 public:
  BufferedResourceLoaderTest() {
    for (int i = 0; i < kDataSize; ++i)
      data_[i] = i;
  }

  virtual ~BufferedResourceLoaderTest() {
  }

  void Initialize(const char* url, int first_position, int last_position) {
    gurl_ = GURL(url);
    first_position_ = first_position;
    last_position_ = last_position;

    frame_.reset(new NiceMock<MockWebFrame>());

    url_loader_ = new NiceMock<MockWebURLLoader>();
    loader_ = new BufferedResourceLoader(gurl_,
                                         first_position_, last_position_);
    loader_->SetURLLoaderForTest(url_loader_);
  }

  void SetLoaderBuffer(size_t forward_capacity, size_t backward_capacity) {
    loader_->buffer_.reset(
        new media::SeekableBuffer(backward_capacity, forward_capacity));
  }

  void Start() {
    InSequence s;
    EXPECT_CALL(*url_loader_, loadAsynchronously(_, loader_.get()));
    loader_->Start(
        NewCallback(this, &BufferedResourceLoaderTest::StartCallback),
        NewCallback(this, &BufferedResourceLoaderTest::NetworkCallback),
        frame_.get());
  }

  void FullResponse(int64 instance_size) {
    FullResponse(instance_size, net::OK);
  }

  void FullResponse(int64 instance_size, int status) {
    EXPECT_CALL(*this, StartCallback(status));
    if (status != net::OK) {
      EXPECT_CALL(*url_loader_, cancel())
          .WillOnce(RequestCanceled(loader_));
    }

    WebURLResponse response(gurl_);
    response.setHTTPHeaderField(WebString::fromUTF8("Content-Length"),
                                WebString::fromUTF8(base::StringPrintf("%"
                                    PRId64, instance_size)));
    response.setExpectedContentLength(instance_size);
    response.setHTTPStatusCode(kHttpOK);
    loader_->didReceiveResponse(url_loader_, response);

    if (status == net::OK) {
      EXPECT_EQ(instance_size, loader_->content_length());
      EXPECT_EQ(instance_size, loader_->instance_size());
    }

    EXPECT_FALSE(loader_->range_supported());
  }

  void PartialResponse(int64 first_position, int64 last_position,
                       int64 instance_size) {
    PartialResponse(first_position, last_position, instance_size, false, true);
  }

  void PartialResponse(int64 first_position, int64 last_position,
                       int64 instance_size, bool chunked, bool accept_ranges) {
    EXPECT_CALL(*this, StartCallback(net::OK));

    WebURLResponse response(gurl_);
    response.setHTTPHeaderField(WebString::fromUTF8("Content-Range"),
                                WebString::fromUTF8(base::StringPrintf("bytes "
                                            "%" PRId64 "-%" PRId64 "/%" PRId64,
                                            first_position,
                                            last_position,
                                            instance_size)));

    // HTTP 1.1 doesn't permit Content-Length with Transfer-Encoding: chunked.
    int64 content_length = -1;
    if (chunked) {
      response.setHTTPHeaderField(WebString::fromUTF8("Transfer-Encoding"),
                                  WebString::fromUTF8("chunked"));
    } else {
      content_length = last_position - first_position + 1;
    }
    response.setExpectedContentLength(content_length);

    // A server isn't required to return Accept-Ranges even though it might.
    if (accept_ranges) {
      response.setHTTPHeaderField(WebString::fromUTF8("Accept-Ranges"),
                                  WebString::fromUTF8("bytes"));
    }

    response.setHTTPStatusCode(kHttpPartialContent);
    loader_->didReceiveResponse(url_loader_, response);

    // XXX: what's the difference between these two? For example in the chunked
    // range request case, Content-Length is unspecified (because it's chunked)
    // but Content-Range: a-b/c can be returned, where c == Content-Length
    //
    // Can we eliminate one?
    EXPECT_EQ(content_length, loader_->content_length());
    EXPECT_EQ(instance_size, loader_->instance_size());

    // A valid partial response should always result in this being true.
    EXPECT_TRUE(loader_->range_supported());
  }

  void Redirect(const char* url) {
    GURL redirectUrl(url);
    WebKit::WebURLRequest newRequest(redirectUrl);
    WebKit::WebURLResponse redirectResponse(gurl_);

    loader_->willSendRequest(url_loader_, newRequest, redirectResponse);

    MessageLoop::current()->RunAllPending();
  }

  void StopWhenLoad() {
    InSequence s;
    EXPECT_CALL(*url_loader_, cancel())
        .WillOnce(RequestCanceled(loader_));
    loader_->Stop();
    loader_ = NULL;
  }

  // Helper method to write to |loader_| from |data_|.
  void WriteLoader(int position, int size) {
    EXPECT_CALL(*this, NetworkCallback())
        .RetiresOnSaturation();
    loader_->didReceiveData(url_loader_,
                            reinterpret_cast<char*>(data_ + position),
                            size,
                            size);
  }

  // Helper method to read from |loader_|.
  void ReadLoader(int64 position, int size, uint8* buffer) {
    loader_->Read(position, size, buffer,
                  NewCallback(this, &BufferedResourceLoaderTest::ReadCallback));
  }

  // Verifis that data in buffer[0...size] is equal to data_[pos...pos+size].
  void VerifyBuffer(uint8* buffer, int pos, int size) {
    EXPECT_EQ(0, memcmp(buffer, data_ + pos, size));
  }

  void ConfirmLoaderDeferredState(bool expectedVal) {
    EXPECT_EQ(loader_->deferred_, expectedVal);
  }

  MOCK_METHOD1(StartCallback, void(int error));
  MOCK_METHOD1(ReadCallback, void(int error));
  MOCK_METHOD0(NetworkCallback, void());

 protected:
  GURL gurl_;
  int64 first_position_;
  int64 last_position_;

  scoped_refptr<BufferedResourceLoader> loader_;
  NiceMock<MockWebURLLoader>* url_loader_;
  scoped_ptr<NiceMock<MockWebFrame> > frame_;

  uint8 data_[kDataSize];

 private:
  DISALLOW_COPY_AND_ASSIGN(BufferedResourceLoaderTest);
};

TEST_F(BufferedResourceLoaderTest, StartStop) {
  Initialize(kHttpUrl, -1, -1);
  Start();
  StopWhenLoad();
}

// Tests that a bad HTTP response is recived, e.g. file not found.
TEST_F(BufferedResourceLoaderTest, BadHttpResponse) {
  Initialize(kHttpUrl, -1, -1);
  Start();

  EXPECT_CALL(*this, StartCallback(net::ERR_FAILED));
  EXPECT_CALL(*url_loader_, cancel())
      .WillOnce(RequestCanceled(loader_));

  WebURLResponse response(gurl_);
  response.setHTTPStatusCode(404);
  response.setHTTPStatusText("Not Found\n");
  loader_->didReceiveResponse(url_loader_, response);
}

// Tests that partial content is requested but not fulfilled.
TEST_F(BufferedResourceLoaderTest, NotPartialResponse) {
  Initialize(kHttpUrl, 100, -1);
  Start();
  FullResponse(1024, net::ERR_INVALID_RESPONSE);
}

// Tests that a 200 response is received.
TEST_F(BufferedResourceLoaderTest, FullResponse) {
  Initialize(kHttpUrl, -1, -1);
  Start();
  FullResponse(1024);
  StopWhenLoad();
}

// Tests that a partial content response is received.
TEST_F(BufferedResourceLoaderTest, PartialResponse) {
  Initialize(kHttpUrl, 100, 200);
  Start();
  PartialResponse(100, 200, 1024);
  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, PartialResponse_Chunked) {
  Initialize(kHttpUrl, 100, 200);
  Start();
  PartialResponse(100, 200, 1024, true, true);
  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, PartialResponse_NoAcceptRanges) {
  Initialize(kHttpUrl, 100, 200);
  Start();
  PartialResponse(100, 200, 1024, false, false);
  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, PartialResponse_ChunkedNoAcceptRanges) {
  Initialize(kHttpUrl, 100, 200);
  Start();
  PartialResponse(100, 200, 1024, true, false);
  StopWhenLoad();
}

// Tests that an invalid partial response is received.
TEST_F(BufferedResourceLoaderTest, InvalidPartialResponse) {
  Initialize(kHttpUrl, 0, 10);
  Start();

  EXPECT_CALL(*this, StartCallback(net::ERR_INVALID_RESPONSE));
  EXPECT_CALL(*url_loader_, cancel())
      .WillOnce(RequestCanceled(loader_));

  WebURLResponse response(gurl_);
  response.setHTTPHeaderField(WebString::fromUTF8("Content-Range"),
                              WebString::fromUTF8(base::StringPrintf("bytes "
                                  "%d-%d/%d", 1, 10, 1024)));
  response.setExpectedContentLength(10);
  response.setHTTPStatusCode(kHttpPartialContent);
  loader_->didReceiveResponse(url_loader_, response);
}

// Tests the logic of sliding window for data buffering and reading.
TEST_F(BufferedResourceLoaderTest, BufferAndRead) {
  Initialize(kHttpUrl, 10, 29);
  loader_->UpdateDeferStrategy(BufferedResourceLoader::kThresholdDefer);
  Start();
  PartialResponse(10, 29, 30);

  uint8 buffer[10];
  InSequence s;

  // Writes 10 bytes and read them back.
  WriteLoader(10, 10);
  EXPECT_CALL(*this, ReadCallback(10));
  ReadLoader(10, 10, buffer);
  VerifyBuffer(buffer, 10, 10);

  // Writes 10 bytes and read 2 times.
  WriteLoader(20, 10);
  EXPECT_CALL(*this, ReadCallback(5));
  ReadLoader(20, 5, buffer);
  VerifyBuffer(buffer, 20, 5);
  EXPECT_CALL(*this, ReadCallback(5));
  ReadLoader(25, 5, buffer);
  VerifyBuffer(buffer, 25, 5);

  // Read backward within buffer.
  EXPECT_CALL(*this, ReadCallback(10));
  ReadLoader(10, 10, buffer);
  VerifyBuffer(buffer, 10, 10);

  // Read backward outside buffer.
  EXPECT_CALL(*this, ReadCallback(net::ERR_CACHE_MISS));
  ReadLoader(9, 10, buffer);

  // Response has completed.
  EXPECT_CALL(*this, NetworkCallback());
  loader_->didFinishLoading(url_loader_, 0);

  // Try to read 10 from position 25 will just return with 5 bytes.
  EXPECT_CALL(*this, ReadCallback(5));
  ReadLoader(25, 10, buffer);
  VerifyBuffer(buffer, 25, 5);

  // Try to read outside buffered range after request has completed.
  EXPECT_CALL(*this, ReadCallback(net::ERR_CACHE_MISS));
  ReadLoader(5, 10, buffer);

  // Try to read beyond the instance size.
  EXPECT_CALL(*this, ReadCallback(0));
  ReadLoader(30, 10, buffer);
}

TEST_F(BufferedResourceLoaderTest, ReadOutsideBuffer) {
  Initialize(kHttpUrl, 10, 0x00FFFFFF);
  loader_->UpdateDeferStrategy(BufferedResourceLoader::kThresholdDefer);
  Start();
  PartialResponse(10, 0x00FFFFFF, 0x01000000);

  uint8 buffer[10];
  InSequence s;

  // Read very far aheard will get a cache miss.
  EXPECT_CALL(*this, ReadCallback(net::ERR_CACHE_MISS));
  ReadLoader(0x00FFFFFF, 1, buffer);

  // The following call will not call ReadCallback() because it is waiting for
  // data to arrive.
  ReadLoader(10, 10, buffer);

  // Writing to loader will fulfill the read request.
  EXPECT_CALL(*this, ReadCallback(10));
  WriteLoader(10, 20);
  VerifyBuffer(buffer, 10, 10);

  // The following call cannot be fulfilled now.
  ReadLoader(25, 10, buffer);

  EXPECT_CALL(*this, ReadCallback(5));
  EXPECT_CALL(*this, NetworkCallback());
  loader_->didFinishLoading(url_loader_, 0);
}

TEST_F(BufferedResourceLoaderTest, RequestFailedWhenRead) {
  Initialize(kHttpUrl, 10, 29);
  Start();
  PartialResponse(10, 29, 30);

  uint8 buffer[10];
  InSequence s;

  ReadLoader(10, 10, buffer);
  EXPECT_CALL(*this, ReadCallback(net::ERR_FAILED));
  EXPECT_CALL(*this, NetworkCallback());
  WebURLError error;
  error.reason = net::ERR_FAILED;
  loader_->didFail(url_loader_, error);
}

// Tests the data buffering logic of NeverDefer strategy.
TEST_F(BufferedResourceLoaderTest, NeverDeferStrategy) {
  Initialize(kHttpUrl, 10, 99);
  SetLoaderBuffer(10, 20);
  loader_->UpdateDeferStrategy(BufferedResourceLoader::kNeverDefer);
  Start();
  PartialResponse(10, 99, 100);

  uint8 buffer[10];

  // Read past the buffer size; should not defer regardless.
  WriteLoader(10, 10);
  WriteLoader(20, 50);
  ConfirmLoaderDeferredState(false);

  // Should move past window.
  EXPECT_CALL(*this, ReadCallback(net::ERR_CACHE_MISS));
  ReadLoader(10, 10, buffer);

  StopWhenLoad();
}

// Tests the data buffering logic of ReadThenDefer strategy.
TEST_F(BufferedResourceLoaderTest, ReadThenDeferStrategy) {
  Initialize(kHttpUrl, 10, 99);
  SetLoaderBuffer(10, 20);
  loader_->UpdateDeferStrategy(BufferedResourceLoader::kReadThenDefer);
  Start();
  PartialResponse(10, 99, 100);

  uint8 buffer[10];

  // Make an outstanding read request.
  // We should disable deferring after the read request, so expect
  // a network event.
  EXPECT_CALL(*this, NetworkCallback());
  ReadLoader(10, 10, buffer);

  // Receive almost enough data to cover, shouldn't defer.
  WriteLoader(10, 9);
  ConfirmLoaderDeferredState(false);

  // As soon as we have received enough data to fulfill the read, defer.
  EXPECT_CALL(*this, NetworkCallback());
  EXPECT_CALL(*this, ReadCallback(10));
  WriteLoader(19, 1);

  ConfirmLoaderDeferredState(true);
  VerifyBuffer(buffer, 10, 10);

  StopWhenLoad();
}

// Tests the data buffering logic of ThresholdDefer strategy.
TEST_F(BufferedResourceLoaderTest, ThresholdDeferStrategy) {
  Initialize(kHttpUrl, 10, 99);
  SetLoaderBuffer(10, 20);
  loader_->UpdateDeferStrategy(BufferedResourceLoader::kThresholdDefer);
  Start();
  PartialResponse(10, 99, 100);

  uint8 buffer[10];

  WriteLoader(10, 5);
  // Haven't reached threshold, don't defer.
  ConfirmLoaderDeferredState(false);

  // We're at the threshold now, let's defer.
  EXPECT_CALL(*this, NetworkCallback());
  WriteLoader(15, 5);
  ConfirmLoaderDeferredState(true);

  // Now we've read over half of the buffer, disable deferring.
  EXPECT_CALL(*this, ReadCallback(6));
  EXPECT_CALL(*this, NetworkCallback());
  ReadLoader(10, 6, buffer);

  ConfirmLoaderDeferredState(false);
  VerifyBuffer(buffer, 10, 6);

  StopWhenLoad();
}

// NOTE: This test will need to be reworked a little once
// http://code.google.com/p/chromium/issues/detail?id=72578
// is fixed.
TEST_F(BufferedResourceLoaderTest, HasSingleOrigin) {
  // Make sure no redirect case works as expected.
  Initialize(kHttpUrl, -1, -1);
  Start();
  FullResponse(1024);
  EXPECT_TRUE(loader_->HasSingleOrigin());
  StopWhenLoad();

  // Test redirect to the same domain.
  Initialize(kHttpUrl, -1, -1);
  Start();
  Redirect(kHttpRedirectToSameDomainUrl1);
  FullResponse(1024);
  EXPECT_TRUE(loader_->HasSingleOrigin());
  StopWhenLoad();

  // Test redirect twice to the same domain.
  Initialize(kHttpUrl, -1, -1);
  Start();
  Redirect(kHttpRedirectToSameDomainUrl1);
  Redirect(kHttpRedirectToSameDomainUrl2);
  FullResponse(1024);
  EXPECT_TRUE(loader_->HasSingleOrigin());
  StopWhenLoad();

  // Test redirect to a different domain.
  Initialize(kHttpUrl, -1, -1);
  Start();
  Redirect(kHttpRedirectToDifferentDomainUrl1);
  EXPECT_FALSE(loader_->HasSingleOrigin());
  StopWhenLoad();

  // Test redirect to the same domain and then to a different domain.
  Initialize(kHttpUrl, -1, -1);
  Start();
  Redirect(kHttpRedirectToSameDomainUrl1);
  Redirect(kHttpRedirectToDifferentDomainUrl1);
  EXPECT_FALSE(loader_->HasSingleOrigin());
  StopWhenLoad();
}

// TODO(hclam): add unit test for defer loading.

}  // namespace webkit_glue
