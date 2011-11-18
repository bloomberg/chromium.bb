// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/bind.h"
#include "base/test/test_timeouts.h"
#include "media/base/media_log.h"
#include "media/base/mock_callback.h"
#include "media/base/mock_filter_host.h"
#include "media/base/mock_filters.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLResponse.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "webkit/media/buffered_data_source.h"
#include "webkit/mocks/mock_webframeclient.h"
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

using WebKit::WebFrame;
using WebKit::WebString;
using WebKit::WebURLError;
using WebKit::WebURLResponse;
using WebKit::WebView;

using webkit_glue::MockWebFrameClient;
using webkit_glue::MockWebURLLoader;

namespace webkit_media {

static const char* kHttpUrl = "http://test";
static const char* kFileUrl = "file://test";
static const int kDataSize = 1024;
static const int kMaxCacheMissesBeforeFailTest = 20;

enum NetworkState {
  NONE,
  LOADED,
  LOADING
};

// A mock BufferedDataSource to inject mock BufferedResourceLoader through
// CreateResourceLoader() method.
class MockBufferedDataSource : public BufferedDataSource {
 public:
  MockBufferedDataSource(MessageLoop* message_loop, WebFrame* frame)
      : BufferedDataSource(message_loop, frame, new media::MediaLog()) {
  }

  virtual base::TimeDelta GetTimeoutMilliseconds() {
    return base::TimeDelta::FromMilliseconds(
                            TestTimeouts::tiny_timeout_ms());
  }

  MOCK_METHOD2(CreateResourceLoader,
               BufferedResourceLoader*(int64 first_position,
                                       int64 last_position));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBufferedDataSource);
};

class MockBufferedResourceLoader : public BufferedResourceLoader {
 public:
  MockBufferedResourceLoader()
      : BufferedResourceLoader(GURL(), 0, 0, kThresholdDefer,
                               0, 0, new media::MediaLog()) {
  }

  MOCK_METHOD3(Start, void(net::OldCompletionCallback* read_callback,
                           const base::Closure& network_callback,
                           WebFrame* frame));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD4(Read, void(int64 position, int read_size, uint8* buffer,
                          net::OldCompletionCallback* callback));
  MOCK_METHOD0(content_length, int64());
  MOCK_METHOD0(instance_size, int64());
  MOCK_METHOD0(range_supported, bool());
  MOCK_METHOD0(network_activity, bool());
  MOCK_METHOD0(url, const GURL&());
  MOCK_METHOD0(GetBufferedFirstBytePosition, int64());
  MOCK_METHOD0(GetBufferedLastBytePosition, int64());

 protected:
  ~MockBufferedResourceLoader() {}

  DISALLOW_COPY_AND_ASSIGN(MockBufferedResourceLoader);
};

class BufferedDataSourceTest : public testing::Test {
 public:
  BufferedDataSourceTest()
      : view_(WebView::create(NULL)) {
    view_->initializeMainFrame(&client_);
    message_loop_ = MessageLoop::current();

    for (size_t i = 0; i < sizeof(data_); ++i) {
      data_[i] = i;
    }
  }

  virtual ~BufferedDataSourceTest() {
    view_->close();
  }

  void ExpectCreateAndStartResourceLoader(int start_error) {
    EXPECT_CALL(*data_source_, CreateResourceLoader(_, _))
        .WillOnce(Return(loader_.get()));

    EXPECT_CALL(*loader_, Start(NotNull(), _, NotNull()))
        .WillOnce(
            DoAll(Assign(&error_, start_error),
                  Invoke(this,
                         &BufferedDataSourceTest::InvokeStartCallback)));
  }

