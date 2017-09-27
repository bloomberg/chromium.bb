// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/VideoFrameSubmitter.h"

#include <memory>
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/thread.h"
#include "cc/layers/video_frame_provider.h"
#include "cc/test/layer_test_common.h"
#include "cc/trees/task_runner_provider.h"
#include "components/viz/test/fake_external_begin_frame_source.h"
#include "media/base/video_frame.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "platform/wtf/Functional.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_sink.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;
using testing::StrictMock;

namespace blink {

namespace {

class MockVideoFrameProvider : public cc::VideoFrameProvider {
 public:
  MockVideoFrameProvider() {}
  ~MockVideoFrameProvider() {}

  MOCK_METHOD1(SetVideoFrameProviderClient, void(Client*));
  MOCK_METHOD2(UpdateCurrentFrame, bool(base::TimeTicks, base::TimeTicks));
  MOCK_METHOD0(HasCurrentFrame, bool());
  MOCK_METHOD0(GetCurrentFrame, scoped_refptr<media::VideoFrame>());
  MOCK_METHOD0(PutCurrentFrame, void());
};

class MockCompositorFrameSink : public viz::mojom::blink::CompositorFrameSink {
 public:
  MockCompositorFrameSink(
      viz::mojom::blink::CompositorFrameSinkRequest* request)
      : binding_(this, std::move(*request)) {}
  ~MockCompositorFrameSink() {}

  MOCK_METHOD1(SetNeedsBeginFrame, void(bool));

  void SubmitCompositorFrame(
      const viz::LocalSurfaceId& id,
      viz::CompositorFrame frame,
      ::viz::mojom::blink::HitTestRegionListPtr hit_test_region_list,
      uint64_t submit_time) {
    DoSubmitCompositorFrame(id, &frame);
  }
  MOCK_METHOD2(DoSubmitCompositorFrame,
               void(const viz::LocalSurfaceId&, viz::CompositorFrame*));
  MOCK_METHOD1(DidNotProduceFrame, void(const viz::BeginFrameAck&));

 private:
  mojo::Binding<viz::mojom::blink::CompositorFrameSink> binding_;
};

}  // namespace

class VideoFrameSubmitterTest : public ::testing::Test {
 public:
  VideoFrameSubmitterTest()
      : thread_("ThreadForTest"),
        now_src_(new base::SimpleTestTickClock()),
        begin_frame_source_(new viz::FakeExternalBeginFrameSource(0.f, false)),
        provider_(new StrictMock<MockVideoFrameProvider>()) {}

  void SetUp() override {
    thread_.StartAndWaitForTesting();
    thread_.task_runner()->PostTask(
        FROM_HERE, base::Bind(&VideoFrameSubmitterTest::MakeSubmitter,
                              base::Unretained(this)));
    thread_.FlushForTesting();
  }

  void TearDown() override {
    thread_.task_runner()->DeleteSoon(FROM_HERE, std::move(submitter_));
    thread_.task_runner()->DeleteSoon(FROM_HERE, std::move(sink_));
  }

  void MakeSubmitter() {
    submitter_ = std::make_unique<VideoFrameSubmitter>(provider_.get());
    viz::mojom::blink::CompositorFrameSinkPtr submitter_sink;
    viz::mojom::blink::CompositorFrameSinkRequest request =
        mojo::MakeRequest(&submitter_sink);
    sink_ = std::make_unique<StrictMock<MockCompositorFrameSink>>(&request);
    submitter_->SetSink(&submitter_sink);
  }

 protected:
  base::Thread thread_;
  std::unique_ptr<base::SimpleTestTickClock> now_src_;
  std::unique_ptr<viz::FakeExternalBeginFrameSource> begin_frame_source_;
  std::unique_ptr<StrictMock<MockCompositorFrameSink>> sink_;
  std::unique_ptr<StrictMock<MockVideoFrameProvider>> provider_;
  std::unique_ptr<VideoFrameSubmitter> submitter_;
};

TEST_F(VideoFrameSubmitterTest, StatRenderingFlipsBits) {
  EXPECT_FALSE(submitter_->Rendering());
  EXPECT_CALL(*sink_, SetNeedsBeginFrame(true));

  thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&VideoFrameSubmitter::StartRendering,
                            base::Unretained(submitter_.get())));
  thread_.FlushForTesting();

  EXPECT_TRUE(submitter_->Rendering());
}

TEST_F(VideoFrameSubmitterTest, StopUsingProviderNullsProvider) {
  EXPECT_FALSE(submitter_->Rendering());
  EXPECT_EQ(provider_.get(), submitter_->Provider());

  submitter_->StopUsingProvider();

  EXPECT_EQ(nullptr, submitter_->Provider());
}

