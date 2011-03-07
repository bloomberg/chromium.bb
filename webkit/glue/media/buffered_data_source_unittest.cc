// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/test/test_timeouts.h"
#include "media/base/mock_callback.h"
#include "media/base/mock_filter_host.h"
#include "media/base/mock_filters.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLError.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLResponse.h"
#include "webkit/glue/media/buffered_data_source.h"
#include "webkit/mocks/mock_webframe.h"

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

namespace webkit_glue {

static const char* kHttpUrl = "http://test";
static const char* kFileUrl = "file://test";
static const int kDataSize = 1024;

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
      : BufferedDataSource(message_loop, frame) {
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
  MockBufferedResourceLoader() : BufferedResourceLoader(GURL(), 0, 0) {
  }

  MOCK_METHOD3(Start, void(net::CompletionCallback* read_callback,
                           NetworkEventCallback* network_callback,
                           WebFrame* frame));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD4(Read, void(int64 position, int read_size, uint8* buffer,
                          net::CompletionCallback* callback));
  MOCK_METHOD0(content_length, int64());
  MOCK_METHOD0(instance_size, int64());
  MOCK_METHOD0(partial_response, bool());
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
  BufferedDataSourceTest() {
    message_loop_ = MessageLoop::current();

    // Prepare test data.
    for (size_t i = 0; i < sizeof(data_); ++i) {
      data_[i] = i;
    }
  }

  virtual ~BufferedDataSourceTest() {
  }

  void ExpectCreateAndStartResourceLoader(int start_error) {
    EXPECT_CALL(*data_source_, CreateResourceLoader(_, _))
        .WillOnce(Return(loader_.get()));

    EXPECT_CALL(*loader_, Start(NotNull(), NotNull(), NotNull()))
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

    frame_.reset(new NiceMock<MockWebFrame>());

    data_source_ = new MockBufferedDataSource(MessageLoop::current(),
                                              frame_.get());
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
    ON_CALL(*loader_, partial_response())
        .WillByDefault(Return(partial_response));
    ON_CALL(*loader_, url())
        .WillByDefault(ReturnRef(gurl_));
    if (initialized_ok) {
      // Expected loaded or not.
      EXPECT_CALL(host_, SetLoaded(loaded));

      // TODO(hclam): The condition for streaming needs to be adjusted.
      if (instance_size != -1 && (loaded || partial_response)) {
        EXPECT_CALL(host_, SetTotalBytes(instance_size));
        if (loaded)
          EXPECT_CALL(host_, SetBufferedBytes(instance_size));
        else
          EXPECT_CALL(host_, SetBufferedBytes(0));
      } else {
        EXPECT_CALL(host_, SetStreaming(true));
      }
    } else {
      EXPECT_CALL(host_, SetError(media::PIPELINE_ERROR_NETWORK));
      EXPECT_CALL(*loader_, Stop());
    }

    // Actual initialization of the data source.
    data_source_->Initialize(url, media::NewExpectedCallback());
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

    data_source_->Stop(media::NewExpectedCallback());
    message_loop_->RunAllPending();
  }

  void InvokeStartCallback(
      net::CompletionCallback* callback,
      BufferedResourceLoader::NetworkEventCallback* network_callback,
      WebFrame* frame) {
    callback->RunWithParams(Tuple1<int>(error_));
    delete callback;
    // TODO(hclam): Save this callback.
    delete network_callback;
  }

  void InvokeReadCallback(int64 position, int size, uint8* buffer,
                          net::CompletionCallback* callback) {
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
        NewCallback(this, &BufferedDataSourceTest::ReadCallback));
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
        NewCallback(this, &BufferedDataSourceTest::ReadCallback));
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

  void ReadDataSourceMiss(int64 position, int size) {
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
    EXPECT_CALL(*new_loader, Start(NotNull(), NotNull(), NotNull()))
        .WillOnce(DoAll(Assign(&error_, net::OK),
                        Invoke(this,
                               &BufferedDataSourceTest::InvokeStartCallback)));
    EXPECT_CALL(*new_loader, partial_response())
        .WillRepeatedly(Return(loader_->partial_response()));

    // 4. Then again a read request is made to the new loader.
    EXPECT_CALL(*new_loader, Read(position, size, NotNull(), NotNull()))
        .WillOnce(DoAll(Assign(&error_, size),
                        Invoke(this,
                               &BufferedDataSourceTest::InvokeReadCallback)));

    EXPECT_CALL(*this, ReadCallback(size));

    data_source_->Read(
        position, size, buffer_,
        NewCallback(this, &BufferedDataSourceTest::ReadCallback));
    message_loop_->RunAllPending();

    // Make sure data is correct.
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
        NewCallback(this, &BufferedDataSourceTest::ReadCallback));