  void InitializeDataSource(const char* url, int error,
                            bool partial_response, int64 instance_size,
                            NetworkState networkState) {
    // Saves the url first.
    gurl_ = GURL(url);

    data_source_ = new MockBufferedDataSource(MessageLoop::current(),
                                              view_->mainFrame());
    data_source_->set_host(&host_);

    scoped_refptr<NiceMock<MockBufferedResourceLoader> > first_loader(
        new NiceMock<MockBufferedResourceLoader>());

    // Creates the mock loader to be injected.
    loader_ = first_loader;

    bool initialized_ok = (error == net::OK);
    bool loaded = networkState == LOADED;
    {
      InSequence s;
      ExpectCreateAndStartResourceLoader(error);

      // In the case of an invalid partial response we expect a second loader
      // to be created.
      if (partial_response && (error == net::ERR_INVALID_RESPONSE)) {
        // Verify that the initial loader is stopped.
        EXPECT_CALL(*loader_, url())
            .WillRepeatedly(ReturnRef(gurl_));
        EXPECT_CALL(*loader_, Stop());

        // Replace loader_ with a new instance.
        loader_ = new NiceMock<MockBufferedResourceLoader>();

        // Create and start. Make sure Start() is called on the new loader.
        ExpectCreateAndStartResourceLoader(net::OK);

        // Update initialization variable since we know the second loader will
        // return OK.
        initialized_ok = true;
      }
    }

    // Attach a static function that deletes the memory referred by the
    // "callback" parameter.
    ON_CALL(*loader_, Read(_, _, _ , _))
        .WillByDefault(DeleteArg<3>());

    ON_CALL(*loader_, instance_size())
        .WillByDefault(Return(instance_size));

    // range_supported() return true if we expect to get a partial response.
    ON_CALL(*loader_, range_supported())
        .WillByDefault(Return(partial_response));

    ON_CALL(*loader_, url())
        .WillByDefault(ReturnRef(gurl_));
    media::PipelineStatus expected_init_status = media::PIPELINE_OK;
    if (initialized_ok) {
      // Expected loaded or not.
      EXPECT_CALL(host_, SetLoaded(loaded));

      if (instance_size != -1)
        EXPECT_CALL(host_, SetTotalBytes(instance_size));

      if (loaded)
        EXPECT_CALL(host_, SetBufferedBytes(instance_size));
      else
        EXPECT_CALL(host_, SetBufferedBytes(0));

      if (!partial_response || instance_size == -1)
        EXPECT_CALL(host_, SetStreaming(true));

    } else {
      expected_init_status = media::PIPELINE_ERROR_NETWORK;
      EXPECT_CALL(*loader_, Stop());
    }

    // Actual initialization of the data source.
    data_source_->Initialize(url,
        media::NewExpectedStatusCB(expected_init_status));
    message_loop_->RunAllPending();

    if (initialized_ok) {
      // Verify the size of the data source.
      int64 size;
      if (instance_size != -1 && (loaded || partial_response)) {
        EXPECT_TRUE(data_source_->GetSize(&size));
        EXPECT_EQ(instance_size, size);
      } else {
        EXPECT_TRUE(data_source_->IsStreaming());
      }
    }
  }

  void StopDataSource() {
    if (loader_) {
      InSequence s;
      EXPECT_CALL(*loader_, Stop());
    }

    data_source_->Stop(media::NewExpectedClosure());
    message_loop_->RunAllPending();
  }

  void InvokeStartCallback(
      net::OldCompletionCallback* callback,
      const base::Closure& network_callback,
      WebFrame* frame) {
    callback->RunWithParams(Tuple1<int>(error_));
    delete callback;
    // TODO(hclam): Save network_callback.
  }

  void InvokeReadCallback(int64 position, int size, uint8* buffer,
                          net::OldCompletionCallback* callback) {
    if (error_ > 0)
      memcpy(buffer, data_ + static_cast<int>(position), error_);
    callback->RunWithParams(Tuple1<int>(error_));
    delete callback;
  }

