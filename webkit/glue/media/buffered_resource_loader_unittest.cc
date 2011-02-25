// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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

namespace {

const char* kHttpUrl = "http://test";
const char kHttpRedirectToSameDomainUrl1[] = "http://test/ing";
const char kHttpRedirectToSameDomainUrl2[] = "http://test/ing2";
const char kHttpRedirectToDifferentDomainUrl1[] = "http://test2";
const char kHttpRedirectToDifferentDomainUrl2[] = "http://test2/ing";

const int kDataSize = 1024;
const int kHttpOK = 200;
const int kHttpPartialContent = 206;

enum NetworkState {
  NONE,
  LOADED,
  LOADING
};

}  // namespace

namespace webkit_glue {

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
    EXPECT_CALL(*this, StartCallback(net::OK));

    WebURLResponse response(gurl_);
    response.setHTTPHeaderField(WebString::fromUTF8("Content-Length"),
                                WebString::fromUTF8(base::StringPrintf("%"
                                    PRId64, instance_size)));
    response.setExpectedContentLength(instance_size);
    response.setHTTPStatusCode(kHttpOK);
    loader_->didReceiveResponse(url_loader_, response);
    EXPECT_EQ(instance_size, loader_->content_length());
    EXPECT_EQ(instance_size, loader_->instance_size());
    EXPECT_FALSE(loader_->partial_response());
  }

  void PartialResponse(int64 first_position, int64 last_position,
                       int64 instance_size) {
    EXPECT_CALL(*this, StartCallback(net::OK));
    int64 content_length = last_position - first_position + 1;

    WebURLResponse response(gurl_);
    response.setHTTPHeaderField(WebString::fromUTF8("Content-Range"),
                                WebString::fromUTF8(base::StringPrintf("bytes "
                                            "%" PRId64 "-%" PRId64 "/%" PRId64,
                                            first_position,
                                            last_position,
                                            instance_size)));
    response.setExpectedContentLength(content_length);
    response.setHTTPStatusCode(kHttpPartialContent);
    loader_->didReceiveResponse(url_loader_, response);
    EXPECT_EQ(content_length, loader_->content_length());
    EXPECT_EQ(instance_size, loader_->instance_size());
    EXPECT_TRUE(loader_->partial_response());
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
                            reinterpret_cast<char*>(data_ + position), size);
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

  // Helper method to disallow deferring in |loader_|.
  void DisallowLoaderDefer() {
    if (loader_->deferred_) {
      EXPECT_CALL(*url_loader_, setDefersLoading(false));
      EXPECT_CALL(*this, NetworkCallback());
    }
    loader_->SetAllowDefer(false);
  }

  // Helper method to allow deferring in |loader_|.
  void AllowLoaderDefer() {
    loader_->SetAllowDefer(true);
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
  FullResponse(1024);
  StopWhenLoad();
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

// Tests the logic of caching data to disk when media is paused.
TEST_F(BufferedResourceLoaderTest, AllowDefer_NoDataReceived) {
  Initialize(kHttpUrl, 10, 99);
  SetLoaderBuffer(10, 20);
  Start();
  PartialResponse(10, 99, 100);

  // Start in undeferred state, then disallow defer, then allow defer
  // without receiving data in between.
  DisallowLoaderDefer();
  AllowLoaderDefer();
  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, AllowDefer_ReadSameWindow) {
  Initialize(kHttpUrl, 10, 99);
  SetLoaderBuffer(10, 20);
  Start();
  PartialResponse(10, 99, 100);

  uint8 buffer[10];

  // Start in undeferred state, disallow defer, receive data but don't shift
  // buffer window, then allow defer and read.
  DisallowLoaderDefer();
  WriteLoader(10, 10);
  AllowLoaderDefer();

  EXPECT_CALL(*this, ReadCallback(10));
  ReadLoader(10, 10, buffer);
  VerifyBuffer(buffer, 10, 10);
  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, AllowDefer_ReadPastWindow) {
  Initialize(kHttpUrl, 10, 99);
  SetLoaderBuffer(10, 20);
  Start();
  PartialResponse(10, 99, 100);

  uint8 buffer[10];

  // Not deferred, disallow defer, received data and shift buffer window,
  // allow defer, then read in area outside of buffer window.
  DisallowLoaderDefer();
  WriteLoader(10, 10);
  WriteLoader(20, 50);
  AllowLoaderDefer();

  EXPECT_CALL(*this, ReadCallback(net::ERR_CACHE_MISS));
  ReadLoader(10, 10, buffer);
  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, AllowDefer_DeferredNoDataReceived) {
  Initialize(kHttpUrl, 10, 99);
  SetLoaderBuffer(10, 20);
  Start();
  PartialResponse(10, 99, 100);

  uint8 buffer[10];

  // Start in deferred state, then disallow defer, receive no data, and
  // allow defer and read.
  EXPECT_CALL(*url_loader_, setDefersLoading(true));
  EXPECT_CALL(*this, NetworkCallback());
  WriteLoader(10, 40);

  DisallowLoaderDefer();
  AllowLoaderDefer();

  EXPECT_CALL(*this, ReadCallback(10));
  ReadLoader(20, 10, buffer);
  VerifyBuffer(buffer, 20, 10);
  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, AllowDefer_DeferredReadSameWindow) {
  Initialize(kHttpUrl, 10, 99);
  SetLoaderBuffer(10, 20);
  Start();
  PartialResponse(10, 99, 100);

  uint8 buffer[10];

  // Start in deferred state, disallow defer, receive data and shift buffer
  // window, allow defer, and read in a place that's still in the window.
  EXPECT_CALL(*url_loader_, setDefersLoading(true));
  EXPECT_CALL(*this, NetworkCallback());
  WriteLoader(10, 30);

  DisallowLoaderDefer();
  WriteLoader(40, 5);
  AllowLoaderDefer();

  EXPECT_CALL(*this, ReadCallback(10));
  ReadLoader(20, 10, buffer);
  VerifyBuffer(buffer, 20, 10);
  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, AllowDefer_DeferredReadPastWindow) {
  Initialize(kHttpUrl, 10, 99);
  SetLoaderBuffer(10, 20);
  Start();
  PartialResponse(10, 99, 100);

  uint8 buffer[10];

  // Start in deferred state, disallow defer, receive data and shift buffer
  // window, allow defer, and read outside of the buffer window.
  EXPECT_CALL(*url_loader_, setDefersLoading(true));
  EXPECT_CALL(*this, NetworkCallback());
  WriteLoader(10, 40);

  DisallowLoaderDefer();
  WriteLoader(50, 20);
  WriteLoader(70, 40);
  AllowLoaderDefer();

  EXPECT_CALL(*this, ReadCallback(net::ERR_CACHE_MISS));
  ReadLoader(20, 5, buffer);
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
  EXPECT_CALL(*this, StartCallback(net::ERR_ADDRESS_INVALID));
  Redirect(kHttpRedirectToDifferentDomainUrl1);
  EXPECT_FALSE(loader_->HasSingleOrigin());
  StopWhenLoad();

  // Test redirect to the same domain and then to a different domain.
  Initialize(kHttpUrl, -1, -1);
  Start();
  Redirect(kHttpRedirectToSameDomainUrl1);
  EXPECT_CALL(*this, StartCallback(net::ERR_ADDRESS_INVALID));
  Redirect(kHttpRedirectToDifferentDomainUrl1);
  EXPECT_FALSE(loader_->HasSingleOrigin());
  StopWhenLoad();
}

// TODO(hclam): add unit test for defer loading.

}  // namespace webkit_glue
