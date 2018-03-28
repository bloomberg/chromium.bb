// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/testing/TestingPlatformSupportWithWebRTC.h"

#include <memory>
#include "public/platform/WebMediaStreamTrack.h"
#include "public/platform/WebRTCDTMFSenderHandler.h"
#include "public/platform/WebRTCError.h"
#include "public/platform/WebRTCRtpReceiver.h"
#include "public/platform/WebRTCRtpSender.h"
#include "public/platform/WebRTCSessionDescription.h"
#include "public/platform/WebVector.h"

namespace blink {

namespace {

class DummyWebRTCRtpSender : public WebRTCRtpSender {
 private:
  static uintptr_t last_id_;

 public:
  DummyWebRTCRtpSender() : id_(++last_id_) {}
  ~DummyWebRTCRtpSender() override {}

  uintptr_t Id() const override { return id_; }
  WebMediaStreamTrack Track() const override { return WebMediaStreamTrack(); }
  void ReplaceTrack(WebMediaStreamTrack, WebRTCVoidRequest) override {}
  std::unique_ptr<WebRTCDTMFSenderHandler> GetDtmfSender() const override {
    return nullptr;
  }
  std::unique_ptr<WebRTCRtpParameters> GetParameters() const override {
    return std::unique_ptr<WebRTCRtpParameters>();
  }
  void GetStats(std::unique_ptr<blink::WebRTCStatsReportCallback>) override {}

 private:
  const uintptr_t id_;
};

uintptr_t DummyWebRTCRtpSender::last_id_ = 0;

}  // namespace

MockWebRTCPeerConnectionHandler::MockWebRTCPeerConnectionHandler() = default;

MockWebRTCPeerConnectionHandler::~MockWebRTCPeerConnectionHandler() = default;

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

void MockWebRTCPeerConnectionHandler::GetStats(const WebRTCStatsRequest&) {}

void MockWebRTCPeerConnectionHandler::GetStats(
    std::unique_ptr<WebRTCStatsReportCallback>) {}

std::unique_ptr<WebRTCRtpSender> MockWebRTCPeerConnectionHandler::AddTrack(
    const WebMediaStreamTrack&,
    const WebVector<WebMediaStream>&) {
  return std::make_unique<DummyWebRTCRtpSender>();
}

bool MockWebRTCPeerConnectionHandler::RemoveTrack(WebRTCRtpSender*) {
  return true;
}

WebRTCDataChannelHandler* MockWebRTCPeerConnectionHandler::CreateDataChannel(
    const WebString& label,
    const WebRTCDataChannelInit&) {
  return nullptr;
}

void MockWebRTCPeerConnectionHandler::Stop() {}

WebString MockWebRTCPeerConnectionHandler::Id() const {
  return WebString();
}

std::unique_ptr<WebRTCPeerConnectionHandler>
TestingPlatformSupportWithWebRTC::CreateRTCPeerConnectionHandler(
    WebRTCPeerConnectionHandlerClient*,
    scoped_refptr<base::SingleThreadTaskRunner>) {
  return std::make_unique<MockWebRTCPeerConnectionHandler>();
}

}  // namespace blink