  void ReadDataSourceHit(int64 position, int size, int read_size) {
    EXPECT_TRUE(loader_);

    InSequence s;
    // Expect the read is delegated to the resource loader.
    EXPECT_CALL(*loader_, Read(position, size, NotNull(), NotNull()))
        .WillOnce(DoAll(Assign(&error_, read_size),
                        Invoke(this,
                               &BufferedDataSourceTest::InvokeReadCallback)));

    // The read has succeeded, so read callback will be called.
    EXPECT_CALL(*this, ReadCallback(read_size));

    data_source_->Read(
        position, size, buffer_,
        base::Bind(&BufferedDataSourceTest::ReadCallback,
                   base::Unretained(this)));
    message_loop_->RunAllPending();

    // Make sure data is correct.
    EXPECT_EQ(0,
              memcmp(buffer_, data_ + static_cast<int>(position), read_size));
  }

  void ReadDataSourceHang(int64 position, int size) {
    EXPECT_TRUE(loader_);

    // Expect a call to read, but the call never returns.
    EXPECT_CALL(*loader_, Read(position, size, NotNull(), NotNull()));
    data_source_->Read(
        position, size, buffer_,
        base::Bind(&BufferedDataSourceTest::ReadCallback,
                   base::Unretained(this)));
    message_loop_->RunAllPending();

    // Now expect the read to return after aborting the data source.
    EXPECT_CALL(*this, ReadCallback(_));
    EXPECT_CALL(*loader_, Stop());
    data_source_->Abort();
    message_loop_->RunAllPending();

    // The loader has now been stopped. Set this to null so that when the
    // DataSource is stopped, it does not expect a call to stop the loader.
    loader_ = NULL;
  }

  void ReadDataSourceMiss(int64 position, int size, int start_error) {
    EXPECT_TRUE(loader_);

    // 1. Reply with a cache miss for the read.
    {
      InSequence s;
      EXPECT_CALL(*loader_, Read(position, size, NotNull(), NotNull()))
          .WillOnce(DoAll(Assign(&error_, net::ERR_CACHE_MISS),
                          Invoke(this,
                                 &BufferedDataSourceTest::InvokeReadCallback)));
      EXPECT_CALL(*loader_, Stop());
    }

    // 2. Then the current loader will be stop and destroyed.
    NiceMock<MockBufferedResourceLoader> *new_loader =
        new NiceMock<MockBufferedResourceLoader>();
    EXPECT_CALL(*data_source_, CreateResourceLoader(position, -1))
        .WillOnce(Return(new_loader));

    // 3. Then the new loader will be started.
    EXPECT_CALL(*new_loader, Start(NotNull(), _, NotNull()))
        .WillOnce(DoAll(Assign(&error_, start_error),
                        Invoke(this,
                               &BufferedDataSourceTest::InvokeStartCallback)));

    if (start_error == net::OK) {
      EXPECT_CALL(*new_loader, range_supported())
          .WillRepeatedly(Return(loader_->range_supported()));

      // 4a. Then again a read request is made to the new loader.
      EXPECT_CALL(*new_loader, Read(position, size, NotNull(), NotNull()))
          .WillOnce(DoAll(Assign(&error_, size),
                          Invoke(this,
                                 &BufferedDataSourceTest::InvokeReadCallback)));

      EXPECT_CALL(*this, ReadCallback(size));
    } else {
      // 4b. The read callback is called with an error because Start() on the
      // new loader returned an error.
      EXPECT_CALL(*this, ReadCallback(media::DataSource::kReadError));
    }

    data_source_->Read(
        position, size, buffer_,
        base::Bind(&BufferedDataSourceTest::ReadCallback,
                   base::Unretained(this)));
    message_loop_->RunAllPending();

    // Make sure data is correct.
    if (start_error == net::OK)
      EXPECT_EQ(0, memcmp(buffer_, data_ + static_cast<int>(position), size));

    loader_ = new_loader;
  }

  void ReadDataSourceFailed(int64 position, int size, int error) {
    EXPECT_TRUE(loader_);

    // 1. Expect the read is delegated to the resource loader.
    EXPECT_CALL(*loader_, Read(position, size, NotNull(), NotNull()))
        .WillOnce(DoAll(Assign(&error_, error),
                        Invoke(this,
                               &BufferedDataSourceTest::InvokeReadCallback)));

    // 2. Host will then receive an error.
    EXPECT_CALL(*loader_, Stop());

    // 3. The read has failed, so read callback will be called.
    EXPECT_CALL(*this, ReadCallback(media::DataSource::kReadError));

    data_source_->Read(
        position, size, buffer_,
        base::Bind(&BufferedDataSourceTest::ReadCallback,
                   base::Unretained(this)));

    message_loop_->RunAllPending();
  }

