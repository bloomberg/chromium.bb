// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/string_util.h"
#include "media/base/filters.h"
#include "media/base/mock_filter_host.h"
#include "media/base/mock_filters.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "webkit/glue/media/buffered_data_source.h"
#include "webkit/glue/media/mock_media_resource_loader_bridge_factory.h"
#include "webkit/glue/mock_resource_loader_bridge.h"

using ::testing::_;
using ::testing::Assign;
using ::testing::DeleteArg;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::StrictMock;
using ::testing::WithArgs;

namespace {

const char* kHttpUrl = "http://test";
const int kDataSize = 1024;

}  // namespace

namespace webkit_glue {

class BufferedResourceLoaderTest : public testing::Test {
 public:
  BufferedResourceLoaderTest() {
    bridge_.reset(new StrictMock<MockResourceLoaderBridge>());

    for (int i = 0; i < kDataSize; ++i)
      data_[i] = i;
  }

  ~BufferedResourceLoaderTest() {
    if (bridge_.get())
      EXPECT_CALL(*bridge_, OnDestroy());
    EXPECT_CALL(bridge_factory_, OnDestroy());
  }

  void Initialize(const char* url, int first_position, int last_position) {
    gurl_ = GURL(url);
    first_position_ = first_position;
    last_position_ = last_position;

    loader_.reset(new BufferedResourceLoader(&bridge_factory_, gurl_,
                                             first_position_, last_position_));
    EXPECT_EQ(gurl_.spec(), loader_->GetURLForDebugging());
  }

  void Start() {
    InSequence s;
    EXPECT_CALL(bridge_factory_,
                CreateBridge(gurl_, _, first_position_, last_position_))
        .WillOnce(Return(bridge_.get()));
    EXPECT_CALL(*bridge_, Start(loader_.get()));
    loader_->Start(NewCallback(this,
                               &BufferedResourceLoaderTest::StartCallback));
  }

  void FullResponse(int64 instance_size) {
    EXPECT_CALL(*this, StartCallback(net::OK));
    ResourceLoaderBridge::ResponseInfo info;
    std::string header = StringPrintf("HTTP/1.1 200 OK\n"
                                      "Content-Length: %lld", instance_size);
    replace(header.begin(), header.end(), '\n', '\0');
    info.headers = new net::HttpResponseHeaders(header);
    info.content_length = instance_size;
    loader_->OnReceivedResponse(info, false);
    EXPECT_EQ(instance_size, loader_->content_length());
    EXPECT_EQ(instance_size, loader_->instance_size());
  }

  void PartialResponse(int64 instance_size) {
    EXPECT_CALL(*this, StartCallback(net::OK));
    int64 content_length = last_position_ - first_position_ + 1;
    ResourceLoaderBridge::ResponseInfo info;
    std::string header = StringPrintf("HTTP/1.1 206 Partial Content\n"
                                      "Content-Range: bytes %lld-%lld/%lld",
                                      first_position_,
                                      last_position_,
                                      instance_size);
    replace(header.begin(), header.end(), '\n', '\0');
    info.headers = new net::HttpResponseHeaders(header);
    info.content_length = content_length;
    loader_->OnReceivedResponse(info, false);
    EXPECT_EQ(content_length, loader_->content_length());
    EXPECT_EQ(instance_size, loader_->instance_size());
  }

  void StopWhenLoad() {
    InSequence s;
    EXPECT_CALL(*bridge_, Cancel());
    EXPECT_CALL(*bridge_, OnDestroy())
      .WillOnce(Invoke(this, &BufferedResourceLoaderTest::ReleaseBridge));
    loader_->Stop();
  }

  void ReleaseBridge() {
    bridge_.release();
  }

