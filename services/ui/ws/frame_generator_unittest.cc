// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/frame_generator.h"

#include "base/macros.h"
#include "cc/output/compositor_frame_sink.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/test/begin_frame_args_test.cc"
#include "cc/test/fake_external_begin_frame_source.h"
#include "services/ui/ws/server_window.h"
#include "services/ui/ws/server_window_delegate.h"
#include "services/ui/ws/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {
namespace ws {
namespace test {

// TestServerWindowDelegate implements ServerWindowDelegate and returns nullptrs
// when either of the methods from the interface is called.
class TestServerWindowDelegate : public ServerWindowDelegate {
 public:
  TestServerWindowDelegate() {}
  ~TestServerWindowDelegate() override {}

  // ServerWindowDelegate implementation:
  cc::mojom::DisplayCompositor* GetDisplayCompositor() override {
    return nullptr;
  }

  ServerWindow* GetRootWindow(const ServerWindow* window) override {
    return nullptr;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestServerWindowDelegate);
};

// FakeCompositorFrameSink observes a FakeExternalBeginFrameSource and receives
// CompositorFrames from a FrameGenerator.
class FakeCompositorFrameSink : public cc::CompositorFrameSink,
                                public cc::BeginFrameObserver,
                                public cc::ExternalBeginFrameSourceClient {
 public:
  FakeCompositorFrameSink()
      : cc::CompositorFrameSink(nullptr, nullptr, nullptr, nullptr) {}

  // cc::CompositorFrameSink implementation:
  bool BindToClient(cc::CompositorFrameSinkClient* client) override {
    if (!cc::CompositorFrameSink::BindToClient(client))
      return false;

    external_begin_frame_source_ =
        base::MakeUnique<cc::ExternalBeginFrameSource>(this);
    client_->SetBeginFrameSource(external_begin_frame_source_.get());
    return true;
  }

  void DetachFromClient() override {
    cc::CompositorFrameSink::DetachFromClient();
  }

  void SubmitCompositorFrame(cc::CompositorFrame frame) override {
    ++number_frames_received_;
    last_frame_ = std::move(frame);
  }

  // cc::BeginFrameObserver implementation.
  void OnBeginFrame(const cc::BeginFrameArgs& args) override {
    external_begin_frame_source_->OnBeginFrame(args);
    last_begin_frame_args_ = args;
  }

  const cc::BeginFrameArgs& LastUsedBeginFrameArgs() const override {
    return last_begin_frame_args_;
  }

  void OnBeginFrameSourcePausedChanged(bool paused) override {}

  // cc::ExternalBeginFrameSourceClient implementation:
  void OnNeedsBeginFrames(bool needs_begin_frames) override {
    needs_begin_frames_ = needs_begin_frames;
    UpdateNeedsBeginFramesInternal();
  }

  void OnDidFinishFrame(const cc::BeginFrameAck& ack) override {
    begin_frame_source_->DidFinishFrame(this, ack);
  }

  void SetBeginFrameSource(cc::BeginFrameSource* source) {
    if (begin_frame_source_ && observing_begin_frames_) {
      begin_frame_source_->RemoveObserver(this);
      observing_begin_frames_ = false;
    }
    begin_frame_source_ = source;
    UpdateNeedsBeginFramesInternal();
  }

  const cc::CompositorFrameMetadata& last_metadata() const {
    return last_frame_.metadata;
  }

  const cc::RenderPassList& last_render_pass_list() const {
    return last_frame_.render_pass_list;
  }

  int number_frames_received() { return number_frames_received_; }

 private:
  void UpdateNeedsBeginFramesInternal() {
    if (!begin_frame_source_)
      return;

    if (needs_begin_frames_ == observing_begin_frames_)
      return;

    observing_begin_frames_ = needs_begin_frames_;
    if (needs_begin_frames_) {
      begin_frame_source_->AddObserver(this);
    } else {
      begin_frame_source_->RemoveObserver(this);
    }
  }

  int number_frames_received_ = 0;
  std::unique_ptr<cc::ExternalBeginFrameSource> external_begin_frame_source_;
  cc::BeginFrameSource* begin_frame_source_ = nullptr;
  cc::BeginFrameArgs last_begin_frame_args_;
  bool observing_begin_frames_ = false;
  bool needs_begin_frames_ = false;
  cc::CompositorFrame last_frame_;

  DISALLOW_COPY_AND_ASSIGN(FakeCompositorFrameSink);
};

class FrameGeneratorTest : public testing::Test {
 public:
  FrameGeneratorTest() {}
  ~FrameGeneratorTest() override {}

