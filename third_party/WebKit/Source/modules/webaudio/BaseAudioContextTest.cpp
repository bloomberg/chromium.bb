// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/BaseAudioContext.h"

#include "bindings/core/v8/V8BindingForCore.h"
#include "build/build_config.h"
#include "core/dom/Document.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/frame/FrameOwner.h"
#include "core/frame/FrameTypes.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/html/media/AutoplayPolicy.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"
#include "modules/webaudio/AudioContextOptions.h"
#include "modules/webaudio/AudioWorkletThread.h"
#include "platform/testing/HistogramTester.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/WebAudioDevice.h"
#include "public/platform/WebAudioLatencyHint.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

const char* const kAutoplayMetric = "WebAudio.Autoplay";
const char* const kAutoplayCrossOriginMetric = "WebAudio.Autoplay.CrossOrigin";

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

class MockWebAudioDeviceForBaseAudioContext : public WebAudioDevice {
 public:
  explicit MockWebAudioDeviceForBaseAudioContext(double sample_rate,
                                                 int frames_per_buffer)
      : sample_rate_(sample_rate), frames_per_buffer_(frames_per_buffer) {}
  ~MockWebAudioDeviceForBaseAudioContext() override = default;

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
    return WTF::MakeUnique<MockWebAudioDeviceForBaseAudioContext>(
        AudioHardwareSampleRate(), AudioHardwareBufferSize());
  }

  std::unique_ptr<WebThread> CreateThread(const char* name) override {
    // return WTF::WrapUnique(old_platform_->CurrentThread());
    return old_platform_->CreateThread(name);
  }

  double AudioHardwareSampleRate() override { return 44100; }
  size_t AudioHardwareBufferSize() override { return 128; }
};

}  // anonymous namespace

#if defined(OS_ANDROID)
// Often times out on Android: https://crbug.com/752511.
#define MAYBE_TEST_P(test_case_name, test_name) \
  TEST_P(test_case_name, DISABLED_##test_name)
#else
#define MAYBE_TEST_P TEST_P
#endif

class BaseAudioContextAutoplayTest
    : public ::testing::TestWithParam<AutoplayPolicy::Type> {
 protected:
  using AutoplayStatus = BaseAudioContext::AutoplayStatus;

  void SetUp() override {
    dummy_page_holder_ = DummyPageHolder::Create();
    dummy_frame_owner_ = DummyFrameOwner::Create();
    GetDocument().UpdateSecurityOrigin(
        SecurityOrigin::Create("https", "example.com", 80));

    CreateChildFrame();

    GetDocument().GetSettings()->SetAutoplayPolicy(GetParam());
    ChildDocument().GetSettings()->SetAutoplayPolicy(GetParam());

    histogram_tester_ = WTF::MakeUnique<HistogramTester>();
    AudioWorkletThread::CreateSharedBackingThreadForTest();
  }

  void TearDown() override {
    if (child_frame_)
      child_frame_->Detach(FrameDetachType::kRemove);

    AudioWorkletThread::ClearSharedBackingThread();
  }

  void CreateChildFrame() {
    child_frame_ = LocalFrame::Create(
        MockCrossOriginLocalFrameClient::Create(GetDocument().GetFrame()),
        *GetDocument().GetFrame()->GetPage(), dummy_frame_owner_.Get());
    child_frame_->SetView(
        LocalFrameView::Create(*child_frame_, IntSize(500, 500)));
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

  HistogramTester* GetHistogramTester() {
    return histogram_tester_.get();
  }

 private:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
  Persistent<DummyFrameOwner> dummy_frame_owner_;
  Persistent<LocalFrame> child_frame_;
  std::unique_ptr<HistogramTester> histogram_tester_;
  ScopedTestingPlatformSupport<BaseAudioContextTestPlatform> platform_;
};

// Creates an AudioContext without a gesture inside a x-origin child frame.
MAYBE_TEST_P(BaseAudioContextAutoplayTest,
             AutoplayMetrics_CreateNoGesture_Child) {
  BaseAudioContext* audio_context = BaseAudioContext::Create(
      ChildDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusFailed, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayCrossOriginMetric, AutoplayStatus::kAutoplayStatusFailed, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 1);
      break;
  }
}

// Creates an AudioContext without a gesture inside a main frame.
MAYBE_TEST_P(BaseAudioContextAutoplayTest,
             AutoplayMetrics_CreateNoGesture_Main) {
  BaseAudioContext* audio_context = BaseAudioContext::Create(
      GetDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusFailed, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
  }
}

// Creates an AudioContext then call resume without a gesture in a x-origin
// child frame.
MAYBE_TEST_P(BaseAudioContextAutoplayTest,
             AutoplayMetrics_CallResumeNoGesture_Child) {
  ScriptState::Scope scope(GetScriptStateFrom(ChildDocument()));

  BaseAudioContext* audio_context = BaseAudioContext::Create(
      ChildDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);
  audio_context->resumeContext(GetScriptStateFrom(ChildDocument()));
  RejectPendingResolvers(audio_context);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusFailed, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayCrossOriginMetric, AutoplayStatus::kAutoplayStatusFailed, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 1);
      break;
  }
}