  // Helper method to write to |loader_| from |data_|.
  void WriteLoader(int position, int size) {
    loader_->OnReceivedData(reinterpret_cast<char*>(data_ + position), size);
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

  MOCK_METHOD1(StartCallback, void(int error));
  MOCK_METHOD1(ReadCallback, void(int error));

 protected:
  GURL gurl_;
  int64 first_position_;
  int64 last_position_;

  scoped_ptr<BufferedResourceLoader> loader_;
  StrictMock<MockMediaResourceLoaderBridgeFactory> bridge_factory_;
  scoped_ptr<StrictMock<MockResourceLoaderBridge> > bridge_;

  uint8 data_[kDataSize];

 private:
  DISALLOW_COPY_AND_ASSIGN(BufferedResourceLoaderTest);
};

TEST_F(BufferedResourceLoaderTest, StartStop) {
  Initialize(kHttpUrl, -1, -1);
  Start();
  StopWhenLoad();
}

// Tests that HTTP header is missing in the response.
TEST_F(BufferedResourceLoaderTest, MissingHttpHeader) {
  Initialize(kHttpUrl, -1, -1);
  Start();

  EXPECT_CALL(*this, StartCallback(net::ERR_INVALID_RESPONSE));
  EXPECT_CALL(*bridge_, Cancel());
  EXPECT_CALL(*bridge_, OnDestroy())
      .WillOnce(Invoke(this, &BufferedResourceLoaderTest::ReleaseBridge));

  ResourceLoaderBridge::ResponseInfo info;
  loader_->OnReceivedResponse(info, false);
}

// Tests that a bad HTTP response is recived, e.g. file not found.
TEST_F(BufferedResourceLoaderTest, BadHttpResponse) {
  Initialize(kHttpUrl, -1, -1);
  Start();

  EXPECT_CALL(*this, StartCallback(net::ERR_FAILED));
  EXPECT_CALL(*bridge_, Cancel());
  EXPECT_CALL(*bridge_, OnDestroy())
      .WillOnce(Invoke(this, &BufferedResourceLoaderTest::ReleaseBridge));

  ResourceLoaderBridge::ResponseInfo info;
  info.headers = new net::HttpResponseHeaders("HTTP/1.1 404 Bot Found\n");
  loader_->OnReceivedResponse(info, false);
}

// Tests that partial content is requested but not fulfilled.
TEST_F(BufferedResourceLoaderTest, NotPartialRange) {
  Initialize(kHttpUrl, 100, -1);
  Start();

  EXPECT_CALL(*this, StartCallback(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE));
  EXPECT_CALL(*bridge_, Cancel());
  EXPECT_CALL(*bridge_, OnDestroy())
      .WillOnce(Invoke(this, &BufferedResourceLoaderTest::ReleaseBridge));

  ResourceLoaderBridge::ResponseInfo info;
  info.headers = new net::HttpResponseHeaders("HTTP/1.1 200 OK\n");
  loader_->OnReceivedResponse(info, false);
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
  PartialResponse(1024);
  StopWhenLoad();
}

// Tests the logic of sliding window for data buffering and reading.
TEST_F(BufferedResourceLoaderTest, BufferAndRead) {
  Initialize(kHttpUrl, 10, 29);
  Start();
  PartialResponse(30);

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
  EXPECT_CALL(*bridge_, OnDestroy())
      .WillOnce(Invoke(this, &BufferedResourceLoaderTest::ReleaseBridge));
  URLRequestStatus status;
  status.set_status(URLRequestStatus::SUCCESS);
  loader_->OnCompletedRequest(status, "");

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
  PartialResponse(0x01000000);

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

  EXPECT_CALL(*bridge_, OnDestroy())
      .WillOnce(Invoke(this, &BufferedResourceLoaderTest::ReleaseBridge));
  EXPECT_CALL(*this, ReadCallback(5));
  URLRequestStatus status;
  status.set_status(URLRequestStatus::SUCCESS);
  loader_->OnCompletedRequest(status, "");
}

TEST_F(BufferedResourceLoaderTest, RequestFailedWhenRead) {
  Initialize(kHttpUrl, 10, 29);
  Start();
  PartialResponse(30);

  uint8 buffer[10];
  InSequence s;

  ReadLoader(10, 10, buffer);
  EXPECT_CALL(*bridge_, OnDestroy())
      .WillOnce(Invoke(this, &BufferedResourceLoaderTest::ReleaseBridge));
  EXPECT_CALL(*this, ReadCallback(net::ERR_FAILED));
  URLRequestStatus status;
  status.set_status(URLRequestStatus::FAILED);
  loader_->OnCompletedRequest(status, "");
}

// TODO(hclam): add unit test for defer loading.

class MockBufferedResourceLoader : public BufferedResourceLoader {
 public:
  MockBufferedResourceLoader() : BufferedResourceLoader() {
  }

  ~MockBufferedResourceLoader() {
    OnDestroy();
  }