TEST_F(VideoFrameSubmitterTest,
       StopUsingProviderSubmitsFrameAndStopsRendering) {
  EXPECT_CALL(*sink_, SetNeedsBeginFrame(true));

  thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&VideoFrameSubmitter::StartRendering,
                            base::Unretained(submitter_.get())));
  thread_.FlushForTesting();

  EXPECT_TRUE(submitter_->Rendering());

  EXPECT_CALL(*provider_, GetCurrentFrame());
  EXPECT_CALL(*sink_, DoSubmitCompositorFrame(_, _));
  EXPECT_CALL(*provider_, PutCurrentFrame());
  EXPECT_CALL(*sink_, SetNeedsBeginFrame(false));

  thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&VideoFrameSubmitter::StopUsingProvider,
                            base::Unretained(submitter_.get())));
  thread_.FlushForTesting();

  EXPECT_FALSE(submitter_->Rendering());
}

TEST_F(VideoFrameSubmitterTest, DidReceiveFrameDoesNothingIfRendering) {
  EXPECT_CALL(*sink_, SetNeedsBeginFrame(true));

  thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&VideoFrameSubmitter::StartRendering,
                            base::Unretained(submitter_.get())));
  thread_.FlushForTesting();

  EXPECT_TRUE(submitter_->Rendering());

  thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&VideoFrameSubmitter::DidReceiveFrame,
                            base::Unretained(submitter_.get())));
  thread_.FlushForTesting();
}

TEST_F(VideoFrameSubmitterTest, DidReceiveFrameSubmitsFrame) {
  EXPECT_FALSE(submitter_->Rendering());

  EXPECT_CALL(*provider_, GetCurrentFrame());
  EXPECT_CALL(*sink_, DoSubmitCompositorFrame(_, _));
  EXPECT_CALL(*provider_, PutCurrentFrame());

  thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&VideoFrameSubmitter::DidReceiveFrame,
                            base::Unretained(submitter_.get())));

  thread_.FlushForTesting();
}

TEST_F(VideoFrameSubmitterTest, SubmitFrameWithoutProviderReturns) {
  submitter_->StopUsingProvider();

  viz::BeginFrameAck begin_frame_ack =
      viz::BeginFrameAck::CreateManualAckWithDamage();
  thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&VideoFrameSubmitter::SubmitFrame,
                 base::Unretained(submitter_.get()), begin_frame_ack));

  thread_.FlushForTesting();
}

TEST_F(VideoFrameSubmitterTest, OnBeginFrameSubmitsFrame) {
  EXPECT_CALL(*sink_, SetNeedsBeginFrame(true));

  thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&VideoFrameSubmitter::StartRendering,
                            base::Unretained(submitter_.get())));
  thread_.FlushForTesting();

  EXPECT_CALL(*provider_, UpdateCurrentFrame(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*provider_, GetCurrentFrame());
  EXPECT_CALL(*sink_, DoSubmitCompositorFrame(_, _));
  EXPECT_CALL(*provider_, PutCurrentFrame());

  viz::BeginFrameArgs args = begin_frame_source_->CreateBeginFrameArgs(
      BEGINFRAME_FROM_HERE, now_src_.get());
  thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&VideoFrameSubmitter::OnBeginFrame,
                            base::Unretained(submitter_.get()), args));
  thread_.FlushForTesting();
}

TEST_F(VideoFrameSubmitterTest, MissedFrameArgDoesNotProduceFrame) {
  EXPECT_CALL(*sink_, DidNotProduceFrame(_));

  viz::BeginFrameArgs args = begin_frame_source_->CreateBeginFrameArgs(
      BEGINFRAME_FROM_HERE, now_src_.get());
  args.type = viz::BeginFrameArgs::MISSED;
  thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&VideoFrameSubmitter::OnBeginFrame,
                            base::Unretained(submitter_.get()), args));
  thread_.FlushForTesting();
}

TEST_F(VideoFrameSubmitterTest, MissingProviderDoesNotProduceFrame) {
  submitter_->StopUsingProvider();

  EXPECT_CALL(*sink_, DidNotProduceFrame(_));

  viz::BeginFrameArgs args = begin_frame_source_->CreateBeginFrameArgs(
      BEGINFRAME_FROM_HERE, now_src_.get());
  thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&VideoFrameSubmitter::OnBeginFrame,
                            base::Unretained(submitter_.get()), args));
  thread_.FlushForTesting();
}

TEST_F(VideoFrameSubmitterTest, NoUpdateOnFrameDoesNotProduceFrame) {
  EXPECT_CALL(*provider_, UpdateCurrentFrame(_, _)).WillOnce(Return(false));
  EXPECT_CALL(*sink_, DidNotProduceFrame(_));

  viz::BeginFrameArgs args = begin_frame_source_->CreateBeginFrameArgs(
      BEGINFRAME_FROM_HERE, now_src_.get());
  thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&VideoFrameSubmitter::OnBeginFrame,
                            base::Unretained(submitter_.get()), args));
  thread_.FlushForTesting();
}

TEST_F(VideoFrameSubmitterTest, NotRenderingDoesNotProduceFrame) {
  EXPECT_CALL(*provider_, UpdateCurrentFrame(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*sink_, DidNotProduceFrame(_));

  viz::BeginFrameArgs args = begin_frame_source_->CreateBeginFrameArgs(
      BEGINFRAME_FROM_HERE, now_src_.get());
  thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&VideoFrameSubmitter::OnBeginFrame,
                            base::Unretained(submitter_.get()), args));
  thread_.FlushForTesting();
}

}  // namespace blink
