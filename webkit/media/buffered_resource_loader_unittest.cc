// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/format_macros.h"
#include "base/stringprintf.h"
#include "media/base/media_log.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURLError.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURLResponse.h"
#include "webkit/media/buffered_resource_loader.h"
#include "webkit/mocks/mock_webframeclient.h"
#include "webkit/mocks/mock_weburlloader.h"

using ::testing::InSequence;
using ::testing::Return;
using ::testing::Truly;
using ::testing::NiceMock;

using WebKit::WebString;
using WebKit::WebURLError;
using WebKit::WebURLResponse;
using WebKit::WebView;

using webkit_glue::MockWebFrameClient;
using webkit_glue::MockWebURLLoader;

namespace webkit_media {

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

// Predicate that tests that request disallows compressed data.
static bool CorrectAcceptEncoding(const WebKit::WebURLRequest &request) {
  std::string value = request.httpHeaderField(
      WebString::fromUTF8(net::HttpRequestHeaders::kAcceptEncoding)).utf8();
  return (value.find("identity;q=1") != std::string::npos) &&
         (value.find("*;q=0") != std::string::npos);
}

class BufferedResourceLoaderTest : public testing::Test {
 public:
  BufferedResourceLoaderTest()
      : view_(WebView::create(NULL)) {
    view_->initializeMainFrame(&client_);

    for (int i = 0; i < kDataSize; ++i) {
      data_[i] = i;
    }
  }

  virtual ~BufferedResourceLoaderTest() {
    view_->close();
  }

  void Initialize(const char* url, int first_position, int last_position) {
    gurl_ = GURL(url);
    first_position_ = first_position;
    last_position_ = last_position;

    url_loader_ = new NiceMock<MockWebURLLoader>();
    loader_ = new BufferedResourceLoader(
        gurl_, first_position_, last_position_,
        BufferedResourceLoader::kThresholdDefer, 0, 0,
        new media::MediaLog());
    loader_->SetURLLoaderForTest(url_loader_);
  }

  void SetLoaderBuffer(size_t forward_capacity, size_t backward_capacity) {
    loader_->buffer_.reset(
        new media::SeekableBuffer(backward_capacity, forward_capacity));
  }

  void Start() {
    InSequence s;
    EXPECT_CALL(*url_loader_, loadAsynchronously(Truly(CorrectAcceptEncoding),
                                                 loader_.get()));
    loader_->Start(
        base::Bind(&BufferedResourceLoaderTest::StartCallback,
                   base::Unretained(this)),
        base::Bind(&BufferedResourceLoaderTest::NetworkCallback,
                   base::Unretained(this)),
        view_->mainFrame());
  }

  void FullResponse(int64 instance_size) {
    FullResponse(instance_size, net::OK);
  }