  MOCK_METHOD1(Start, void(net::CompletionCallback* read_callback));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD4(Read, void(int64 position, int read_size, uint8* buffer,
                          net::CompletionCallback* callback));
  MOCK_METHOD0(content_length, int64());
  MOCK_METHOD0(instance_size, int64());
  MOCK_METHOD0(OnDestroy, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBufferedResourceLoader);
};

// A mock BufferedDataSource to inject mock BufferedResourceLoader through
// CreateLoader() method.
class MockBufferedDataSource : public BufferedDataSource {
 public:
  // Static methods for creating this class.
  static media::FilterFactory* CreateFactory(
      MessageLoop* message_loop,
      MediaResourceLoaderBridgeFactory* bridge_factory) {
    return new media::FilterFactoryImpl2<
        MockBufferedDataSource,
        MessageLoop*,
        MediaResourceLoaderBridgeFactory*>(message_loop,
                                           bridge_factory);
  }

  virtual base::TimeDelta GetTimeoutMilliseconds() {
    // It is 100 ms because we don't want the test to run too long.
    return base::TimeDelta::FromMilliseconds(100);
  }

  MOCK_METHOD2(CreateLoader, BufferedResourceLoader*(int64 first_position,
                                                     int64 last_position));

 protected:
  MockBufferedDataSource(
      MessageLoop* message_loop,
      MediaResourceLoaderBridgeFactory* factory)
      : BufferedDataSource(message_loop, factory) {
  }

 private:
  friend class media::FilterFactoryImpl2<
      MockBufferedDataSource,
      MessageLoop*,
      MediaResourceLoaderBridgeFactory*>;

  DISALLOW_COPY_AND_ASSIGN(MockBufferedDataSource);
};

class BufferedDataSourceTest : public testing::Test {
 public:
  BufferedDataSourceTest() {
    message_loop_.reset(MessageLoop::current());
    bridge_factory_.reset(
        new StrictMock<MockMediaResourceLoaderBridgeFactory>());
    ReleaseLoader();
    factory_ = MockBufferedDataSource::CreateFactory(message_loop_.get(),
                                                     bridge_factory_.get());

    // Prepare test data.
    for (size_t i = 0; i < sizeof(data_); ++i) {
      data_[i] = i;
    }
  }

  ~BufferedDataSourceTest() {
    if (data_source_) {
      // Release the bridge factory because we don't own it.
      // Expects bridge factory to be destroyed along with data source.
      EXPECT_CALL(*bridge_factory_, OnDestroy())
          .WillOnce(Invoke(this,
                           &BufferedDataSourceTest::ReleaseBridgeFactory));
    }

    // We don't own the message loop so release it.
    message_loop_.release();
  }

  void InitializeDataSource(const char* url, int error, int probe_error,
                            int64 instance_size) {
    // Saves the url first.
    gurl_ = GURL(url);

    media::MediaFormat url_format;
    url_format.SetAsString(media::MediaFormat::kMimeType,
                           media::mime_type::kURL);
    url_format.SetAsString(media::MediaFormat::kURL, url);
    data_source_ = factory_->Create<MockBufferedDataSource>(url_format);
    CHECK(data_source_);

    // There is no need to provide a message loop to data source.
    data_source_->set_host(&host_);

    // Creates the first mock loader to be injected.
    loader_.reset(new StrictMock<MockBufferedResourceLoader>());
    probe_loader_.reset(new StrictMock<MockBufferedResourceLoader>());

    InSequence s;
    StrictMock<media::MockFilterCallback> callback;

    // There is one resource loader with full range will be created.
    EXPECT_CALL(*data_source_, CreateLoader(-1, -1))
        .WillOnce(Return(loader_.get()));

    // Then another resource loader with a small partial range is created.
    EXPECT_CALL(*data_source_, CreateLoader(1, 1))
        .WillOnce(Return(probe_loader_.get()));

    // The initial response loader will be started.
    EXPECT_CALL(*loader_, Start(NotNull()))
        .WillOnce(DoAll(Assign(&error_, error),
                        Invoke(this,
                               &BufferedDataSourceTest::InvokeStartCallback)));
    if (error == net::OK) {
      EXPECT_CALL(*loader_, instance_size())
          .WillOnce(Return(instance_size));
      if (instance_size != -1) {
        EXPECT_CALL(host_, SetTotalBytes(instance_size));
        EXPECT_CALL(host_, SetBufferedBytes(instance_size));
      } else {
        EXPECT_CALL(host_, SetStreaming(true));
      }

      // Then the probe resource loader will start.
      EXPECT_CALL(*probe_loader_, Start(NotNull()))
          .WillOnce(DoAll(Assign(&error_, probe_error),
                    Invoke(this,
                           &BufferedDataSourceTest::InvokeStartCallback)));
      if (probe_error != net::OK)
        EXPECT_CALL(host_, SetStreaming(true));
      EXPECT_CALL(*probe_loader_, Stop());
      EXPECT_CALL(callback, OnFilterCallback());
      EXPECT_CALL(callback, OnCallbackDestroyed());
    } else {
      EXPECT_CALL(host_, SetError(media::PIPELINE_ERROR_NETWORK));
      EXPECT_CALL(*loader_, Stop());
      EXPECT_CALL(*probe_loader_, Stop());
      EXPECT_CALL(callback, OnFilterCallback());
      EXPECT_CALL(callback, OnCallbackDestroyed());

      // This expectation looks a little strange, but this is actually what
      // will happen since we are not running a message loop. So simply
      // delete the callback.
      EXPECT_CALL(*probe_loader_, Start(NotNull()))
          .WillOnce(DeleteArg<0>());
    }

    data_source_->Initialize(url, callback.NewCallback());
    message_loop_->RunAllPending();

    if (error == net::OK) {
      // Verify the size of the data source.
      int64 size;
      if (instance_size != -1) {
        EXPECT_TRUE(data_source_->GetSize(&size));
        EXPECT_EQ(instance_size, size);

        if (probe_error == net::OK) {
          EXPECT_FALSE(data_source_->IsStreaming());
        }
      } else {
        EXPECT_FALSE(data_source_->GetSize(&size));
        EXPECT_EQ(0, size);
        EXPECT_TRUE(data_source_->IsStreaming());
      }

      // Verify the data source is streamed if the probe has received an error.
      if (probe_error != net::OK) {
        EXPECT_TRUE(data_source_->IsStreaming());
      }
    }
  }

