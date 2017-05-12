// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/BaseAudioContext.h"

#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentUserGestureToken.h"
#include "core/frame/FrameOwner.h"
#include "core/frame/FrameTypes.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/html/media/AutoplayPolicy.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"
#include "modules/webaudio/AudioContextOptions.h"
#include "platform/UserGestureIndicator.h"
#include "platform/testing/HistogramTester.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/WebAudioDevice.h"
#include "public/platform/WebAudioLatencyHint.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

const char* const kCrossOriginMetric = "WebAudio.Autoplay.CrossOrigin";

class MockCrossOriginLocalFrameClient final : public EmptyLocalFrameClient {
 public:
  static MockCrossOriginLocalFrameClient* Create(Frame* parent) {
    return new MockCrossOriginLocalFrameClient(parent);
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(parent_);
    EmptyLocalFrameClient::Trace(visitor);
  }

  Frame* Parent() const override { return parent_.Get(); }
  Frame* Top() const override { return parent_.Get(); }

 private:
  explicit MockCrossOriginLocalFrameClient(Frame* parent) : parent_(parent) {}

  Member<Frame> parent_;
};

class MockWebAudioDevice : public WebAudioDevice {
 public:
  explicit MockWebAudioDevice(double sample_rate, int frames_per_buffer)
      : sample_rate_(sample_rate), frames_per_buffer_(frames_per_buffer) {}
  ~MockWebAudioDevice() override = default;

  void Start() override {}
  void Stop() override {}
  double SampleRate() override { return sample_rate_; }
  int FramesPerBuffer() override { return frames_per_buffer_; }

 private:
  double sample_rate_;
  int frames_per_buffer_;
};

class BaseAudioContextTestPlatform : public TestingPlatformSupport {
 public:
  std::unique_ptr<WebAudioDevice> CreateAudioDevice(
      unsigned number_of_input_channels,
      unsigned number_of_channels,
      const WebAudioLatencyHint& latency_hint,
      WebAudioDevice::RenderCallback*,
      const WebString& device_id,
      const WebSecurityOrigin&) override {
    return WTF::MakeUnique<MockWebAudioDevice>(AudioHardwareSampleRate(),
                                               AudioHardwareBufferSize());
  }

  double AudioHardwareSampleRate() override { return 44100; }
  size_t AudioHardwareBufferSize() override { return 128; }
};

}  // anonymous namespace

class BaseAudioContextTest : public ::testing::Test {
 protected:
  using AutoplayStatus = BaseAudioContext::AutoplayStatus;

  void SetUp() override {
    dummy_page_holder_ = DummyPageHolder::Create();
    dummy_frame_owner_ = DummyFrameOwner::Create();
    GetDocument().UpdateSecurityOrigin(
        SecurityOrigin::Create("https", "example.com", 80));
  }

  void TearDown() override {
    if (child_frame_)
      child_frame_->Detach(FrameDetachType::kRemove);
  }

  void CreateChildFrame() {
    child_frame_ = LocalFrame::Create(
        MockCrossOriginLocalFrameClient::Create(GetDocument().GetFrame()),
        *GetDocument().GetFrame()->GetPage(), dummy_frame_owner_.Get());
    child_frame_->SetView(FrameView::Create(*child_frame_, IntSize(500, 500)));
    child_frame_->Init();

    ChildDocument().UpdateSecurityOrigin(
        SecurityOrigin::Create("https", "cross-origin.com", 80));
  }

  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }

  Document& ChildDocument() { return *child_frame_->GetDocument(); }

  ScriptState* GetScriptStateFrom(const Document& document) {
    return ToScriptStateForMainWorld(document.GetFrame());
  }

  void RejectPendingResolvers(BaseAudioContext* audio_context) {
    audio_context->RejectPendingResolvers();
  }

  void RecordAutoplayStatus(BaseAudioContext* audio_context) {
    audio_context->RecordAutoplayStatus();
  }

 private:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
  Persistent<DummyFrameOwner> dummy_frame_owner_;
  Persistent<LocalFrame> child_frame_;
  ScopedTestingPlatformSupport<BaseAudioContextTestPlatform> platform_;
};