  void FullResponse(int64 instance_size, int status) {
    EXPECT_CALL(*this, StartCallback(status));

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
    EXPECT_CALL(*url_loader_, cancel());
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

  void WriteData(int size) {
    EXPECT_CALL(*this, NetworkCallback())
        .RetiresOnSaturation();

    scoped_array<char> data(new char[size]);
    loader_->didReceiveData(url_loader_, data.get(), size, size);
  }

  void WriteUntilThreshold() {
    size_t buffered = loader_->buffer_->forward_bytes();
    size_t capacity = loader_->buffer_->forward_capacity();
    CHECK_LT(buffered, capacity);

    EXPECT_CALL(*this, NetworkCallback());
    WriteData(capacity - buffered);
    ConfirmLoaderDeferredState(true);
  }

  // Helper method to read from |loader_|.
  void ReadLoader(int64 position, int size, uint8* buffer) {
    loader_->Read(position, size, buffer,
                  base::Bind(&BufferedResourceLoaderTest::ReadCallback,
                             base::Unretained(this)));
  }

  // Verifies that data in buffer[0...size] is equal to data_[pos...pos+size].
  void VerifyBuffer(uint8* buffer, int pos, int size) {
    EXPECT_EQ(0, memcmp(buffer, data_ + pos, size));
  }

  void ConfirmLoaderOffsets(int64 expected_offset,
                            int expected_first_offset,
                            int expected_last_offset) {
    EXPECT_EQ(loader_->offset_, expected_offset);
    EXPECT_EQ(loader_->first_offset_, expected_first_offset);
    EXPECT_EQ(loader_->last_offset_, expected_last_offset);
  }

  void ConfirmBufferState(size_t backward_bytes,
                          size_t backward_capacity,
                          size_t forward_bytes,
                          size_t forward_capacity) {
    EXPECT_EQ(backward_bytes, loader_->buffer_->backward_bytes());
    EXPECT_EQ(backward_capacity, loader_->buffer_->backward_capacity());
    EXPECT_EQ(forward_bytes, loader_->buffer_->forward_bytes());
    EXPECT_EQ(forward_capacity, loader_->buffer_->forward_capacity());
  }

  void ConfirmLoaderBufferBackwardCapacity(size_t expected_backward_capacity) {
    EXPECT_EQ(loader_->buffer_->backward_capacity(),
              expected_backward_capacity);
  }

  void ConfirmLoaderBufferForwardCapacity(size_t expected_forward_capacity) {
    EXPECT_EQ(loader_->buffer_->forward_capacity(), expected_forward_capacity);
  }

  void ConfirmLoaderDeferredState(bool expectedVal) {
    EXPECT_EQ(loader_->active_loader_->deferred(), expectedVal);
  }

  // Makes sure the |loader_| buffer window is in a reasonable range.
  void CheckBufferWindowBounds() {
    // Corresponds to value defined in buffered_resource_loader.cc.
    static const size_t kMinBufferCapacity = 2 * 1024 * 1024;
    EXPECT_GE(loader_->buffer_->forward_capacity(), kMinBufferCapacity);
    EXPECT_GE(loader_->buffer_->backward_capacity(), kMinBufferCapacity);

    // Corresponds to value defined in buffered_resource_loader.cc.
    static const size_t kMaxBufferCapacity = 20 * 1024 * 1024;
    EXPECT_LE(loader_->buffer_->forward_capacity(), kMaxBufferCapacity);
    EXPECT_LE(loader_->buffer_->backward_capacity(), kMaxBufferCapacity);
  }

  MOCK_METHOD1(StartCallback, void(int error));
  MOCK_METHOD1(ReadCallback, void(int error));
  MOCK_METHOD0(NetworkCallback, void());

  // Accessors for private variables on |loader_|.
  size_t forward_bytes() { return loader_->buffer_->forward_bytes(); }
  size_t forward_capacity() { return loader_->buffer_->forward_capacity(); }

 protected:
  GURL gurl_;
  int64 first_position_;
  int64 last_position_;

  scoped_refptr<BufferedResourceLoader> loader_;
  NiceMock<MockWebURLLoader>* url_loader_;

  MockWebFrameClient client_;
  WebView* view_;

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

  WebURLResponse response(gurl_);
  response.setHTTPStatusCode(404);
  response.setHTTPStatusText("Not Found\n");
  loader_->didReceiveResponse(url_loader_, response);
  StopWhenLoad();
}

// Tests that partial content is requested but not fulfilled.
TEST_F(BufferedResourceLoaderTest, NotPartialResponse) {
  Initialize(kHttpUrl, 100, -1);
  Start();
  FullResponse(1024, net::ERR_INVALID_RESPONSE);
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

  WebURLResponse response(gurl_);
  response.setHTTPHeaderField(WebString::fromUTF8("Content-Range"),
                              WebString::fromUTF8(base::StringPrintf("bytes "
                                  "%d-%d/%d", 1, 10, 1024)));
  response.setExpectedContentLength(10);
  response.setHTTPStatusCode(kHttpPartialContent);
  loader_->didReceiveResponse(url_loader_, response);
  StopWhenLoad();
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

// Tests the logic of expanding the data buffer for large reads.
TEST_F(BufferedResourceLoaderTest, ReadExtendBuffer) {
  Initialize(kHttpUrl, 10, 0x014FFFFFF);
  SetLoaderBuffer(10, 20);
  Start();
  PartialResponse(10, 0x014FFFFFF, 0x01500000);

  // Don't test for network callbacks (covered by *Strategy tests).
  EXPECT_CALL(*this, NetworkCallback())
      .WillRepeatedly(Return());

  uint8 buffer[20];
  InSequence s;

  // Write more than forward capacity and read it back. Ensure forward capacity
  // gets reset.
  WriteLoader(10, 20);
  EXPECT_CALL(*this, ReadCallback(20));
  ReadLoader(10, 20, buffer);

  VerifyBuffer(buffer, 10, 20);
  ConfirmLoaderBufferForwardCapacity(10);

  // Make and outstanding read request larger than forward capacity. Ensure
  // forward capacity gets extended.
  ReadLoader(30, 20, buffer);

  ConfirmLoaderBufferForwardCapacity(20);

  // Fulfill outstanding request. Ensure forward capacity gets reset.
  EXPECT_CALL(*this, ReadCallback(20));
  WriteLoader(30, 20);

  VerifyBuffer(buffer, 30, 20);
  ConfirmLoaderBufferForwardCapacity(10);

  // Try to read further ahead than kForwardWaitThreshold allows. Ensure
  // forward capacity is not changed.
  EXPECT_CALL(*this, ReadCallback(net::ERR_CACHE_MISS));
  ReadLoader(0x00300000, 1, buffer);

  ConfirmLoaderBufferForwardCapacity(10);

  // Try to read more than maximum forward capacity. Ensure forward capacity is
  // not changed.
  EXPECT_CALL(*this, ReadCallback(net::ERR_FAILED));
  ReadLoader(30, 0x01400001, buffer);

  ConfirmLoaderBufferForwardCapacity(10);

  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, ReadOutsideBuffer) {
  Initialize(kHttpUrl, 10, 0x00FFFFFF);
  Start();
  PartialResponse(10, 0x00FFFFFF, 0x01000000);

  uint8 buffer[10];
  InSequence s;

  // Read very far ahead will get a cache miss.
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

  EXPECT_CALL(*this, NetworkCallback());
  EXPECT_CALL(*this, ReadCallback(5));
  loader_->didFinishLoading(url_loader_, 0);
}

TEST_F(BufferedResourceLoaderTest, RequestFailedWhenRead) {
  Initialize(kHttpUrl, 10, 29);
  Start();
  PartialResponse(10, 29, 30);

  uint8 buffer[10];
  InSequence s;

  ReadLoader(10, 10, buffer);
  EXPECT_CALL(*this, NetworkCallback());
  EXPECT_CALL(*this, ReadCallback(net::ERR_FAILED));
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

  // Read again which should disable deferring since there should be nothing
  // left in our internal buffer.
  EXPECT_CALL(*this, NetworkCallback());
  ReadLoader(20, 10, buffer);

  ConfirmLoaderDeferredState(false);

  // Over-fulfill requested bytes, then deferring should be enabled again.
  EXPECT_CALL(*this, NetworkCallback());
  EXPECT_CALL(*this, ReadCallback(10));
  WriteLoader(20, 40);

  ConfirmLoaderDeferredState(true);
  VerifyBuffer(buffer, 20, 10);

  // Read far ahead, which should disable deferring. In this case we still have
  // bytes in our internal buffer.
  EXPECT_CALL(*this, NetworkCallback());
  ReadLoader(80, 10, buffer);

  ConfirmLoaderDeferredState(false);

  // Fulfill requested bytes, then deferring should be enabled again.
  EXPECT_CALL(*this, NetworkCallback());
  EXPECT_CALL(*this, ReadCallback(10));
  WriteLoader(60, 40);

  ConfirmLoaderDeferredState(true);
  VerifyBuffer(buffer, 80, 10);

  StopWhenLoad();
}

// Tests the data buffering logic of ThresholdDefer strategy.
TEST_F(BufferedResourceLoaderTest, ThresholdDeferStrategy) {
  Initialize(kHttpUrl, 10, 99);
  SetLoaderBuffer(10, 20);
  Start();
  PartialResponse(10, 99, 100);

  uint8 buffer[10];
  InSequence s;

  // Initial expectation: we're not deferring.
  ConfirmLoaderDeferredState(false);

  // Write half of threshold: keep not deferring.
  WriteData(5);
  ConfirmLoaderDeferredState(false);

  // Write rest of space until threshold: start deferring.
  EXPECT_CALL(*this, NetworkCallback());
  WriteData(5);
  ConfirmLoaderDeferredState(true);

  // Read a little from the buffer: keep deferring.
  EXPECT_CALL(*this, ReadCallback(2));
  ReadLoader(10, 2, buffer);
  ConfirmLoaderDeferredState(true);

  // Read a little more and go under threshold: stop deferring.
  EXPECT_CALL(*this, ReadCallback(4));
  EXPECT_CALL(*this, NetworkCallback());
  ReadLoader(12, 4, buffer);
  ConfirmLoaderDeferredState(false);

  // Write rest of space until threshold: start deferring.
  EXPECT_CALL(*this, NetworkCallback());
  WriteData(6);
  ConfirmLoaderDeferredState(true);

  // Read a little from the buffer: keep deferring.
  EXPECT_CALL(*this, ReadCallback(4));
  ReadLoader(16, 4, buffer);
  ConfirmLoaderDeferredState(true);

  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, Tricky_ReadForwardsPastBuffered) {
  Initialize(kHttpUrl, 10, 99);
  SetLoaderBuffer(10, 10);
  Start();
  PartialResponse(10, 99, 100);

  uint8 buffer[256];
  InSequence s;

  // PRECONDITION
  WriteUntilThreshold();
  EXPECT_CALL(*this, ReadCallback(4));
  ReadLoader(10, 4, buffer);
  ConfirmBufferState(4, 10, 6, 10);
  ConfirmLoaderOffsets(14, 0, 0);
  ConfirmLoaderDeferredState(true);

  // *** TRICKY BUSINESS, PT. I ***
  // Read past buffered: stop deferring.
  //
  // In order for the read to complete we must:
  //   1) Stop deferring to receive more data.
  //
  // BEFORE
  //   offset=14 [xxxxxx____]
  //                    ^^^^ requested 4 bytes @ offset 20
  // AFTER
  //   offset=24 [__________]
  //
  EXPECT_CALL(*this, NetworkCallback());
  ReadLoader(20, 4, buffer);
  ConfirmLoaderDeferredState(false);

  // Write a little, make sure we didn't start deferring.
  WriteData(2);
  ConfirmLoaderDeferredState(false);

  // Write the rest, read should complete.
  EXPECT_CALL(*this, ReadCallback(4));
  WriteData(2);
  ConfirmLoaderDeferredState(false);

  // POSTCONDITION
  ConfirmBufferState(4, 10, 0, 10);
  ConfirmLoaderOffsets(24, 0, 0);
  ConfirmLoaderDeferredState(false);

  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, Tricky_ReadBackwardsPastBuffered) {
  Initialize(kHttpUrl, 10, 99);
  SetLoaderBuffer(10, 10);
  Start();
  PartialResponse(10, 99, 100);

  uint8 buffer[256];
  InSequence s;

  // PRECONDITION
  WriteUntilThreshold();
  ConfirmBufferState(0, 10, 10, 10);
  ConfirmLoaderOffsets(10, 0, 0);
  ConfirmLoaderDeferredState(true);

  // *** TRICKY BUSINESS, PT. II ***
  // Read backwards a little too much: cache miss.
  //
  // BEFORE
  //   offset=10 [__________|xxxxxxxxxx]
  //                       ^ ^^^ requested 10 bytes @ offset 9
  // AFTER
  //   offset=10 [__________|xxxxxxxxxx]  !!! cache miss !!!
  //
  EXPECT_CALL(*this, ReadCallback(net::ERR_CACHE_MISS));
  ReadLoader(9, 4, buffer);

  // POSTCONDITION
  ConfirmBufferState(0, 10, 10, 10);
  ConfirmLoaderOffsets(10, 0, 0);
  ConfirmLoaderDeferredState(true);

  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, Tricky_SmallReadWithinThreshold) {
  Initialize(kHttpUrl, 10, 99);
  SetLoaderBuffer(10, 10);
  Start();
  PartialResponse(10, 99, 100);

  uint8 buffer[256];
  InSequence s;

  // PRECONDITION
  WriteUntilThreshold();
  ConfirmBufferState(0, 10, 10, 10);
  ConfirmLoaderOffsets(10, 0, 0);
  ConfirmLoaderDeferredState(true);

  // *** TRICKY BUSINESS, PT. III ***
  // Read past forward capacity but within threshold: stop deferring.
  //
  // In order for the read to complete we must:
  //   1) Adjust offset forward to create capacity.
  //   2) Stop deferring to receive more data.
  //
  // BEFORE
  //   offset=10 [xxxxxxxxxx]
  //                             ^^^^ requested 4 bytes @ offset 24
  // ADJUSTED OFFSET
  //   offset=20 [__________]
  //                  ^^^^ requested 4 bytes @ offset 24
  // AFTER
  //   offset=28 [__________]
  //
  EXPECT_CALL(*this, NetworkCallback());
  ReadLoader(24, 4, buffer);
  ConfirmLoaderOffsets(20, 4, 8);
  ConfirmLoaderDeferredState(false);

  // Write a little, make sure we didn't start deferring.
  WriteData(4);
  ConfirmLoaderDeferredState(false);

  // Write the rest, read should complete.
  EXPECT_CALL(*this, ReadCallback(4));
  WriteData(4);
  ConfirmLoaderDeferredState(false);

  // POSTCONDITION
  ConfirmBufferState(8, 10, 0, 10);
  ConfirmLoaderOffsets(28, 0, 0);
  ConfirmLoaderDeferredState(false);

  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, Tricky_LargeReadWithinThreshold) {
  Initialize(kHttpUrl, 10, 99);
  SetLoaderBuffer(10, 10);
  Start();
  PartialResponse(10, 99, 100);

  uint8 buffer[256];
  InSequence s;

  // PRECONDITION
  WriteUntilThreshold();
  ConfirmBufferState(0, 10, 10, 10);
  ConfirmLoaderOffsets(10, 0, 0);
  ConfirmLoaderDeferredState(true);

  // *** TRICKY BUSINESS, PT. IV ***
  // Read a large amount past forward capacity but within
  // threshold: stop deferring.
  //
  // In order for the read to complete we must:
  //   1) Adjust offset forward to create capacity.
  //   2) Expand capacity to make sure we don't defer as data arrives.
  //   3) Stop deferring to receive more data.
  //
  // BEFORE
  //   offset=10 [xxxxxxxxxx]
  //                             ^^^^^^^^^^^^ requested 12 bytes @ offset 24
  // ADJUSTED OFFSET
  //   offset=20 [__________]
  //                  ^^^^^^ ^^^^^^ requested 12 bytes @ offset 24
  // ADJUSTED CAPACITY
  //   offset=20 [________________]
  //                  ^^^^^^^^^^^^ requested 12 bytes @ offset 24
  // AFTER
  //   offset=36 [__________]
  //
  EXPECT_CALL(*this, NetworkCallback());
  ReadLoader(24, 12, buffer);
  ConfirmLoaderOffsets(20, 4, 16);
  ConfirmBufferState(10, 10, 0, 16);
  ConfirmLoaderDeferredState(false);

  // Write a little, make sure we didn't start deferring.
  WriteData(10);
  ConfirmLoaderDeferredState(false);

  // Write the rest, read should complete and capacity should go back to normal.
  EXPECT_CALL(*this, ReadCallback(12));
  WriteData(6);
  ConfirmLoaderBufferForwardCapacity(10);
  ConfirmLoaderDeferredState(false);

  // POSTCONDITION
  ConfirmBufferState(6, 10, 0, 10);
  ConfirmLoaderOffsets(36, 0, 0);
  ConfirmLoaderDeferredState(false);

  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, Tricky_LargeReadBackwards) {
  Initialize(kHttpUrl, 10, 99);
  SetLoaderBuffer(10, 10);
  Start();
  PartialResponse(10, 99, 100);

  uint8 buffer[256];
  InSequence s;

  // PRECONDITION
  WriteUntilThreshold();
  EXPECT_CALL(*this, ReadCallback(10));
  EXPECT_CALL(*this, NetworkCallback());
  ReadLoader(10, 10, buffer);
  WriteUntilThreshold();
  ConfirmBufferState(10, 10, 10, 10);
  ConfirmLoaderOffsets(20, 0, 0);
  ConfirmLoaderDeferredState(true);

  // *** TRICKY BUSINESS, PT. V ***
  // Read a large amount that involves backwards data: stop deferring.
  //
  // In order for the read to complete we must:
  //   1) Adjust offset *backwards* to create capacity.
  //   2) Expand capacity to make sure we don't defer as data arrives.
  //   3) Stop deferring to receive more data.
  //
  // BEFORE
  //   offset=20 [xxxxxxxxxx|xxxxxxxxxx]
  //                    ^^^^ ^^^^^^^^^^ ^^^^ requested 18 bytes @ offset 16
  // ADJUSTED OFFSET
  //   offset=16 [____xxxxxx|xxxxxxxxxx]xxxx
  //                         ^^^^^^^^^^ ^^^^^^^^ requested 18 bytes @ offset 16
  // ADJUSTED CAPACITY
  //   offset=16 [____xxxxxx|xxxxxxxxxxxxxx____]
  //                         ^^^^^^^^^^^^^^^^^^ requested 18 bytes @ offset 16
  // AFTER
  //   offset=34 [xxxxxxxxxx|__________]
  //
  EXPECT_CALL(*this, NetworkCallback());
  ReadLoader(16, 18, buffer);
  ConfirmLoaderOffsets(16, 0, 18);
  ConfirmBufferState(6, 10, 14, 18);
  ConfirmLoaderDeferredState(false);

  // Write a little, make sure we didn't start deferring.
  WriteData(2);
  ConfirmLoaderDeferredState(false);

  // Write the rest, read should complete and capacity should go back to normal.
  EXPECT_CALL(*this, ReadCallback(18));
  WriteData(2);
  ConfirmLoaderBufferForwardCapacity(10);
  ConfirmLoaderDeferredState(false);

  // POSTCONDITION
  ConfirmBufferState(4, 10, 0, 10);
  ConfirmLoaderOffsets(34, 0, 0);
  ConfirmLoaderDeferredState(false);

  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, Tricky_ReadPastThreshold) {
  const size_t kSize = 5 * 1024 * 1024;
  const size_t kThreshold = 2 * 1024 * 1024;

  Initialize(kHttpUrl, 10, kSize);
  SetLoaderBuffer(10, 10);
  Start();
  PartialResponse(10, kSize - 1, kSize);

  uint8 buffer[256];
  InSequence s;

  // PRECONDITION
  WriteUntilThreshold();
  ConfirmBufferState(0, 10, 10, 10);
  ConfirmLoaderOffsets(10, 0, 0);
  ConfirmLoaderDeferredState(true);

  // *** TRICKY BUSINESS, PT. VI ***
  // Read past the forward wait threshold: cache miss.
  //
  // BEFORE
  //   offset=10 [xxxxxxxxxx] ...
  //                              ^^^^ requested 10 bytes @ threshold
  // AFTER
  //   offset=10 [xxxxxxxxxx]  !!! cache miss !!!
  //
  EXPECT_CALL(*this, ReadCallback(net::ERR_CACHE_MISS));
  ReadLoader(kThreshold + 20, 10, buffer);

  // POSTCONDITION
  ConfirmBufferState(0, 10, 10, 10);
  ConfirmLoaderOffsets(10, 0, 0);
  ConfirmLoaderDeferredState(true);

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

TEST_F(BufferedResourceLoaderTest, BufferWindow_Default) {
  Initialize(kHttpUrl, -1, -1);
  Start();

  // Test ensures that default construction of a BufferedResourceLoader has sane
  // values.
  //
  // Please do not change these values in order to make a test pass! Instead,
  // start a conversation on what the default buffer window capacities should
  // be.
  ConfirmLoaderBufferBackwardCapacity(2 * 1024 * 1024);
  ConfirmLoaderBufferForwardCapacity(2 * 1024 * 1024);

  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, BufferWindow_Bitrate_Unknown) {
  Initialize(kHttpUrl, -1, -1);
  Start();
  loader_->SetBitrate(0);
  CheckBufferWindowBounds();
  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, BufferWindow_Bitrate_BelowLowerBound) {
  Initialize(kHttpUrl, -1, -1);
  Start();
  loader_->SetBitrate(1024 * 8);  // 1 Kbps.
  CheckBufferWindowBounds();
  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, BufferWindow_Bitrate_WithinBounds) {
  Initialize(kHttpUrl, -1, -1);
  Start();
  loader_->SetBitrate(2 * 1024 * 1024 * 8);  // 2 Mbps.
  CheckBufferWindowBounds();
  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, BufferWindow_Bitrate_AboveUpperBound) {
  Initialize(kHttpUrl, -1, -1);
  Start();
  loader_->SetBitrate(100 * 1024 * 1024 * 8);  // 100 Mbps.
  CheckBufferWindowBounds();
  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, BufferWindow_PlaybackRate_Negative) {
  Initialize(kHttpUrl, -1, -1);
  Start();
  loader_->SetPlaybackRate(-10);
  CheckBufferWindowBounds();
  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, BufferWindow_PlaybackRate_Zero) {
  Initialize(kHttpUrl, -1, -1);
  Start();
  loader_->SetPlaybackRate(0);
  CheckBufferWindowBounds();
  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, BufferWindow_PlaybackRate_BelowLowerBound) {
  Initialize(kHttpUrl, -1, -1);
  Start();
  loader_->SetPlaybackRate(0.1f);
  CheckBufferWindowBounds();
  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, BufferWindow_PlaybackRate_WithinBounds) {
  Initialize(kHttpUrl, -1, -1);
  Start();
  loader_->SetPlaybackRate(10);
  CheckBufferWindowBounds();
  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, BufferWindow_PlaybackRate_AboveUpperBound) {
  Initialize(kHttpUrl, -1, -1);
  Start();
  loader_->SetPlaybackRate(100);
  CheckBufferWindowBounds();
  StopWhenLoad();
}

}  // namespace webkit_media
