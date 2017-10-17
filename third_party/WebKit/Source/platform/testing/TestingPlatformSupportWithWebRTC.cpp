// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/testing/TestingPlatformSupportWithWebRTC.h"

#include <memory>
#include "public/platform/WebRTCError.h"
#include "public/platform/WebRTCRtpReceiver.h"
#include "public/platform/WebRTCRtpSender.h"
#include "public/platform/WebRTCSessionDescription.h"
#include "public/platform/WebVector.h"

namespace blink {

MockWebRTCPeerConnectionHandler::MockWebRTCPeerConnectionHandler() {}

MockWebRTCPeerConnectionHandler::~MockWebRTCPeerConnectionHandler() {}

bool MockWebRTCPeerConnectionHandler::Initialize(const WebRTCConfiguration&,
                                                 const WebMediaConstraints&) {
  return true;
}

void MockWebRTCPeerConnectionHandler::CreateOffer(
    const WebRTCSessionDescriptionRequest&,
    const WebMediaConstraints&) {}

void MockWebRTCPeerConnectionHandler::CreateOffer(
    const WebRTCSessionDescriptionRequest&,
    const WebRTCOfferOptions&) {}

void MockWebRTCPeerConnectionHandler::CreateAnswer(
    const WebRTCSessionDescriptionRequest&,
    const WebMediaConstraints&) {}

void MockWebRTCPeerConnectionHandler::CreateAnswer(
    const WebRTCSessionDescriptionRequest&,
    const WebRTCAnswerOptions&) {}

void MockWebRTCPeerConnectionHandler::SetLocalDescription(
    const WebRTCVoidRequest&,
    const WebRTCSessionDescription&) {}

void MockWebRTCPeerConnectionHandler::SetRemoteDescription(
    const WebRTCVoidRequest&,
    const WebRTCSessionDescription&) {}

WebRTCSessionDescription MockWebRTCPeerConnectionHandler::LocalDescription() {
  return WebRTCSessionDescription();
}

WebRTCSessionDescription MockWebRTCPeerConnectionHandler::RemoteDescription() {
  return WebRTCSessionDescription();
}

WebRTCErrorType MockWebRTCPeerConnectionHandler::SetConfiguration(
    const WebRTCConfiguration&) {
  return WebRTCErrorType::kNone;
}

bool MockWebRTCPeerConnectionHandler::AddStream(const WebMediaStream&,
                                                const WebMediaConstraints&) {
  return true;
}

void MockWebRTCPeerConnectionHandler::RemoveStream(const WebMediaStream&) {}

void MockWebRTCPeerConnectionHandler::GetStats(const WebRTCStatsRequest&) {}

void MockWebRTCPeerConnectionHandler::GetStats(
    std::unique_ptr<WebRTCStatsReportCallback>) {}

WebVector<std::unique_ptr<WebRTCRtpSender>>
MockWebRTCPeerConnectionHandler::GetSenders() {
  return WebVector<std::unique_ptr<WebRTCRtpSender>>();
}

std::unique_ptr<WebRTCRtpSender> MockWebRTCPeerConnectionHandler::AddTrack(
    const WebMediaStreamTrack&,
    const WebVector<WebMediaStream>&) {
  return nullptr;
}

bool MockWebRTCPeerConnectionHandler::RemoveTrack(WebRTCRtpSender*) {
  return false;
}

WebRTCDataChannelHandler* MockWebRTCPeerConnectionHandler::CreateDataChannel(
    const WebString& label,
    const WebRTCDataChannelInit&) {
  return nullptr;
}

WebRTCDTMFSenderHandler* MockWebRTCPeerConnectionHandler::CreateDTMFSender(
    const WebMediaStreamTrack&) {
  return nullptr;
}

void MockWebRTCPeerConnectionHandler::Stop() {}

std::unique_ptr<WebRTCPeerConnectionHandler>
TestingPlatformSupportWithWebRTC::CreateRTCPeerConnectionHandler(
    WebRTCPeerConnectionHandlerClient*) {
  return std::make_unique<MockWebRTCPeerConnectionHandler>();
}

}  // namespace blink
