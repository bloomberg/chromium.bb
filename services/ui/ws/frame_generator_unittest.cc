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
    frame_generator_ =
        base::MakeUnique<FrameGenerator>(std::move(compositor_frame_sink));
    frame_generator_->OnWindowSizeChanged(gfx::Size(1, 2));
  };

  void InitWithSurfaceInfo() {
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
  }

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

 private:
  FakeCompositorFrameSink* compositor_frame_sink_ = nullptr;
  std::unique_ptr<cc::FakeExternalBeginFrameSource> begin_frame_source_;
  std::unique_ptr<TestServerWindowDelegate> server_window_delegate_;
  std::unique_ptr<FrameGenerator> frame_generator_;
  int next_sequence_number_ = 1;

  DISALLOW_COPY_AND_ASSIGN(FrameGeneratorTest);
};

TEST_F(FrameGeneratorTest, InvalidSurfaceInfo) {
  // After SetUP(), frame_generator() has its |is_window_visible_| set to true
  // and |bounds_| to an arbitrary non-empty gfx::Rect but not a valid
  // SurfaceInfo. frame_generator() should not request BeginFrames in this
  // state.
  IssueBeginFrame();
  EXPECT_EQ(0, NumberOfFramesReceived());
}

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
  InitWithSurfaceInfo();

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

TEST_F(FrameGeneratorTest, WindowBoundsChanged) {
  InitWithSurfaceInfo();

  // Window bounds change triggers a BeginFrame.
  constexpr int expected_render_pass_id = 1;
  const gfx::Size kArbitrarySize(3, 4);
  frame_generator()->OnWindowSizeChanged(kArbitrarySize);
  IssueBeginFrame();
  EXPECT_EQ(2, NumberOfFramesReceived());
  cc::RenderPass* received_render_pass = LastRenderPassList().front().get();
  EXPECT_EQ(expected_render_pass_id, received_render_pass->id);
  EXPECT_EQ(kArbitrarySize, received_render_pass->output_rect.size());
  EXPECT_EQ(kArbitrarySize, received_render_pass->damage_rect.size());
  EXPECT_EQ(gfx::Transform(), received_render_pass->transform_to_root_target);
}

// Change window bounds twice before issuing a BeginFrame. The CompositorFrame
// submitted by frame_generator() should only has the second bounds.
TEST_F(FrameGeneratorTest, WindowBoundsChangedTwice) {
  InitWithSurfaceInfo();

  const gfx::Size kArbitrarySize(3, 4);
  const gfx::Size kAnotherArbitrarySize(5, 6);
  frame_generator()->OnWindowSizeChanged(kArbitrarySize);
  frame_generator()->OnWindowSizeChanged(kAnotherArbitrarySize);
  IssueBeginFrame();
  EXPECT_EQ(2, NumberOfFramesReceived());
  cc::RenderPass* received_render_pass = LastRenderPassList().front().get();
  EXPECT_EQ(kAnotherArbitrarySize, received_render_pass->output_rect.size());
  EXPECT_EQ(kAnotherArbitrarySize, received_render_pass->damage_rect.size());

  // frame_generator() stops requesting BeginFrames after getting one.
  IssueBeginFrame();
  EXPECT_EQ(2, NumberOfFramesReceived());
}

TEST_F(FrameGeneratorTest, WindowDamaged) {
  InitWithSurfaceInfo();

  frame_generator()->OnWindowDamaged();
  IssueBeginFrame();
  EXPECT_EQ(2, NumberOfFramesReceived());
}

}  // namespace test
}  // namespace ws
}  // namespace ui