// Creates an AudioContext then call resume without a gesture in a main frame.
MAYBE_TEST_P(BaseAudioContextAutoplayTest,
             AutoplayMetrics_CallResumeNoGesture_Main) {
  ScriptState::Scope scope(GetScriptStateFrom(GetDocument()));

  BaseAudioContext* audio_context = BaseAudioContext::Create(
      GetDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);
  audio_context->resumeContext(GetScriptStateFrom(ChildDocument()));
  RejectPendingResolvers(audio_context);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusFailed, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
  }
}

// Creates an AudioContext with a user gesture inside a x-origin child frame.
MAYBE_TEST_P(BaseAudioContextAutoplayTest,
             AutoplayMetrics_CreateGesture_Child) {
  std::unique_ptr<UserGestureIndicator> user_gesture_scope =
      LocalFrame::CreateUserGesture(ChildDocument().GetFrame(),
                                    UserGestureToken::kNewGesture);

  BaseAudioContext* audio_context = BaseAudioContext::Create(
      ChildDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusSucceeded, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayCrossOriginMetric, AutoplayStatus::kAutoplayStatusSucceeded,
          1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 1);
      break;
  }
}

// Creates an AudioContext with a user gesture inside a main frame.
MAYBE_TEST_P(BaseAudioContextAutoplayTest, AutoplayMetrics_CreateGesture_Main) {
  std::unique_ptr<UserGestureIndicator> user_gesture_scope =
      LocalFrame::CreateUserGesture(GetDocument().GetFrame(),
                                    UserGestureToken::kNewGesture);

  BaseAudioContext* audio_context = BaseAudioContext::Create(
      GetDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusSucceeded, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
  }
}

// Creates an AudioContext then calls resume with a user gesture inside a
// x-origin child frame.
MAYBE_TEST_P(BaseAudioContextAutoplayTest,
             AutoplayMetrics_CallResumeGesture_Child) {
  ScriptState::Scope scope(GetScriptStateFrom(ChildDocument()));

  BaseAudioContext* audio_context = BaseAudioContext::Create(
      ChildDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);

  std::unique_ptr<UserGestureIndicator> user_gesture_scope =
      LocalFrame::CreateUserGesture(ChildDocument().GetFrame(),
                                    UserGestureToken::kNewGesture);

  audio_context->resumeContext(GetScriptStateFrom(ChildDocument()));
  RejectPendingResolvers(audio_context);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusSucceeded, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayCrossOriginMetric, AutoplayStatus::kAutoplayStatusSucceeded,
          1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 1);
      break;
  }
}

// Creates an AudioContext then calls resume with a user gesture inside a main
// frame.
MAYBE_TEST_P(BaseAudioContextAutoplayTest,
             AutoplayMetrics_CallResumeGesture_Main) {
  ScriptState::Scope scope(GetScriptStateFrom(GetDocument()));

  BaseAudioContext* audio_context = BaseAudioContext::Create(
      GetDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);

  std::unique_ptr<UserGestureIndicator> user_gesture_scope =
      LocalFrame::CreateUserGesture(GetDocument().GetFrame(),
                                    UserGestureToken::kNewGesture);

  audio_context->resumeContext(GetScriptStateFrom(GetDocument()));
  RejectPendingResolvers(audio_context);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusSucceeded, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
  }
}