  BufferedResourceLoader* InvokeCacheMissCreateResourceLoader(int64 start,
                                                              int64 end) {
    NiceMock<MockBufferedResourceLoader>* new_loader =
        new NiceMock<MockBufferedResourceLoader>();

    EXPECT_CALL(*new_loader, Start(NotNull(), _, NotNull()))
        .WillOnce(DoAll(Assign(&error_, net::OK),
                        Invoke(this,
                               &BufferedDataSourceTest::InvokeStartCallback)));

    EXPECT_CALL(*new_loader, range_supported())
        .WillRepeatedly(Return(loader_->range_supported()));

    int error = net::ERR_FAILED;
    if (cache_miss_count_ < kMaxCacheMissesBeforeFailTest) {
      cache_miss_count_++;
      error = net::ERR_CACHE_MISS;
    }

    EXPECT_CALL(*new_loader, Read(start, _, NotNull(), NotNull()))
        .WillOnce(DoAll(Assign(&error_, error),
                        Invoke(this,
                               &BufferedDataSourceTest::InvokeReadCallback)));

    loader_ = new_loader;
    return new_loader;
  }

  void ReadDataSourceAlwaysCacheMiss(int64 position, int size) {
    cache_miss_count_ = 0;

    EXPECT_CALL(*data_source_, CreateResourceLoader(position, -1))
        .WillRepeatedly(Invoke(
            this,
            &BufferedDataSourceTest::InvokeCacheMissCreateResourceLoader));

    EXPECT_CALL(*loader_, Read(position, size, NotNull(), NotNull()))
        .WillOnce(DoAll(Assign(&error_, net::ERR_CACHE_MISS),
                        Invoke(this,
                               &BufferedDataSourceTest::InvokeReadCallback)));

    EXPECT_CALL(*this, ReadCallback(media::DataSource::kReadError));

    data_source_->Read(
        position, size, buffer_,
        base::Bind(&BufferedDataSourceTest::ReadCallback,
                   base::Unretained(this)));

    message_loop_->RunAllPending();

    EXPECT_LT(cache_miss_count_, kMaxCacheMissesBeforeFailTest);
  }

  MOCK_METHOD1(ReadCallback, void(size_t size));

  scoped_refptr<NiceMock<MockBufferedResourceLoader> > loader_;
  scoped_refptr<MockBufferedDataSource> data_source_;

  MockWebFrameClient client_;
  WebView* view_;

  StrictMock<media::MockFilterHost> host_;
  GURL gurl_;
  MessageLoop* message_loop_;

  int error_;
  uint8 buffer_[1024];
  uint8 data_[1024];

  int cache_miss_count_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BufferedDataSourceTest);
};

TEST_F(BufferedDataSourceTest, InitializationSuccess) {
  InitializeDataSource(kHttpUrl, net::OK, true, 1024, LOADING);
  StopDataSource();
}

TEST_F(BufferedDataSourceTest, InitiailizationFailed) {
  InitializeDataSource(kHttpUrl, net::ERR_FILE_NOT_FOUND, false, 0, NONE);
  StopDataSource();
}

TEST_F(BufferedDataSourceTest, MissingContentLength) {
  InitializeDataSource(kHttpUrl, net::OK, true, -1, LOADING);
  StopDataSource();
}

TEST_F(BufferedDataSourceTest, RangeRequestNotSupported) {
  InitializeDataSource(kHttpUrl, net::OK, false, 1024, LOADING);
  StopDataSource();
}

// Test the case where we get a 206 response, but no Content-Range header.
TEST_F(BufferedDataSourceTest, MissingContentRange) {
  InitializeDataSource(kHttpUrl, net::ERR_INVALID_RESPONSE, true, 1024,
                       LOADING);
  StopDataSource();
}