  void StopDataSource() {
    if (loader_.get()) {
      InSequence s;
      EXPECT_CALL(*loader_, Stop());
      EXPECT_CALL(*probe_loader_, Stop());
    }

    EXPECT_CALL(*loader_, OnDestroy())
       .WillOnce(Invoke(this, &BufferedDataSourceTest::ReleaseLoader));
    EXPECT_CALL(*probe_loader_, OnDestroy())
       .WillOnce(Invoke(this, &BufferedDataSourceTest::ReleaseProbeLoader));

    data_source_->Stop();
    message_loop_->RunAllPending();
  }

  void ReleaseBridgeFactory() {
    bridge_factory_.release();
  }

  void ReleaseLoader() {
    loader_.release();
  }

  void ReleaseProbeLoader() {
    probe_loader_.release();
  }

  void InvokeStartCallback(net::CompletionCallback* callback) {
    callback->RunWithParams(Tuple1<int>(error_));
    delete callback;
  }

  void InvokeReadCallback(int64 position, int size, uint8* buffer,
                          net::CompletionCallback* callback) {
    if (error_ > 0)
      memcpy(buffer, data_ + static_cast<int>(position), error_);
    callback->RunWithParams(Tuple1<int>(error_));
    delete callback;
  }

  void ReadDataSourceHit(int64 position, int size, int read_size) {
    EXPECT_TRUE(loader_.get() != NULL);

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

  void ReadDataSourceMiss(int64 position, int size) {
    EXPECT_TRUE(loader_.get() != NULL);

    InSequence s;
    // 1. Reply with a cache miss for the read.
    EXPECT_CALL(*loader_, Read(position, size, NotNull(), NotNull()))
        .WillOnce(DoAll(Assign(&error_, net::ERR_CACHE_MISS),
                        Invoke(this,
                               &BufferedDataSourceTest::InvokeReadCallback)));

    // 2. Then the current loader will be stop and destroyed.
    StrictMock<MockBufferedResourceLoader> *new_loader =
        new StrictMock<MockBufferedResourceLoader>();
    EXPECT_CALL(*loader_, Stop());
    EXPECT_CALL(*data_source_, CreateLoader(position, -1))
        .WillOnce(Return(new_loader));
    EXPECT_CALL(*loader_, OnDestroy())
        .WillOnce(Invoke(this, &BufferedDataSourceTest::ReleaseLoader));

    // 3. Then the new loader will be started.
    EXPECT_CALL(*new_loader, Start(NotNull()))
        .WillOnce(DoAll(Assign(&error_, net::OK),
                        Invoke(this,
                               &BufferedDataSourceTest::InvokeStartCallback)));

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

    EXPECT_TRUE(loader_.get() == NULL);
    loader_.reset(new_loader);
  }

  void ReadDataSourceFailed(int64 position, int size, int error) {
    EXPECT_TRUE(loader_.get() != NULL);

    InSequence s;
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
    InSequence s;
    // 1. Drop the request and let it times out.
    EXPECT_CALL(*loader_, Read(position, size, NotNull(), NotNull()))
        .WillOnce(DeleteArg<3>());

    // 2. Then the current loader will be stop and destroyed.
    StrictMock<MockBufferedResourceLoader> *new_loader =
        new StrictMock<MockBufferedResourceLoader>();
    EXPECT_CALL(*loader_, Stop());
    EXPECT_CALL(*data_source_, CreateLoader(position, -1))
        .WillOnce(Return(new_loader));
    EXPECT_CALL(*loader_, OnDestroy())
        .WillOnce(Invoke(this, &BufferedDataSourceTest::ReleaseLoader));

    // 3. Then the new loader will be started.
    EXPECT_CALL(*new_loader, Start(NotNull()))
        .WillOnce(DoAll(Assign(&error_, net::OK),
                        Invoke(this,
                               &BufferedDataSourceTest::InvokeStartCallback)));

    // 4. Then again a read request is made to the new loader.
    EXPECT_CALL(*new_loader, Read(position, size, NotNull(), NotNull()))
        .WillOnce(DoAll(Assign(&error_, size),
                        Invoke(this,
                               &BufferedDataSourceTest::InvokeReadCallback),
                        InvokeWithoutArgs(message_loop_.get(),
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

    EXPECT_TRUE(loader_.get() == NULL);
    loader_.reset(new_loader);
  }

  MOCK_METHOD1(ReadCallback, void(size_t size));

  scoped_ptr<StrictMock<MockMediaResourceLoaderBridgeFactory> >
      bridge_factory_;
  scoped_ptr<StrictMock<MockBufferedResourceLoader> > loader_;
  scoped_ptr<StrictMock<MockBufferedResourceLoader> > probe_loader_;
  scoped_refptr<MockBufferedDataSource > data_source_;
  scoped_refptr<media::FilterFactory> factory_;

  StrictMock<media::MockFilterHost> host_;
  GURL gurl_;
  scoped_ptr<MessageLoop> message_loop_;

  int error_;
  uint8 buffer_[1024];
  uint8 data_[1024];

 private:
  DISALLOW_COPY_AND_ASSIGN(BufferedDataSourceTest);
};

TEST_F(BufferedDataSourceTest, InitializationSuccess) {
  InitializeDataSource(kHttpUrl, net::OK, net::OK, 1024);
  StopDataSource();
}

TEST_F(BufferedDataSourceTest, InitiailizationFailed) {
  InitializeDataSource(kHttpUrl, net::ERR_FILE_NOT_FOUND,
                       net::ERR_FILE_NOT_FOUND, 0);
  StopDataSource();
}

TEST_F(BufferedDataSourceTest, MissingContentLength) {
  InitializeDataSource(kHttpUrl, net::OK, net::OK, -1);
  StopDataSource();
}

TEST_F(BufferedDataSourceTest, RangeRequestNotSupported) {
  InitializeDataSource(kHttpUrl, net::OK,
                       net::ERR_REQUEST_RANGE_NOT_SATISFIABLE, 1024);
  StopDataSource();
}

TEST_F(BufferedDataSourceTest,
       MissingContentLengthAndRangeRequestNotSupported) {
  InitializeDataSource(kHttpUrl, net::OK,
                       net::ERR_REQUEST_RANGE_NOT_SATISFIABLE, -1);
  StopDataSource();
}

TEST_F(BufferedDataSourceTest, ReadCacheHit) {
  InitializeDataSource(kHttpUrl, net::OK, net::OK, 25);

  // Performs read with cache hit.
  ReadDataSourceHit(10, 10, 10);

  // Performs read with cache hit but partially filled.
  ReadDataSourceHit(20, 10, 5);

  StopDataSource();
}

TEST_F(BufferedDataSourceTest, ReadCacheMiss) {
  InitializeDataSource(kHttpUrl, net::OK, net::OK, 1024);
  ReadDataSourceMiss(1000, 10);
  ReadDataSourceMiss(20, 10);
  StopDataSource();
}

TEST_F(BufferedDataSourceTest, ReadFailed) {
  InitializeDataSource(kHttpUrl, net::OK, net::OK, 1024);
  ReadDataSourceHit(10, 10, 10);
  ReadDataSourceFailed(10, 10, net::ERR_CONNECTION_RESET);
  StopDataSource();
}

TEST_F(BufferedDataSourceTest, ReadTimesOut) {
  InitializeDataSource(kHttpUrl, net::OK, net::OK, 1024);
  ReadDataSourceTimesOut(20, 10);
  StopDataSource();
}

}  // namespace webkit_glue
