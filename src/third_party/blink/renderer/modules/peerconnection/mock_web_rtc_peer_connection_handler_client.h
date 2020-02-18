// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_MOCK_WEB_RTC_PEER_CONNECTION_HANDLER_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_MOCK_WEB_RTC_PEER_CONNECTION_HANDLER_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/platform/web_media_stream.h"
#include "third_party/blink/public/platform/web_rtc_peer_connection_handler_client.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_ice_candidate_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_rtp_receiver_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_rtp_transceiver_platform.h"

namespace blink {

class MockWebRTCPeerConnectionHandlerClient
    : public blink::WebRTCPeerConnectionHandlerClient {
 public:
  MockWebRTCPeerConnectionHandlerClient();
  ~MockWebRTCPeerConnectionHandlerClient() override;

  // WebRTCPeerConnectionHandlerClient implementation.
  MOCK_METHOD0(NegotiationNeeded, void());
  MOCK_METHOD1(DidGenerateICECandidate,
               void(scoped_refptr<RTCIceCandidatePlatform> candidate));
  MOCK_METHOD4(DidFailICECandidate,
               void(const blink::WebString& host_candidate,
                    const blink::WebString& url,
                    int error_code,
                    const blink::WebString& error_text));
  MOCK_METHOD1(DidChangeSignalingState,
               void(webrtc::PeerConnectionInterface::SignalingState state));
  MOCK_METHOD1(DidChangeIceGatheringState,
               void(webrtc::PeerConnectionInterface::IceGatheringState state));
  MOCK_METHOD1(DidChangeIceConnectionState,
               void(webrtc::PeerConnectionInterface::IceConnectionState state));
  MOCK_METHOD1(
      DidChangePeerConnectionState,
      void(webrtc::PeerConnectionInterface::PeerConnectionState state));
  void DidAddReceiverPlanB(
      std::unique_ptr<RTCRtpReceiverPlatform> web_rtp_receiver) override {
    DidAddReceiverPlanBForMock(&web_rtp_receiver);
  }
  void DidRemoveReceiverPlanB(
      std::unique_ptr<RTCRtpReceiverPlatform> web_rtp_receiver) override {
    DidRemoveReceiverPlanBForMock(&web_rtp_receiver);
  }
  MOCK_METHOD1(DidModifySctpTransport,
               void(blink::WebRTCSctpTransportSnapshot snapshot));
  void DidModifyTransceivers(
      blink::WebVector<std::unique_ptr<RTCRtpTransceiverPlatform>>
          platform_transceivers,
      WebVector<uintptr_t> removed_transceivers,
      bool is_remote_description) override {
    DidModifyTransceiversForMock(&platform_transceivers, is_remote_description);
  }
  MOCK_METHOD1(DidAddRemoteDataChannel,
               void(scoped_refptr<webrtc::DataChannelInterface>));
  MOCK_METHOD1(DidNoteInterestingUsage, void(int));
  MOCK_METHOD0(ReleasePeerConnectionHandler, void());

  // Move-only arguments do not play nicely with MOCK, the workaround is to
  // EXPECT_CALL with these instead.
  MOCK_METHOD1(DidAddReceiverPlanBForMock,
               void(std::unique_ptr<RTCRtpReceiverPlatform>*));
  MOCK_METHOD1(DidRemoveReceiverPlanBForMock,
               void(std::unique_ptr<RTCRtpReceiverPlatform>*));
  MOCK_METHOD2(
      DidModifyTransceiversForMock,
      void(blink::WebVector<std::unique_ptr<RTCRtpTransceiverPlatform>>*,
           bool));

  void didGenerateICECandidateWorker(
      scoped_refptr<RTCIceCandidatePlatform> candidate);
  void didAddReceiverWorker(
      std::unique_ptr<RTCRtpReceiverPlatform>* stream_web_rtp_receivers);
  void didRemoveReceiverWorker(
      std::unique_ptr<RTCRtpReceiverPlatform>* stream_web_rtp_receivers);

  const std::string& candidate_sdp() const { return candidate_sdp_; }
  const base::Optional<uint16_t>& candidate_mlineindex() const {
    return candidate_mline_index_;
  }
  const std::string& candidate_mid() const { return candidate_mid_; }
  const blink::WebString& remote_stream_id() const { return remote_stream_id_; }

 private:
  blink::WebString remote_stream_id_;
  std::string candidate_sdp_;
  base::Optional<uint16_t> candidate_mline_index_;
  std::string candidate_mid_;

  DISALLOW_COPY_AND_ASSIGN(MockWebRTCPeerConnectionHandlerClient);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_MOCK_WEB_RTC_PEER_CONNECTION_HANDLER_CLIENT_H_