TEST_F(BaseAudioContextTest, AutoplayMetrics_NoRestriction) {
  HistogramTester histogram_tester;

  BaseAudioContext* audio_context = BaseAudioContext::Create(
      GetDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);
  RecordAutoplayStatus(audio_context);

  histogram_tester.ExpectTotalCount(kCrossOriginMetric, 0);
}

TEST_F(BaseAudioContextTest, AutoplayMetrics_CreateNoGesture) {
  HistogramTester histogram_tester;
  CreateChildFrame();
  ChildDocument().GetSettings()->SetAutoplayPolicy(
      AutoplayPolicy::Type::kUserGestureRequired);

  BaseAudioContext* audio_context = BaseAudioContext::Create(
      ChildDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);
  RecordAutoplayStatus(audio_context);

  histogram_tester.ExpectBucketCount(kCrossOriginMetric,
                                     AutoplayStatus::kAutoplayStatusFailed, 1);
  histogram_tester.ExpectTotalCount(kCrossOriginMetric, 1);
}

TEST_F(BaseAudioContextTest, AutoplayMetrics_CallResumeNoGesture) {
  HistogramTester histogram_tester;
  CreateChildFrame();
  ChildDocument().GetSettings()->SetAutoplayPolicy(
      AutoplayPolicy::Type::kUserGestureRequired);

  ScriptState::Scope scope(GetScriptStateFrom(ChildDocument()));

  BaseAudioContext* audio_context = BaseAudioContext::Create(
      ChildDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);
  audio_context->resumeContext(GetScriptStateFrom(ChildDocument()));
  RejectPendingResolvers(audio_context);
  RecordAutoplayStatus(audio_context);

  histogram_tester.ExpectBucketCount(kCrossOriginMetric,
                                     AutoplayStatus::kAutoplayStatusFailed, 1);
  histogram_tester.ExpectTotalCount(kCrossOriginMetric, 1);
}

TEST_F(BaseAudioContextTest, AutoplayMetrics_CreateGesture) {
  HistogramTester histogram_tester;
  CreateChildFrame();
  ChildDocument().GetSettings()->SetAutoplayPolicy(
      AutoplayPolicy::Type::kUserGestureRequired);

  UserGestureIndicator user_gesture_scope(DocumentUserGestureToken::Create(
      &ChildDocument(), UserGestureToken::kNewGesture));

  BaseAudioContext* audio_context = BaseAudioContext::Create(
      ChildDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);
  RecordAutoplayStatus(audio_context);

  histogram_tester.ExpectBucketCount(
      kCrossOriginMetric, AutoplayStatus::kAutoplayStatusSucceeded, 1);
  histogram_tester.ExpectTotalCount(kCrossOriginMetric, 1);
}

TEST_F(BaseAudioContextTest, AutoplayMetrics_CallResumeGesture) {
  HistogramTester histogram_tester;
  CreateChildFrame();
  ChildDocument().GetSettings()->SetAutoplayPolicy(
      AutoplayPolicy::Type::kUserGestureRequired);

  ScriptState::Scope scope(GetScriptStateFrom(ChildDocument()));

  BaseAudioContext* audio_context = BaseAudioContext::Create(
      ChildDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);

  UserGestureIndicator user_gesture_scope(DocumentUserGestureToken::Create(
      &ChildDocument(), UserGestureToken::kNewGesture));

  audio_context->resumeContext(GetScriptStateFrom(ChildDocument()));
  RejectPendingResolvers(audio_context);
  RecordAutoplayStatus(audio_context);

  histogram_tester.ExpectBucketCount(
      kCrossOriginMetric, AutoplayStatus::kAutoplayStatusSucceeded, 1);
  histogram_tester.ExpectTotalCount(kCrossOriginMetric, 1);
}

