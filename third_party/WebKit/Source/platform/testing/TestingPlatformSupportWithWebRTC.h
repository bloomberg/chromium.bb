// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TestingPlatformSupportWithWebRTC_h
#define TestingPlatformSupportWithWebRTC_h

#include "platform/testing/TestingPlatformSupport.h"
#include "public/platform/WebRTCPeerConnectionHandler.h"

namespace blink {

class MockWebRTCPeerConnectionHandler : public WebRTCPeerConnectionHandler {
 public:
  MockWebRTCPeerConnectionHandler();
  ~MockWebRTCPeerConnectionHandler() override;

  bool Initialize(const WebRTCConfiguration&,
                  const WebMediaConstraints&) override;

  void CreateOffer(const WebRTCSessionDescriptionRequest&,
                   const WebMediaConstraints&) override;
  void CreateOffer(const WebRTCSessionDescriptionRequest&,
                   const WebRTCOfferOptions&) override;
  void CreateAnswer(const WebRTCSessionDescriptionRequest&,
                    const WebMediaConstraints&) override;
  void CreateAnswer(const WebRTCSessionDescriptionRequest&,
                    const WebRTCAnswerOptions&) override;
  void SetLocalDescription(const WebRTCVoidRequest&,
                           const WebRTCSessionDescription&) override;
  void SetRemoteDescription(const WebRTCVoidRequest&,
                            const WebRTCSessionDescription&) override;
  WebRTCSessionDescription LocalDescription() override;
  WebRTCSessionDescription RemoteDescription() override;
  WebRTCErrorType SetConfiguration(const WebRTCConfiguration&) override;
  bool AddStream(const WebMediaStream&, const WebMediaConstraints&) override;
  void RemoveStream(const WebMediaStream&) override;
  void GetStats(const WebRTCStatsRequest&) override;
  void GetStats(std::unique_ptr<WebRTCStatsReportCallback>) override;
  WebVector<std::unique_ptr<WebRTCRtpSender>> GetSenders() override;
  WebVector<std::unique_ptr<WebRTCRtpReceiver>> GetReceivers() override;
  std::unique_ptr<WebRTCRtpSender> AddTrack(
      const WebMediaStreamTrack&,
      const WebVector<WebMediaStream>&) override;
  bool RemoveTrack(WebRTCRtpSender*) override;
  WebRTCDataChannelHandler* CreateDataChannel(
      const WebString& label,
      const WebRTCDataChannelInit&) override;
  WebRTCDTMFSenderHandler* CreateDTMFSender(
      const WebMediaStreamTrack&) override;
  void Stop() override;
};

class TestingPlatformSupportWithWebRTC : public TestingPlatformSupport {
 public:
  std::unique_ptr<WebRTCPeerConnectionHandler> CreateRTCPeerConnectionHandler(
      WebRTCPeerConnectionHandlerClient*) override;
};

}  // namespace blink

#endif  // TestingPlatformSupportWithWebRTC_h