    message_loop_->RunAllPending();
  }

  void ReadDataSourceTimesOut(int64 position, int size) {
    // 1. Drop the request and let it times out.
    {
      InSequence s;
      EXPECT_CALL(*loader_, Read(position, size, NotNull(), NotNull()))
          .WillOnce(DeleteArg<3>());
      EXPECT_CALL(*loader_, Stop());
    }

    // 2. Then the current loader will be stop and destroyed.
    NiceMock<MockBufferedResourceLoader> *new_loader =
        new NiceMock<MockBufferedResourceLoader>();
    EXPECT_CALL(*data_source_, CreateResourceLoader(position, -1))
        .WillOnce(Return(new_loader));

    // 3. Then the new loader will be started and respond to queries about
    //    whether this is a partial response using the value of the previous
    //    loader.
    EXPECT_CALL(*new_loader, Start(NotNull(), NotNull(), NotNull()))
        .WillOnce(DoAll(Assign(&error_, net::OK),
                        Invoke(this,
                               &BufferedDataSourceTest::InvokeStartCallback)));
    EXPECT_CALL(*new_loader, partial_response())
        .WillRepeatedly(Return(loader_->partial_response()));

    // 4. Then again a read request is made to the new loader.
    EXPECT_CALL(*new_loader, Read(position, size, NotNull(), NotNull()))
        .WillOnce(DoAll(Assign(&error_, size),
                        Invoke(this,
                               &BufferedDataSourceTest::InvokeReadCallback),
                        InvokeWithoutArgs(message_loop_,
                                          &MessageLoop::Quit)));

    EXPECT_CALL(*this, ReadCallback(size));

    data_source_->Read(
        position, size, buffer_,
        NewCallback(this, &BufferedDataSourceTest::ReadCallback));

    // This blocks the current thread until the watch task is executed and
    // triggers a read callback to quit this message loop.
    message_loop_->Run();

    // Make sure data is correct.
    EXPECT_EQ(0, memcmp(buffer_, data_ + static_cast<int>(position), size));

    loader_ = new_loader;
  }

  MOCK_METHOD1(ReadCallback, void(size_t size));

  scoped_refptr<NiceMock<MockBufferedResourceLoader> > loader_;
  scoped_refptr<MockBufferedDataSource> data_source_;
  scoped_ptr<NiceMock<MockWebFrame> > frame_;

  StrictMock<media::MockFilterHost> host_;
  GURL gurl_;
  MessageLoop* message_loop_;

  int error_;
  uint8 buffer_[1024];
  uint8 data_[1024];

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
  ReadDataSourceMiss(1000, 10);
  ReadDataSourceMiss(20, 10);
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

TEST_F(BufferedDataSourceTest, ReadTimesOut) {
  InitializeDataSource(kHttpUrl, net::OK, true, 1024, LOADING);
  ReadDataSourceTimesOut(20, 10);
  StopDataSource();
}

TEST_F(BufferedDataSourceTest, FileHasLoadedState) {
  InitializeDataSource(kFileUrl, net::OK, true, 1024, LOADED);
  ReadDataSourceTimesOut(20, 10);
  StopDataSource();
}

// This test makes sure that Stop() does not require a task to run on
// |message_loop_| before it calls its callback. This prevents accidental
// introduction of a pipeline teardown deadlock. The pipeline owner blocks
// the render message loop while waiting for Stop() to complete. Since this
// object runs on the render message loop, Stop() will not complete if it
// requires a task to run on the the message loop that is being blocked.
TEST_F(BufferedDataSourceTest, StopDoesNotUseMessageLoopForCallback) {
  InitializeDataSource(kFileUrl, net::OK, true, 1024, LOADED);

  // Create a callback that lets us verify that it was called before
  // Stop() returns. This is to make sure that the callback does not
  // require |message_loop_| to execute tasks before being called.
  media::MockCallback* stop_callback = media::NewExpectedCallback();
  bool stop_done_called = false;
  ON_CALL(*stop_callback, RunWithParams(_))
      .WillByDefault(Assign(&stop_done_called, true));

  // Stop() the data source like normal.
  data_source_->Stop(stop_callback);

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
      NewCallback(static_cast<BufferedDataSourceTest*>(this),
                  &BufferedDataSourceTest::ReadCallback));

  // Call Abort() with the read pending.
  EXPECT_CALL(*this, ReadCallback(-1));
  EXPECT_CALL(*loader_, Stop());
  data_source_->Abort();

  // Verify that Read()'s after the Abort() issue callback with an error.
  EXPECT_CALL(*this, ReadCallback(-1));
  data_source_->Read(
      0, 10, buffer_,
      NewCallback(static_cast<BufferedDataSourceTest*>(this),
                  &BufferedDataSourceTest::ReadCallback));

  // Stop() the data source like normal.
  data_source_->Stop(media::NewExpectedCallback());

  // Allow cleanup task to run.
  message_loop_->RunAllPending();

  // Verify that Read() was not called on the loader.
  EXPECT_FALSE(read_called);
}

}  // namespace webkit_glue