TEST_F(BaseAudioContextTest, AutoplayMetrics_NodeStartNoGesture) {
  HistogramTester histogram_tester;
  CreateChildFrame();
  ChildDocument().GetSettings()->SetAutoplayPolicy(
      AutoplayPolicy::Type::kUserGestureRequired);

  BaseAudioContext* audio_context = BaseAudioContext::Create(
      ChildDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);
  audio_context->MaybeRecordStartAttempt();
  RecordAutoplayStatus(audio_context);

  histogram_tester.ExpectBucketCount(kCrossOriginMetric,
                                     AutoplayStatus::kAutoplayStatusFailed, 1);
  histogram_tester.ExpectTotalCount(kCrossOriginMetric, 1);
}

TEST_F(BaseAudioContextTest, AutoplayMetrics_NodeStartGesture) {
  HistogramTester histogram_tester;
  CreateChildFrame();
  ChildDocument().GetSettings()->SetAutoplayPolicy(
      AutoplayPolicy::Type::kUserGestureRequired);

  BaseAudioContext* audio_context = BaseAudioContext::Create(
      ChildDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);

  UserGestureIndicator user_gesture_scope(DocumentUserGestureToken::Create(
      &ChildDocument(), UserGestureToken::kNewGesture));
  audio_context->MaybeRecordStartAttempt();
  RecordAutoplayStatus(audio_context);

  histogram_tester.ExpectBucketCount(
      kCrossOriginMetric, AutoplayStatus::kAutoplayStatusFailedWithStart, 1);
  histogram_tester.ExpectTotalCount(kCrossOriginMetric, 1);
}

TEST_F(BaseAudioContextTest, AutoplayMetrics_NodeStartNoGestureThenSuccess) {
  HistogramTester histogram_tester;
  CreateChildFrame();
  ChildDocument().GetSettings()->SetAutoplayPolicy(
      AutoplayPolicy::Type::kUserGestureRequired);

  ScriptState::Scope scope(GetScriptStateFrom(ChildDocument()));

  BaseAudioContext* audio_context = BaseAudioContext::Create(
      ChildDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);
  audio_context->MaybeRecordStartAttempt();

  UserGestureIndicator user_gesture_scope(DocumentUserGestureToken::Create(
      &ChildDocument(), UserGestureToken::kNewGesture));
  audio_context->resumeContext(GetScriptStateFrom(ChildDocument()));
  RejectPendingResolvers(audio_context);
  RecordAutoplayStatus(audio_context);

  histogram_tester.ExpectBucketCount(
      kCrossOriginMetric, AutoplayStatus::kAutoplayStatusSucceeded, 1);
  histogram_tester.ExpectTotalCount(kCrossOriginMetric, 1);
}

TEST_F(BaseAudioContextTest, AutoplayMetrics_NodeStartGestureThenSucces) {
  HistogramTester histogram_tester;
  CreateChildFrame();
  ChildDocument().GetSettings()->SetAutoplayPolicy(
      AutoplayPolicy::Type::kUserGestureRequired);

  ScriptState::Scope scope(GetScriptStateFrom(ChildDocument()));

  BaseAudioContext* audio_context = BaseAudioContext::Create(
      ChildDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);

  UserGestureIndicator user_gesture_scope(DocumentUserGestureToken::Create(
      &ChildDocument(), UserGestureToken::kNewGesture));
  audio_context->MaybeRecordStartAttempt();
  audio_context->resumeContext(GetScriptStateFrom(ChildDocument()));
  RejectPendingResolvers(audio_context);
  RecordAutoplayStatus(audio_context);

  histogram_tester.ExpectBucketCount(
      kCrossOriginMetric, AutoplayStatus::kAutoplayStatusSucceeded, 1);
  histogram_tester.ExpectTotalCount(kCrossOriginMetric, 1);
}

}  // namespace blink