// Creates an AudioContext then calls start on a node without a gesture inside a
// x-origin child frame.
MAYBE_TEST_P(BaseAudioContextAutoplayTest,
             AutoplayMetrics_NodeStartNoGesture_Child) {
  BaseAudioContext* audio_context = BaseAudioContext::Create(
      ChildDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);
  audio_context->MaybeRecordStartAttempt();
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusFailed, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayCrossOriginMetric, AutoplayStatus::kAutoplayStatusFailed, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 1);
      break;
  }
}

// Creates an AudioContext then calls start on a node without a gesture inside a
// main frame.
MAYBE_TEST_P(BaseAudioContextAutoplayTest,
             AutoplayMetrics_NodeStartNoGesture_Main) {
  BaseAudioContext* audio_context = BaseAudioContext::Create(
      GetDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);
  audio_context->MaybeRecordStartAttempt();
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusFailed, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
  }
}

// Creates an AudioContext then calls start on a node with a gesture inside a
// x-origin child frame.
MAYBE_TEST_P(BaseAudioContextAutoplayTest,
             AutoplayMetrics_NodeStartGesture_Child) {
  BaseAudioContext* audio_context = BaseAudioContext::Create(
      ChildDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);

  std::unique_ptr<UserGestureIndicator> user_gesture_scope =
      LocalFrame::CreateUserGesture(ChildDocument().GetFrame(),
                                    UserGestureToken::kNewGesture);
  audio_context->MaybeRecordStartAttempt();
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusFailedWithStart, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayCrossOriginMetric,
          AutoplayStatus::kAutoplayStatusFailedWithStart, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 1);
      break;
  }
}

// Creates an AudioContext then calls start on a node with a gesture inside a
// main frame.
MAYBE_TEST_P(BaseAudioContextAutoplayTest,
             AutoplayMetrics_NodeStartGesture_Main) {
  BaseAudioContext* audio_context = BaseAudioContext::Create(
      GetDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);

  std::unique_ptr<UserGestureIndicator> user_gesture_scope =
      LocalFrame::CreateUserGesture(GetDocument().GetFrame(),
                                    UserGestureToken::kNewGesture);
  audio_context->MaybeRecordStartAttempt();
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusFailedWithStart, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
  }
}

// Creates an AudioContext then calls start on a node without a gesture and
// finally allows the AudioContext to produce sound inside x-origin child frame.
MAYBE_TEST_P(BaseAudioContextAutoplayTest,
             AutoplayMetrics_NodeStartNoGestureThenSuccess_Child) {
  ScriptState::Scope scope(GetScriptStateFrom(ChildDocument()));

  BaseAudioContext* audio_context = BaseAudioContext::Create(
      ChildDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);
  audio_context->MaybeRecordStartAttempt();

  std::unique_ptr<UserGestureIndicator> user_gesture_scope =
      LocalFrame::CreateUserGesture(ChildDocument().GetFrame(),
                                    UserGestureToken::kNewGesture);
  audio_context->resumeContext(GetScriptStateFrom(ChildDocument()));
  RejectPendingResolvers(audio_context);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusSucceeded, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayCrossOriginMetric, AutoplayStatus::kAutoplayStatusSucceeded,
          1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 1);
      break;
  }
}

// Creates an AudioContext then calls start on a node without a gesture and
// finally allows the AudioContext to produce sound inside a main frame.
MAYBE_TEST_P(BaseAudioContextAutoplayTest,
             AutoplayMetrics_NodeStartNoGestureThenSuccess_Main) {
  ScriptState::Scope scope(GetScriptStateFrom(GetDocument()));

  BaseAudioContext* audio_context = BaseAudioContext::Create(
      GetDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);
  audio_context->MaybeRecordStartAttempt();

  std::unique_ptr<UserGestureIndicator> user_gesture_scope =
      LocalFrame::CreateUserGesture(GetDocument().GetFrame(),
                                    UserGestureToken::kNewGesture);
  audio_context->resumeContext(GetScriptStateFrom(GetDocument()));
  RejectPendingResolvers(audio_context);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusSucceeded, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
  }
}