TEST_F(BufferedDataSourceTest,
       MissingContentLengthAndRangeRequestNotSupported) {
  InitializeDataSource(kHttpUrl, net::OK, false, -1, LOADING);
  StopDataSource();
}

TEST_F(BufferedDataSourceTest, ReadCacheHit) {
  InitializeDataSource(kHttpUrl, net::OK, true, 25, LOADING);

  // Performs read with cache hit.
  ReadDataSourceHit(10, 10, 10);

  // Performs read with cache hit but partially filled.
  ReadDataSourceHit(20, 10, 5);

  StopDataSource();
}

TEST_F(BufferedDataSourceTest, ReadCacheMiss) {
  InitializeDataSource(kHttpUrl, net::OK, true, 1024, LOADING);
  ReadDataSourceMiss(1000, 10, net::OK);
  ReadDataSourceMiss(20, 10, net::OK);
  StopDataSource();
}

// Test the case where the initial response from the server indicates that
// Range requests are supported, but a later request prove otherwise.
TEST_F(BufferedDataSourceTest, ServerLiesAboutRangeSupport) {
  InitializeDataSource(kHttpUrl, net::OK, true, 1024, LOADING);
  ReadDataSourceHit(10, 10, 10);
  ReadDataSourceMiss(1000, 10, net::ERR_INVALID_RESPONSE);
  StopDataSource();
}

TEST_F(BufferedDataSourceTest, ReadHang) {
  InitializeDataSource(kHttpUrl, net::OK, true, 25, LOADING);
  ReadDataSourceHang(10, 10);
  StopDataSource();
}

TEST_F(BufferedDataSourceTest, ReadFailed) {
  InitializeDataSource(kHttpUrl, net::OK, true, 1024, LOADING);
  ReadDataSourceHit(10, 10, 10);
  ReadDataSourceFailed(10, 10, net::ERR_CONNECTION_RESET);
  StopDataSource();
}

// Helper that sets |*value| to true.  Useful for binding into a Closure.
static void SetTrue(bool* value) {
  *value = true;
}

// This test makes sure that Stop() does not require a task to run on
// |message_loop_| before it calls its callback. This prevents accidental
// introduction of a pipeline teardown deadlock. The pipeline owner blocks
// the render message loop while waiting for Stop() to complete. Since this
// object runs on the render message loop, Stop() will not complete if it
// requires a task to run on the the message loop that is being blocked.
TEST_F(BufferedDataSourceTest, StopDoesNotUseMessageLoopForCallback) {
  InitializeDataSource(kFileUrl, net::OK, true, 1024, LOADED);

  // Stop() the data source, using a callback that lets us verify that it was
  // called before Stop() returns. This is to make sure that the callback does
  // not require |message_loop_| to execute tasks before being called.
  bool stop_done_called = false;
  data_source_->Stop(base::Bind(&SetTrue, &stop_done_called));

  // Verify that the callback was called inside the Stop() call.
  EXPECT_TRUE(stop_done_called);

  message_loop_->RunAllPending();
}

TEST_F(BufferedDataSourceTest, AbortDuringPendingRead) {
  InitializeDataSource(kFileUrl, net::OK, true, 1024, LOADED);

  // Setup a way to verify that Read() is not called on the loader.
  // We are doing this to make sure that the ReadTask() is still on
  // the message loop queue when Abort() is called.
  bool read_called = false;
  ON_CALL(*loader_, Read(_, _, _ , _))
      .WillByDefault(DoAll(Assign(&read_called, true),
                           DeleteArg<3>()));

  // Initiate a Read() on the data source, but don't allow the
  // message loop to run.
  data_source_->Read(
      0, 10, buffer_,
      base::Bind(&BufferedDataSourceTest::ReadCallback,
                 base::Unretained(static_cast<BufferedDataSourceTest*>(this))));

  // Call Abort() with the read pending.
  EXPECT_CALL(*this, ReadCallback(-1));
  EXPECT_CALL(*loader_, Stop());
  data_source_->Abort();

  // Verify that Read()'s after the Abort() issue callback with an error.
  EXPECT_CALL(*this, ReadCallback(-1));
  data_source_->Read(
      0, 10, buffer_,
      base::Bind(&BufferedDataSourceTest::ReadCallback,
                 base::Unretained(static_cast<BufferedDataSourceTest*>(this))));

  // Stop() the data source like normal.
  data_source_->Stop(media::NewExpectedClosure());

  // Allow cleanup task to run.
  message_loop_->RunAllPending();

  // Verify that Read() was not called on the loader.
  EXPECT_FALSE(read_called);
}

