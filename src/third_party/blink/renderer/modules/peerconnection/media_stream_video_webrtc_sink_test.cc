// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/media_stream_video_webrtc_sink.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_registry.h"
#include "third_party/blink/renderer/modules/mediastream/video_track_adapter_settings.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"

namespace blink {

using ::testing::AllOf;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Optional;

class MockPeerConnectionDependencyFactory2
    : public MockPeerConnectionDependencyFactory {
 public:
  MOCK_METHOD(scoped_refptr<webrtc::VideoTrackSourceInterface>,
              CreateVideoTrackSourceProxy,
              (webrtc::VideoTrackSourceInterface * source),
              (override));
};

class MockVideoTrackSourceProxy : public MockWebRtcVideoTrackSource {
 public:
  MockVideoTrackSourceProxy()
      : MockWebRtcVideoTrackSource(/*supports_encoded_output=*/false) {}
  MOCK_METHOD(void,
              ProcessConstraints,
              (const webrtc::VideoTrackSourceConstraints& constraints),
              (override));
};

class MediaStreamVideoWebRtcSinkTest : public ::testing::Test {
 public:
  void SetVideoTrack() {
    registry_.Init();
    registry_.AddVideoTrack("test video track");
    auto video_components = registry_.test_stream()->VideoComponents();
    component_ = video_components[0];
    // TODO(hta): Verify that component_ is valid. When constraints produce
    // no valid format, using the track will cause a crash.
  }

  void SetVideoTrack(const absl::optional<bool>& noise_reduction) {
    registry_.Init();
    registry_.AddVideoTrack("test video track",
                            blink::VideoTrackAdapterSettings(), noise_reduction,
                            false, 0.0);
    auto video_components = registry_.test_stream()->VideoComponents();
    component_ = video_components[0];
    // TODO(hta): Verify that component_ is valid. When constraints produce
    // no valid format, using the track will cause a crash.
  }

 protected:
  Persistent<MediaStreamComponent> component_;
  Persistent<MockPeerConnectionDependencyFactory> dependency_factory_ =
      MakeGarbageCollected<MockPeerConnectionDependencyFactory>();

 private:
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;

  blink::MockMediaStreamRegistry registry_;
};

TEST_F(MediaStreamVideoWebRtcSinkTest, NoiseReductionDefaultsToNotSet) {
  SetVideoTrack();
  blink::MediaStreamVideoWebRtcSink my_sink(
      component_, dependency_factory_.Get(),
      blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  EXPECT_TRUE(my_sink.webrtc_video_track());
  EXPECT_FALSE(my_sink.SourceNeedsDenoisingForTesting());
}

TEST_F(MediaStreamVideoWebRtcSinkTest,
       ForwardsConstraintsChangeToWebRtcVideoTrackSourceProxy) {
  Persistent<MockPeerConnectionDependencyFactory2> dependency_factory2 =
      MakeGarbageCollected<MockPeerConnectionDependencyFactory2>();
  dependency_factory_ = dependency_factory2;
  MockVideoTrackSourceProxy* source_proxy = nullptr;
  EXPECT_CALL(*dependency_factory2, CreateVideoTrackSourceProxy)
      .WillOnce(Invoke([&source_proxy](webrtc::VideoTrackSourceInterface*) {
        source_proxy = new MockVideoTrackSourceProxy();
        return source_proxy;
      }));
  SetVideoTrack();
  blink::MediaStreamVideoWebRtcSink sink(
      component_, dependency_factory_.Get(),
      blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  ASSERT_TRUE(source_proxy != nullptr);
  Mock::VerifyAndClearExpectations(dependency_factory_);

  EXPECT_CALL(
      *source_proxy,
      ProcessConstraints(AllOf(
          Field(&webrtc::VideoTrackSourceConstraints::min_fps, Optional(12.0)),
          Field(&webrtc::VideoTrackSourceConstraints::max_fps,
                Optional(34.0)))));
  sink.OnVideoConstraintsChanged(12, 34);
}

}  // namespace blink
