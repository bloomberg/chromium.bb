// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "media/base/filters.h"
#include "media/base/mock_filter_host.h"
#include "media/base/mock_filters.h"
#include "webkit/glue/media/mock_media_resource_loader_bridge_factory.h"
#include "webkit/glue/media/simple_data_source.h"
#include "webkit/glue/mock_resource_loader_bridge.h"

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

namespace {

const int kDataSize = 1024;
const char kHttpUrl[] = "http://test";
const char kHttpsUrl[] = "https://test";
const char kFileUrl[] = "file://test";
const char kDataUrl[] =
    "data:text/plain;base64,YWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eXoK";
const char kDataUrlDecoded[] = "abcdefghijklmnopqrstuvwxyz";
const char kInvalidUrl[] = "whatever://test";

}  // namespace

namespace webkit_glue {

class SimpleDataSourceTest : public testing::Test {
 public:
  SimpleDataSourceTest() {
    bridge_factory_.reset(
        new NiceMock<MockMediaResourceLoaderBridgeFactory>());
    bridge_.reset(new NiceMock<MockResourceLoaderBridge>());

    for (int i = 0; i < kDataSize; ++i) {
      data_[i] = i;
    }
  }

  virtual ~SimpleDataSourceTest() {
    if (bridge_.get())
      EXPECT_CALL(*bridge_, OnDestroy());
    if (bridge_factory_.get())
      EXPECT_CALL(*bridge_factory_, OnDestroy());
  }

  void InitializeDataSource(const char* url) {
    data_source_ = new SimpleDataSource(MessageLoop::current(),
                                        bridge_factory_.get());
    CHECK(data_source_);

    // There is no need to provide a message loop to data source.
    data_source_->set_host(&host_);

    // First a bridge is created.
    InSequence s;
    EXPECT_CALL(*bridge_factory_, CreateBridge(GURL(url), _, -1, -1))
        .WillOnce(Return(bridge_.get()));
    EXPECT_CALL(*bridge_, Start(data_source_.get()))
        .WillOnce(Return(true));

    data_source_->Initialize(url, callback_.NewCallback());

    MessageLoop::current()->RunAllPending();
  }

  void RequestSucceeded(bool is_loaded) {
    ResourceResponseInfo info;
    info.content_length = kDataSize;

    data_source_->OnReceivedResponse(info, false);
    int64 size;
    EXPECT_TRUE(data_source_->GetSize(&size));
    EXPECT_EQ(kDataSize, size);

    for (int i = 0; i < kDataSize; ++i)
      data_source_->OnReceivedData(data_ + i, 1);

    EXPECT_CALL(host_, SetLoaded(is_loaded));

    InSequence s;
    EXPECT_CALL(*bridge_, OnDestroy())
        .WillOnce(Invoke(this, &SimpleDataSourceTest::ReleaseBridge));
    EXPECT_CALL(host_, SetTotalBytes(kDataSize));
    EXPECT_CALL(host_, SetBufferedBytes(kDataSize));
    EXPECT_CALL(callback_, OnFilterCallback());
    EXPECT_CALL(callback_, OnCallbackDestroyed());

    URLRequestStatus status;
    status.set_status(URLRequestStatus::SUCCESS);
    status.set_os_error(0);
    data_source_->OnCompletedRequest(status, "", base::Time());

    // Let the tasks to be executed.
    MessageLoop::current()->RunAllPending();
  }

  void RequestFailed() {
    InSequence s;
    EXPECT_CALL(*bridge_, OnDestroy())
        .WillOnce(Invoke(this, &SimpleDataSourceTest::ReleaseBridge));
    EXPECT_CALL(host_, SetError(media::PIPELINE_ERROR_NETWORK));
    EXPECT_CALL(callback_, OnFilterCallback());
    EXPECT_CALL(callback_, OnCallbackDestroyed());

    URLRequestStatus status;
    status.set_status(URLRequestStatus::FAILED);
    status.set_os_error(100);
    data_source_->OnCompletedRequest(status, "", base::Time());

    // Let the tasks to be executed.
    MessageLoop::current()->RunAllPending();
  }

