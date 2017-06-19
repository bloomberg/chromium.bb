// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/peerconnection/RTCPeerConnection.h"

#include <string>

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "modules/mediastream/MediaStream.h"
#include "modules/mediastream/MediaStreamTrack.h"
#include "modules/peerconnection/RTCConfiguration.h"
#include "modules/peerconnection/RTCIceServer.h"
#include "modules/peerconnection/testing/TestingPlatformSupportWithWebRTC.h"
#include "platform/heap/HeapAllocator.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "public/platform/Platform.h"
#include "public/platform/WebRTCError.h"
#include "public/platform/WebRTCPeerConnectionHandler.h"
#include "public/platform/WebRTCRtpReceiver.h"
#include "public/platform/WebRTCRtpSender.h"
#include "public/platform/WebRTCSessionDescription.h"
#include "public/web/WebHeap.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class RTCPeerConnectionTest : public ::testing::Test {
 public:
  RTCPeerConnection* CreatePC(V8TestingScope& scope) {
    RTCConfiguration config;
    RTCIceServer ice_server;
    ice_server.setURL("stun:fake.stun.url");
    HeapVector<RTCIceServer> ice_servers;
    ice_servers.push_back(ice_server);
    config.setIceServers(ice_servers);
    return RTCPeerConnection::Create(scope.GetExecutionContext(), config,
                                     Dictionary(), scope.GetExceptionState());
  }

  MediaStreamTrack* CreateTrack(V8TestingScope& scope,
                                MediaStreamSource::StreamType type,
                                String id) {
    MediaStreamSource* source =
        MediaStreamSource::Create("sourceId", type, "sourceName", false);
    MediaStreamComponent* component = MediaStreamComponent::Create(id, source);
    return MediaStreamTrack::Create(scope.GetExecutionContext(), component);
  }

  std::string GetExceptionMessage(V8TestingScope& scope) {
    ExceptionState& exception_state = scope.GetExceptionState();
    return exception_state.HadException()
               ? exception_state.Message().Utf8().data()
               : "";
  }

  void AddStream(V8TestingScope& scope,
                 RTCPeerConnection* pc,
                 MediaStream* stream) {
    pc->addStream(scope.GetScriptState(), stream, Dictionary(),
                  scope.GetExceptionState());
    EXPECT_EQ("", GetExceptionMessage(scope));
  }

  void RemoveStream(V8TestingScope& scope,
                    RTCPeerConnection* pc,
                    MediaStream* stream) {
    pc->removeStream(stream, scope.GetExceptionState());
    EXPECT_EQ("", GetExceptionMessage(scope));
  }

  MediaStreamTrack* GetTrack(RTCPeerConnection* pc,
                             MediaStreamComponent* component) {
    return pc->GetTrack(component);
  }

 private:
  ScopedTestingPlatformSupport<TestingPlatformSupportWithWebRTC> platform;
};

TEST_F(RTCPeerConnectionTest, GetAudioTrack) {
  V8TestingScope scope;
  RTCPeerConnection* pc = CreatePC(scope);
  EXPECT_EQ("", GetExceptionMessage(scope));
  ASSERT_TRUE(pc);

  MediaStreamTrack* track =
      CreateTrack(scope, MediaStreamSource::kTypeAudio, "audioTrack");
  HeapVector<Member<MediaStreamTrack>> tracks;
  tracks.push_back(track);
  MediaStream* stream =
      MediaStream::Create(scope.GetExecutionContext(), tracks);
  ASSERT_TRUE(stream);

  EXPECT_FALSE(GetTrack(pc, track->Component()));
  AddStream(scope, pc, stream);
  EXPECT_TRUE(GetTrack(pc, track->Component()));
}

TEST_F(RTCPeerConnectionTest, GetVideoTrack) {
  V8TestingScope scope;
  RTCPeerConnection* pc = CreatePC(scope);
  EXPECT_EQ("", GetExceptionMessage(scope));
  ASSERT_TRUE(pc);

  MediaStreamTrack* track =
      CreateTrack(scope, MediaStreamSource::kTypeVideo, "videoTrack");
  HeapVector<Member<MediaStreamTrack>> tracks;
  tracks.push_back(track);
  MediaStream* stream =
      MediaStream::Create(scope.GetExecutionContext(), tracks);
  ASSERT_TRUE(stream);

  EXPECT_FALSE(GetTrack(pc, track->Component()));
  AddStream(scope, pc, stream);
  EXPECT_TRUE(GetTrack(pc, track->Component()));
}