  // testing::Test overrides:
  void SetUp() override {
    testing::Test::SetUp();

    std::unique_ptr<FakeCompositorFrameSink> compositor_frame_sink =
        base::MakeUnique<FakeCompositorFrameSink>();
    compositor_frame_sink_ = compositor_frame_sink.get();

    constexpr float kRefreshRate = 0.f;
    constexpr bool kTickAutomatically = false;
    begin_frame_source_ = base::MakeUnique<cc::FakeExternalBeginFrameSource>(
        kRefreshRate, kTickAutomatically);
    compositor_frame_sink_->SetBeginFrameSource(begin_frame_source_.get());
    server_window_delegate_ = base::MakeUnique<TestServerWindowDelegate>();
    root_window_ = base::MakeUnique<ServerWindow>(server_window_delegate_.get(),
                                                  WindowId());
    root_window_->SetVisible(true);
    frame_generator_ = base::MakeUnique<FrameGenerator>(
        root_window_.get(), std::move(compositor_frame_sink));
  };

  int NumberOfFramesReceived() {
    return compositor_frame_sink_->number_frames_received();
  }

  void IssueBeginFrame() {
    begin_frame_source_->TestOnBeginFrame(cc::CreateBeginFrameArgsForTesting(
        BEGINFRAME_FROM_HERE, 0, next_sequence_number_));
    ++next_sequence_number_;
  }

  FrameGenerator* frame_generator() { return frame_generator_.get(); }

  const cc::CompositorFrameMetadata& LastMetadata() const {
    return compositor_frame_sink_->last_metadata();
  }

  const cc::RenderPassList& LastRenderPassList() const {
    return compositor_frame_sink_->last_render_pass_list();
  }

  const cc::BeginFrameAck& LastBeginFrameAck() {
    return begin_frame_source_->LastAckForObserver(compositor_frame_sink_);
  }

  ServerWindow* root_window() { return root_window_.get(); }

 private:
  FakeCompositorFrameSink* compositor_frame_sink_ = nullptr;
  std::unique_ptr<cc::FakeExternalBeginFrameSource> begin_frame_source_;
  std::unique_ptr<TestServerWindowDelegate> server_window_delegate_;
  std::unique_ptr<ServerWindow> root_window_;
  std::unique_ptr<FrameGenerator> frame_generator_;
  int next_sequence_number_ = 1;