  void DestroyDataSource() {
    EXPECT_CALL(*bridge_factory_, OnDestroy())
        .WillOnce(Invoke(this, &SimpleDataSourceTest::ReleaseBridgeFactory));

    StrictMock<media::MockFilterCallback> callback;
    EXPECT_CALL(callback, OnFilterCallback());
    EXPECT_CALL(callback, OnCallbackDestroyed());
    data_source_->Stop(callback.NewCallback());
    MessageLoop::current()->RunAllPending();

    data_source_ = NULL;
  }

  void AsyncRead() {
    for (int i = 0; i < kDataSize; ++i) {
      uint8 buffer[1];

      EXPECT_CALL(*this, ReadCallback(1));
      data_source_->Read(
          i, 1, buffer,
          NewCallback(this, &SimpleDataSourceTest::ReadCallback));
      EXPECT_EQ(static_cast<uint8>(data_[i]), buffer[0]);
    }
  }

  void ReleaseBridge() {
    ignore_result(bridge_.release());
  }

  void ReleaseBridgeFactory() {
    ignore_result(bridge_factory_.release());
  }

  MOCK_METHOD1(ReadCallback, void(size_t size));

 protected:
  scoped_ptr<MessageLoop> message_loop_;
  scoped_ptr<NiceMock<MockMediaResourceLoaderBridgeFactory> > bridge_factory_;
  scoped_ptr<NiceMock<MockResourceLoaderBridge> > bridge_;
  scoped_refptr<SimpleDataSource> data_source_;
  StrictMock<media::MockFilterHost> host_;
  StrictMock<media::MockFilterCallback> callback_;
  char data_[kDataSize];

  DISALLOW_COPY_AND_ASSIGN(SimpleDataSourceTest);
};

TEST_F(SimpleDataSourceTest, InitializeHTTP) {
  InitializeDataSource(kHttpUrl);
  RequestSucceeded(false);
  DestroyDataSource();
}

TEST_F(SimpleDataSourceTest, InitializeHTTPS) {
  InitializeDataSource(kHttpsUrl);
  RequestSucceeded(false);
  DestroyDataSource();
}

TEST_F(SimpleDataSourceTest, InitializeFile) {
  InitializeDataSource(kFileUrl);
  RequestSucceeded(true);
  DestroyDataSource();
}

TEST_F(SimpleDataSourceTest, InitializeData) {
  data_source_ = new SimpleDataSource(MessageLoop::current(),
                                      bridge_factory_.get());
  EXPECT_TRUE(data_source_->IsUrlSupported(kDataUrl));
  CHECK(data_source_);

  // There is no need to provide a message loop to data source.
  data_source_->set_host(&host_);

  EXPECT_CALL(host_, SetLoaded(true));
  EXPECT_CALL(host_, SetTotalBytes(sizeof(kDataUrlDecoded)));
  EXPECT_CALL(host_, SetBufferedBytes(sizeof(kDataUrlDecoded)));
  EXPECT_CALL(callback_, OnFilterCallback());
  EXPECT_CALL(callback_, OnCallbackDestroyed());

  data_source_->Initialize(kDataUrl, callback_.NewCallback());
  MessageLoop::current()->RunAllPending();

  DestroyDataSource();
}

TEST_F(SimpleDataSourceTest, RequestFailed) {
  InitializeDataSource(kHttpUrl);
  RequestFailed();
  DestroyDataSource();
}

TEST_F(SimpleDataSourceTest, StopWhenDownloading) {
  InitializeDataSource(kHttpUrl);

  EXPECT_CALL(*bridge_, Cancel());
  EXPECT_CALL(*bridge_, OnDestroy())
      .WillOnce(Invoke(this, &SimpleDataSourceTest::ReleaseBridge));
  EXPECT_CALL(callback_, OnCallbackDestroyed());
  DestroyDataSource();
}

TEST_F(SimpleDataSourceTest, AsyncRead) {
  InitializeDataSource(kFileUrl);
  RequestSucceeded(true);
  AsyncRead();
  DestroyDataSource();
}

}  // namespace webkit_glue
