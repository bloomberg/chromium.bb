// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "webkit/glue/media/web_data_source.h"
#include "webkit/glue/media/web_data_source_factory.h"
#include "webkit/mocks/mock_webframeclient.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::MockFunction;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace webkit_glue {

class MockWebDataSource : public WebDataSource {
 public:
  virtual ~MockWebDataSource() {}

  // WebDataSource methods
  MOCK_METHOD2(Initialize, void(const std::string& url,
                                const media::PipelineStatusCB& callback));
  MOCK_METHOD0(CancelInitialize, void());
  MOCK_METHOD0(HasSingleOrigin, bool());
  MOCK_METHOD0(Abort, void());

  // DataSource methods.
  MOCK_METHOD4(Read, void(int64 position, size_t size, uint8* data,
                          media::DataSource::ReadCallback* callback));
  MOCK_METHOD1(GetSize, bool(int64* size_out));
  MOCK_METHOD1(SetPreload, void(media::Preload preload));
  MOCK_METHOD0(IsStreaming, bool());
};

class WebDataSourceFactoryTest : public testing::Test {
 public:
  WebDataSourceFactoryTest()
      : message_loop_(MessageLoop::current()),
        observer_called_(false),
        view_(WebKit::WebView::create(NULL)) {
    view_->initializeMainFrame(&client_);
  }

  virtual ~WebDataSourceFactoryTest() {
    view_->close();
  }

  void OnBuildObserverCall(WebDataSource* source) {
    observer_called_ = true;
  }

  static WebDataSource* OnBuild(
      const scoped_refptr<WebDataSource>& data_source,
      MessageLoop* render_loop,
      WebKit::WebFrame* frame) {
    return data_source.get();
  }

  void CreateFactory(const scoped_refptr<WebDataSource>& data_source) {
    observer_cb_.reset(
        NewCallback(this,
                    &WebDataSourceFactoryTest::OnBuildObserverCall));
    observer_called_ = false;

    factory_.reset(new WebDataSourceFactory(
        message_loop_,
        view_->mainFrame(),
        base::Bind(&WebDataSourceFactoryTest::OnBuild,
                   data_source),
        observer_cb_.get()));
  }

  MOCK_METHOD1(BuildDoneStatus, void(media::PipelineStatus));

  void OnBuildDone(media::PipelineStatus status,
                   media::DataSource* data_source) {
    build_done_mock_.Call(status);
  }

  // Helper function that creates a BuildCB that expects to be
  // called with a particular status.
  media::DataSourceFactory::BuildCB ExpectBuildDone(
      media::PipelineStatus expected_status) {
    EXPECT_CALL(build_done_mock_, Call(expected_status));
    return base::Bind(&WebDataSourceFactoryTest::OnBuildDone,
                      base::Unretained(this));
  }

  // Helper function that creates a BuildCB that isn't expected
  // to be called.
  media::DataSourceFactory::BuildCB ExpectBuildDoneIsNotCalled() {
    return base::Bind(&WebDataSourceFactoryTest::OnBuildDone,
                      base::Unretained(this));
  }

 protected:
  MessageLoop* message_loop_;
  scoped_ptr<WebDataSourceFactory> factory_;

  scoped_ptr<WebDataSourceBuildObserverHack> observer_cb_;
  bool observer_called_;

  MockWebFrameClient client_;
  WebKit::WebView* view_;

  StrictMock<MockFunction<void(media::PipelineStatus)> > build_done_mock_;
};

TEST_F(WebDataSourceFactoryTest, NormalBuild) {

  InSequence s;

  std::string url = "http://blah.com";
  MockWebDataSource* mock_data_source = new MockWebDataSource();
  media::PipelineStatusCB init_cb;
  EXPECT_CALL(*mock_data_source, Initialize(url, _))
      .WillOnce(SaveArg<1>(&init_cb));
  CreateFactory(mock_data_source);

  factory_->Build(url, ExpectBuildDone(media::PIPELINE_OK));

  EXPECT_FALSE(init_cb.is_null());
  init_cb.Run(media::PIPELINE_OK);
}

// Test the behavior when the factory is destroyed while there is a pending
// Build() request.
TEST_F(WebDataSourceFactoryTest, DestroyedDuringBuild) {
  InSequence s;

  std::string url = "http://blah.com";
  MockWebDataSource* mock_data_source = new MockWebDataSource();
  EXPECT_CALL(*mock_data_source, Initialize(url, _));
  CreateFactory(mock_data_source);

  factory_->Build(url, ExpectBuildDoneIsNotCalled());

  EXPECT_CALL(*mock_data_source, CancelInitialize());
  factory_.reset();
}

// Test the functionality of a cloned factory after the original
// factory has been destroyed.
TEST_F(WebDataSourceFactoryTest, DestroyAfterClone) {
  InSequence s;

  std::string url = "http://blah.com";
  MockWebDataSource* mock_data_source = new MockWebDataSource();
  CreateFactory(mock_data_source);

  scoped_ptr<media::DataSourceFactory> factory2(factory_->Clone());

  factory_.reset();

  // Expect the cloned factory build to fail because the original factory
  // is gone.
  factory2->Build(url, ExpectBuildDone(media::PIPELINE_ERROR_URL_NOT_FOUND));
}

}  // namespace webkit_glue