  DISALLOW_COPY_AND_ASSIGN(FrameGeneratorTest);
};

TEST_F(FrameGeneratorTest, OnSurfaceCreated) {
  EXPECT_EQ(0, NumberOfFramesReceived());

  // FrameGenerator does not request BeginFrames upon creation.
  IssueBeginFrame();
  EXPECT_EQ(0, NumberOfFramesReceived());
  EXPECT_EQ(cc::BeginFrameAck(), LastBeginFrameAck());

  const cc::SurfaceId kArbitrarySurfaceId(
      cc::FrameSinkId(1, 1),
      cc::LocalSurfaceId(1, base::UnguessableToken::Create()));
  const cc::SurfaceInfo kArbitrarySurfaceInfo(kArbitrarySurfaceId, 1.0f,
                                              gfx::Size(100, 100));
  frame_generator()->OnSurfaceCreated(kArbitrarySurfaceInfo);
  EXPECT_EQ(0, NumberOfFramesReceived());

  IssueBeginFrame();
  EXPECT_EQ(1, NumberOfFramesReceived());

  // Verify that the CompositorFrame refers to the window manager's surface via
  // referenced_surfaces.
  const cc::CompositorFrameMetadata& last_metadata = LastMetadata();
  const std::vector<cc::SurfaceId>& referenced_surfaces =
      last_metadata.referenced_surfaces;
  EXPECT_EQ(1lu, referenced_surfaces.size());
  EXPECT_EQ(kArbitrarySurfaceId, referenced_surfaces.front());

  cc::BeginFrameAck expected_ack(0, 2, 2, 0, true);
  EXPECT_EQ(expected_ack, LastBeginFrameAck());
  EXPECT_EQ(expected_ack, last_metadata.begin_frame_ack);

  // FrameGenerator stops requesting BeginFrames after submitting a
  // CompositorFrame.
  IssueBeginFrame();
  EXPECT_EQ(1, NumberOfFramesReceived());
  EXPECT_EQ(expected_ack, LastBeginFrameAck());
}

TEST_F(FrameGeneratorTest, BeginFrameWhileInvisible) {
  EXPECT_EQ(0, NumberOfFramesReceived());

  // A valid SurfaceInfo is required for BeginFrame processing.
  const cc::SurfaceId kArbitrarySurfaceId(
      cc::FrameSinkId(1, 1),
      cc::LocalSurfaceId(1, base::UnguessableToken::Create()));
  const cc::SurfaceInfo kArbitrarySurfaceInfo(kArbitrarySurfaceId, 1.0f,
                                              gfx::Size(100, 100));
  frame_generator()->OnSurfaceCreated(kArbitrarySurfaceInfo);
  EXPECT_EQ(0, NumberOfFramesReceived());

  // No frames are produced while invisible but in need of BeginFrames.
  root_window()->SetVisible(false);
  IssueBeginFrame();
  EXPECT_EQ(0, NumberOfFramesReceived());
  EXPECT_EQ(cc::BeginFrameAck(0, 1, 1, 0, false), LastBeginFrameAck());

  // When visible again, a frame is produced.
  root_window()->SetVisible(true);
  IssueBeginFrame();
  EXPECT_EQ(1, NumberOfFramesReceived());
  cc::BeginFrameAck expected_ack(0, 2, 2, 0, true);
  EXPECT_EQ(expected_ack, LastBeginFrameAck());
  EXPECT_EQ(expected_ack, LastMetadata().begin_frame_ack);
}

TEST_F(FrameGeneratorTest, SetDeviceScaleFactor) {
  EXPECT_EQ(0, NumberOfFramesReceived());
  const cc::SurfaceId kArbitrarySurfaceId(
      cc::FrameSinkId(1, 1),
      cc::LocalSurfaceId(1, base::UnguessableToken::Create()));
  const cc::SurfaceInfo kArbitrarySurfaceInfo(kArbitrarySurfaceId, 1.0f,
                                              gfx::Size(100, 100));
  constexpr float kDefaultScaleFactor = 1.0f;
  constexpr float kArbitraryScaleFactor = 0.5f;

  // A valid SurfaceInfo is required before setting device scale factor.
  frame_generator()->OnSurfaceCreated(kArbitrarySurfaceInfo);
  IssueBeginFrame();
  EXPECT_EQ(1, NumberOfFramesReceived());

  // FrameGenerator stops requesting BeginFrames after receiving one.
  IssueBeginFrame();
  EXPECT_EQ(1, NumberOfFramesReceived());

  // FrameGenerator does not request BeginFrames if its device scale factor
  // remains unchanged.
  frame_generator()->SetDeviceScaleFactor(kDefaultScaleFactor);
  IssueBeginFrame();
  EXPECT_EQ(1, NumberOfFramesReceived());
  const cc::CompositorFrameMetadata& last_metadata = LastMetadata();
  EXPECT_EQ(kDefaultScaleFactor, last_metadata.device_scale_factor);

  frame_generator()->SetDeviceScaleFactor(kArbitraryScaleFactor);
  IssueBeginFrame();
  EXPECT_EQ(2, NumberOfFramesReceived());
  const cc::CompositorFrameMetadata& second_last_metadata = LastMetadata();
  EXPECT_EQ(kArbitraryScaleFactor, second_last_metadata.device_scale_factor);
}

TEST_F(FrameGeneratorTest, SetHighContrastMode) {
  // FrameGenerator requires a valid SurfaceInfo before generating
  // CompositorFrames.
  const cc::SurfaceId kArbitrarySurfaceId(
      cc::FrameSinkId(1, 1),
      cc::LocalSurfaceId(1, base::UnguessableToken::Create()));
  const cc::SurfaceInfo kArbitrarySurfaceInfo(kArbitrarySurfaceId, 1.0f,
                                              gfx::Size(100, 100));
  frame_generator()->OnSurfaceCreated(kArbitrarySurfaceInfo);
  IssueBeginFrame();
  EXPECT_EQ(1, NumberOfFramesReceived());

  // Changing high contrast mode should trigger a BeginFrame.
  frame_generator()->SetHighContrastMode(true);
  IssueBeginFrame();
  EXPECT_EQ(2, NumberOfFramesReceived());

  // Verify that the last frame has an invert filter.
  const cc::RenderPassList& render_pass_list = LastRenderPassList();
  const cc::FilterOperations expected_filters(
      {cc::FilterOperation::CreateInvertFilter(1.f)});
  EXPECT_EQ(expected_filters, render_pass_list.front()->filters);
}

}  // namespace test
}  // namespace ws
}  // namespace ui