// Test that we only allow a limited number of cache misses for a
// single Read() request.
TEST_F(BufferedDataSourceTest, BoundedCacheMisses) {
  InitializeDataSource(kHttpUrl, net::OK, true, 1024, LOADING);

  ReadDataSourceAlwaysCacheMiss(0, 10);

  StopDataSource();
}

// TODO(scherkus): de-dupe from buffered_resource_loader_unittest.cc
ACTION_P(RequestCanceled, loader) {
  WebURLError error;
  error.reason = net::ERR_ABORTED;
  error.domain = WebString::fromUTF8(net::kErrorDomain);
  loader->didFail(NULL, error);
}

// A more realistic BufferedDataSource that uses BufferedResourceLoader instead
// of a mocked version but injects a MockWebURLLoader.
//
// TODO(scherkus): re-write these tests to use this class then drop the "2"
// suffix.
class MockBufferedDataSource2 : public BufferedDataSource {
 public:
  MockBufferedDataSource2(MessageLoop* message_loop, WebFrame* frame)
      : BufferedDataSource(message_loop, frame, new media::MediaLog()),
        url_loader_(NULL) {
  }

  virtual base::TimeDelta GetTimeoutMilliseconds() {
    return base::TimeDelta::FromMilliseconds(
                            TestTimeouts::tiny_timeout_ms());
  }

  virtual BufferedResourceLoader* CreateResourceLoader(int64 first_position,
                                                       int64 last_position) {
    loader_ = BufferedDataSource::CreateResourceLoader(first_position,
                                                       last_position);

    url_loader_ = new NiceMock<MockWebURLLoader>();
    ON_CALL(*url_loader_, cancel())
        .WillByDefault(RequestCanceled(loader_));

    loader_->SetURLLoaderForTest(url_loader_);
    return loader_;
  }

  const scoped_refptr<BufferedResourceLoader>& loader() { return loader_; }
  NiceMock<MockWebURLLoader>* url_loader() { return url_loader_; }

 private:
  scoped_refptr<BufferedResourceLoader> loader_;
  NiceMock<MockWebURLLoader>* url_loader_;

  DISALLOW_COPY_AND_ASSIGN(MockBufferedDataSource2);
};

class BufferedDataSourceTest2 : public testing::Test {
 public:
  BufferedDataSourceTest2()
      : view_(WebView::create(NULL)),
        message_loop_(MessageLoop::current()) {
    view_->initializeMainFrame(&client_);
  }

  virtual ~BufferedDataSourceTest2() {
    view_->close();
  }

  void InitializeDataSource(const char* url) {
    gurl_ = GURL(url);

    data_source_ = new MockBufferedDataSource2(message_loop_,
                                               view_->mainFrame());
    data_source_->set_host(&host_);
    data_source_->Initialize(url,
                             media::NewExpectedStatusCB(media::PIPELINE_OK));
    message_loop_->RunAllPending();

    // Simulate 206 response for a 5,000,000 byte length file.
    WebURLResponse response(gurl_);
    response.setHTTPHeaderField(WebString::fromUTF8("Accept-Ranges"),
                                WebString::fromUTF8("bytes"));
    response.setHTTPHeaderField(WebString::fromUTF8("Content-Range"),
                                WebString::fromUTF8("bytes 0-4999999/5000000"));
    response.setHTTPHeaderField(WebString::fromUTF8("Content-Length"),
                                WebString::fromUTF8("5000000"));
    response.setExpectedContentLength(5000000);
    response.setHTTPStatusCode(206);

    // We should receive corresponding information about the media resource.
    EXPECT_CALL(host_, SetLoaded(false));
    EXPECT_CALL(host_, SetTotalBytes(5000000));
    EXPECT_CALL(host_, SetBufferedBytes(0));

    data_source_->loader()->didReceiveResponse(data_source_->url_loader(),
                                               response);

    message_loop_->RunAllPending();
  }