// Creates an AudioContext then calls start on a node with a gesture and
// finally allows the AudioContext to produce sound inside x-origin child frame.
MAYBE_TEST_P(BaseAudioContextAutoplayTest,
             AutoplayMetrics_NodeStartGestureThenSucces_Child) {
  ScriptState::Scope scope(GetScriptStateFrom(ChildDocument()));

  BaseAudioContext* audio_context = BaseAudioContext::Create(
      ChildDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);

  std::unique_ptr<UserGestureIndicator> user_gesture_scope =
      LocalFrame::CreateUserGesture(ChildDocument().GetFrame(),
                                    UserGestureToken::kNewGesture);
  audio_context->MaybeRecordStartAttempt();
  audio_context->resumeContext(GetScriptStateFrom(ChildDocument()));
  RejectPendingResolvers(audio_context);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusSucceeded, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayCrossOriginMetric, AutoplayStatus::kAutoplayStatusSucceeded,
          1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 1);
      break;
  }
}

// Creates an AudioContext then calls start on a node with a gesture and
// finally allows the AudioContext to produce sound inside a main frame.
MAYBE_TEST_P(BaseAudioContextAutoplayTest,
             AutoplayMetrics_NodeStartGestureThenSucces_Main) {
  ScriptState::Scope scope(GetScriptStateFrom(GetDocument()));

  BaseAudioContext* audio_context = BaseAudioContext::Create(
      GetDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);

  std::unique_ptr<UserGestureIndicator> user_gesture_scope =
      LocalFrame::CreateUserGesture(GetDocument().GetFrame(),
                                    UserGestureToken::kNewGesture);
  audio_context->MaybeRecordStartAttempt();
  audio_context->resumeContext(GetScriptStateFrom(GetDocument()));
  RejectPendingResolvers(audio_context);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusSucceeded, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
  }
}

// Attempts to autoplay an AudioContext in a x-origin child frame when the
// document previous received a user gesture.
MAYBE_TEST_P(BaseAudioContextAutoplayTest,
             AutoplayMetrics_DocumentReceivedGesture_Child) {
  ChildDocument().GetFrame()->UpdateUserActivationInFrameTree();

  BaseAudioContext* audio_context = BaseAudioContext::Create(
      ChildDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusFailed, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayCrossOriginMetric, AutoplayStatus::kAutoplayStatusFailed, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 1);
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusSucceeded, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayCrossOriginMetric, AutoplayStatus::kAutoplayStatusSucceeded,
          1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 1);
      break;
  }
}

// Attempts to autoplay an AudioContext in a main child frame when the
// document previous received a user gesture.
MAYBE_TEST_P(BaseAudioContextAutoplayTest,
             AutoplayMetrics_DocumentReceivedGesture_Main) {
  GetDocument().GetFrame()->UpdateUserActivationInFrameTree();

  BaseAudioContext* audio_context = BaseAudioContext::Create(
      GetDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusSucceeded, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
  }
}

// Attempts to autoplay an AudioContext in a main child frame when the
// document received a user gesture before navigation.
MAYBE_TEST_P(BaseAudioContextAutoplayTest,
             AutoplayMetrics_DocumentReceivedGesture_BeforeNavigation) {
  GetDocument().GetFrame()->SetDocumentHasReceivedUserGestureBeforeNavigation(
      true);

  BaseAudioContext* audio_context = BaseAudioContext::Create(
      GetDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusSucceeded, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
  }
}

INSTANTIATE_TEST_CASE_P(
    BaseAudioContextAutoplayTest,
    BaseAudioContextAutoplayTest,
    ::testing::Values(AutoplayPolicy::Type::kNoUserGestureRequired,
                      AutoplayPolicy::Type::kUserGestureRequired,
                      AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin,
                      AutoplayPolicy::Type::kDocumentUserActivationRequired));

}  // namespace blink
