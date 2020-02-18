// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_MOCK_WEB_RTC_PEER_CONNECTION_HANDLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_MOCK_WEB_RTC_PEER_CONNECTION_HANDLER_H_

#include <memory>
#include <string>

#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/platform/web_rtc_peer_connection_handler.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/webrtc/api/peer_connection_interface.h"
#include "third_party/webrtc/api/stats/rtc_stats.h"

namespace blink {

// TODO(https://crbug.com/908461): This is currently implemented as NO-OPs or to
// create dummy objects whose methods return default values. Consider renaming
// the class, changing it to be GMOCK friendly or deleting it.
class MockWebRTCPeerConnectionHandler : public WebRTCPeerConnectionHandler {
 public:
  MockWebRTCPeerConnectionHandler();
  ~MockWebRTCPeerConnectionHandler() override;

  bool Initialize(const webrtc::PeerConnectionInterface::RTCConfiguration&,
                  const WebMediaConstraints&) override;

  WebVector<std::unique_ptr<RTCRtpTransceiverPlatform>> CreateOffer(
      RTCSessionDescriptionRequest*,
      const WebMediaConstraints&) override;
  WebVector<std::unique_ptr<RTCRtpTransceiverPlatform>> CreateOffer(
      RTCSessionDescriptionRequest*,
      RTCOfferOptionsPlatform*) override;
  void CreateAnswer(RTCSessionDescriptionRequest*,
                    const WebMediaConstraints&) override;
  void CreateAnswer(RTCSessionDescriptionRequest*,
                    RTCAnswerOptionsPlatform*) override;
  void SetLocalDescription(RTCVoidRequest*) override;
  void SetLocalDescription(RTCVoidRequest*,
                           RTCSessionDescriptionPlatform*) override;
  void SetRemoteDescription(RTCVoidRequest*,
                            RTCSessionDescriptionPlatform*) override;
  RTCSessionDescriptionPlatform* LocalDescription() override;
  RTCSessionDescriptionPlatform* RemoteDescription() override;
  RTCSessionDescriptionPlatform* CurrentLocalDescription() override;
  RTCSessionDescriptionPlatform* CurrentRemoteDescription() override;
  RTCSessionDescriptionPlatform* PendingLocalDescription() override;
  RTCSessionDescriptionPlatform* PendingRemoteDescription() override;
  const webrtc::PeerConnectionInterface::RTCConfiguration& GetConfiguration()
      const override;
  webrtc::RTCErrorType SetConfiguration(
      const webrtc::PeerConnectionInterface::RTCConfiguration&) override;
  void AddICECandidate(RTCVoidRequest*,
                       scoped_refptr<RTCIceCandidatePlatform>) override;
  void RestartIce() override;
  void GetStats(RTCStatsRequest*) override;
  void GetStats(WebRTCStatsReportCallback,
                const WebVector<webrtc::NonStandardGroupId>&) override;
  webrtc::RTCErrorOr<std::unique_ptr<RTCRtpTransceiverPlatform>>
  AddTransceiverWithTrack(const WebMediaStreamTrack&,
                          const webrtc::RtpTransceiverInit&) override;
  webrtc::RTCErrorOr<std::unique_ptr<RTCRtpTransceiverPlatform>>
  AddTransceiverWithKind(std::string kind,
                         const webrtc::RtpTransceiverInit&) override;
  webrtc::RTCErrorOr<std::unique_ptr<RTCRtpTransceiverPlatform>> AddTrack(
      const WebMediaStreamTrack&,
      const WebVector<WebMediaStream>&) override;
  webrtc::RTCErrorOr<std::unique_ptr<RTCRtpTransceiverPlatform>> RemoveTrack(
      RTCRtpSenderPlatform*) override;
  scoped_refptr<webrtc::DataChannelInterface> CreateDataChannel(
      const WebString& label,
      const webrtc::DataChannelInit&) override;
  void Stop() override;
  webrtc::PeerConnectionInterface* NativePeerConnection() override;
  void RunSynchronousOnceClosureOnSignalingThread(
      base::OnceClosure closure,
      const char* trace_event_name) override;
  void RunSynchronousRepeatingClosureOnSignalingThread(
      const base::RepeatingClosure& closure,
      const char* trace_event_name) override;
  void TrackIceConnectionStateChange(
      WebRTCPeerConnectionHandler::IceConnectionStateVersion version,
      webrtc::PeerConnectionInterface::IceConnectionState state) override;

 private:
  class DummyRTCRtpTransceiverPlatform;

  Vector<std::unique_ptr<DummyRTCRtpTransceiverPlatform>> transceivers_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_MOCK_WEB_RTC_PEER_CONNECTION_HANDLER_H_