  void StopDataSource() {
    data_source_->Stop(media::NewExpectedClosure());
    message_loop_->RunAllPending();
  }

  MOCK_METHOD1(ReadCallback, void(size_t size));
  media::DataSource::ReadCallback NewReadCallback(size_t size) {
    EXPECT_CALL(*this, ReadCallback(size));
    return base::Bind(&BufferedDataSourceTest2::ReadCallback,
                      base::Unretained(this));
  }

  // Accessors for private variables on |data_source_|.
  media::Preload preload() { return data_source_->preload_; }
  BufferedResourceLoader::DeferStrategy defer_strategy() {
    return data_source_->loader()->defer_strategy_;
  }
  int data_source_bitrate() { return data_source_->bitrate_; }
  int data_source_playback_rate() { return data_source_->playback_rate_; }
  int loader_bitrate() { return data_source_->loader()->bitrate_; }
  int loader_playback_rate() { return data_source_->loader()->playback_rate_; }

  scoped_refptr<MockBufferedDataSource2> data_source_;

  GURL gurl_;
  MockWebFrameClient client_;
  WebView* view_;

  StrictMock<media::MockFilterHost> host_;
  MessageLoop* message_loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BufferedDataSourceTest2);
};

TEST_F(BufferedDataSourceTest2, Default) {
  InitializeDataSource("http://localhost/foo.webm");

  // Ensure we have sane values for default loading scenario.
  EXPECT_EQ(media::AUTO, preload());
  EXPECT_EQ(BufferedResourceLoader::kThresholdDefer, defer_strategy());

  EXPECT_EQ(0, data_source_bitrate());
  EXPECT_EQ(0.0f, data_source_playback_rate());
  EXPECT_EQ(0, loader_bitrate());
  EXPECT_EQ(0.0f, loader_playback_rate());

  StopDataSource();
}

TEST_F(BufferedDataSourceTest2, SetBitrate) {
  InitializeDataSource("http://localhost/foo.webm");

  data_source_->SetBitrate(1234);
  message_loop_->RunAllPending();
  EXPECT_EQ(1234, data_source_bitrate());
  EXPECT_EQ(1234, loader_bitrate());

  // Read so far ahead to cause the loader to get recreated.
  BufferedResourceLoader* old_loader = data_source_->loader();

  uint8 buffer[1024];
  data_source_->Read(4000000, 1024, buffer,
                     NewReadCallback(media::DataSource::kReadError));
  message_loop_->RunAllPending();

  // Verify loader changed but still has same bitrate.
  EXPECT_NE(old_loader, data_source_->loader().get());
  EXPECT_EQ(1234, loader_bitrate());

  StopDataSource();
}

TEST_F(BufferedDataSourceTest2, SetPlaybackRate) {
  InitializeDataSource("http://localhost/foo.webm");

  data_source_->SetPlaybackRate(2.0f);
  message_loop_->RunAllPending();
  EXPECT_EQ(2.0f, data_source_playback_rate());
  EXPECT_EQ(2.0f, loader_playback_rate());

  // Read so far ahead to cause the loader to get recreated.
  BufferedResourceLoader* old_loader = data_source_->loader();

  uint8 buffer[1024];
  data_source_->Read(4000000, 1024, buffer,
                     NewReadCallback(media::DataSource::kReadError));
  message_loop_->RunAllPending();

  // Verify loader changed but still has same bitrate.
  EXPECT_NE(old_loader, data_source_->loader().get());
  EXPECT_EQ(2.0f, loader_playback_rate());

  StopDataSource();
}

}  // namespace webkit_media