TEST_F(RTCPeerConnectionTest, GetAudioAndVideoTrack) {
  V8TestingScope scope;
  RTCPeerConnection* pc = CreatePC(scope);
  EXPECT_EQ("", GetExceptionMessage(scope));
  ASSERT_TRUE(pc);

  HeapVector<Member<MediaStreamTrack>> tracks;
  MediaStreamTrack* audio_track =
      CreateTrack(scope, MediaStreamSource::kTypeAudio, "audioTrack");
  tracks.push_back(audio_track);
  MediaStreamTrack* video_track =
      CreateTrack(scope, MediaStreamSource::kTypeVideo, "videoTrack");
  tracks.push_back(video_track);

  MediaStream* stream =
      MediaStream::Create(scope.GetExecutionContext(), tracks);
  ASSERT_TRUE(stream);

  EXPECT_FALSE(GetTrack(pc, audio_track->Component()));
  EXPECT_FALSE(GetTrack(pc, video_track->Component()));
  AddStream(scope, pc, stream);
  EXPECT_TRUE(GetTrack(pc, audio_track->Component()));
  EXPECT_TRUE(GetTrack(pc, video_track->Component()));
}

TEST_F(RTCPeerConnectionTest, GetTrackRemoveStreamAndGCAll) {
  V8TestingScope scope;
  Persistent<RTCPeerConnection> pc = CreatePC(scope);
  EXPECT_EQ("", GetExceptionMessage(scope));
  ASSERT_TRUE(pc);

  MediaStreamTrack* track =
      CreateTrack(scope, MediaStreamSource::kTypeAudio, "audioTrack");
  HeapVector<Member<MediaStreamTrack>> tracks;
  tracks.push_back(track);
  MediaStream* stream =
      MediaStream::Create(scope.GetExecutionContext(), tracks);
  ASSERT_TRUE(stream);

  MediaStreamComponent* track_component = track->Component();

  EXPECT_FALSE(GetTrack(pc, track_component));
  AddStream(scope, pc, stream);
  EXPECT_TRUE(GetTrack(pc, track_component));

  RemoveStream(scope, pc, stream);
  // This will destroy |MediaStream|, |MediaStreamTrack| and its
  // |MediaStreamComponent|, which will remove its mapping from the peer
  // connection.
  WebHeap::CollectAllGarbageForTesting();
  EXPECT_FALSE(GetTrack(pc, track_component));
}

TEST_F(RTCPeerConnectionTest,
       GetTrackRemoveStreamAndGCWithPersistentComponent) {
  V8TestingScope scope;
  Persistent<RTCPeerConnection> pc = CreatePC(scope);
  EXPECT_EQ("", GetExceptionMessage(scope));
  ASSERT_TRUE(pc);

  MediaStreamTrack* track =
      CreateTrack(scope, MediaStreamSource::kTypeAudio, "audioTrack");
  HeapVector<Member<MediaStreamTrack>> tracks;
  tracks.push_back(track);
  MediaStream* stream =
      MediaStream::Create(scope.GetExecutionContext(), tracks);
  ASSERT_TRUE(stream);

  Persistent<MediaStreamComponent> track_component = track->Component();

  EXPECT_FALSE(GetTrack(pc, track_component));
  AddStream(scope, pc, stream);
  EXPECT_TRUE(GetTrack(pc, track_component));

  RemoveStream(scope, pc, stream);
  // This will destroy |MediaStream| and |MediaStreamTrack| (but not
  // |MediaStreamComponent|), which will remove its mapping from the peer
  // connection.
  WebHeap::CollectAllGarbageForTesting();
  EXPECT_FALSE(GetTrack(pc, track_component));
}

TEST_F(RTCPeerConnectionTest, GetTrackRemoveStreamAndGCWithPersistentStream) {
  V8TestingScope scope;
  Persistent<RTCPeerConnection> pc = CreatePC(scope);
  EXPECT_EQ("", GetExceptionMessage(scope));
  ASSERT_TRUE(pc);

  MediaStreamTrack* track =
      CreateTrack(scope, MediaStreamSource::kTypeAudio, "audioTrack");
  HeapVector<Member<MediaStreamTrack>> tracks;
  tracks.push_back(track);
  Persistent<MediaStream> stream =
      MediaStream::Create(scope.GetExecutionContext(), tracks);
  ASSERT_TRUE(stream);

  MediaStreamComponent* track_component = track->Component();

  EXPECT_FALSE(GetTrack(pc, track_component));
  AddStream(scope, pc, stream);
  EXPECT_TRUE(GetTrack(pc, track_component));

  RemoveStream(scope, pc, stream);
  // With a persistent |MediaStream|, the |MediaStreamTrack| and
  // |MediaStreamComponent| will not be destroyed and continue to be mapped by
  // peer connection.
  WebHeap::CollectAllGarbageForTesting();
  EXPECT_TRUE(GetTrack(pc, track_component));

  stream = nullptr;
  // Now |MediaStream|, |MediaStreamTrack| and |MediaStreamComponent| will be
  // destroyed and the mapping removed from the peer connection.
  WebHeap::CollectAllGarbageForTesting();
  EXPECT_FALSE(GetTrack(pc, track_component));
}

}  // namespace blink
