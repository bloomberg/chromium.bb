// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_web_rtc.h"

#include <utility>

#include "third_party/blink/public/platform/web_media_stream.h"
#include "third_party/blink/public/platform/web_media_stream_source.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/public/platform/web_rtc_dtmf_sender_handler.h"
#include "third_party/blink/public/platform/web_rtc_rtp_source.h"
#include "third_party/blink/public/platform/web_rtc_rtp_transceiver.h"
#include "third_party/blink/public/platform/web_rtc_session_description.h"
#include "third_party/blink/public/platform/web_rtc_stats.h"
#include "third_party/blink/public/platform/web_vector.h"

namespace blink {

namespace {

// Having a refcounted helper class allows multiple DummyWebRTCRtpSender to
// share the same internal states.
class DummyRtpSenderInternal
    : public base::RefCountedThreadSafe<DummyRtpSenderInternal> {
 private:
  static uintptr_t last_id_;

 public:
  explicit DummyRtpSenderInternal(WebMediaStreamTrack track)
      : id_(++last_id_), track_(std::move(track)) {}

  uintptr_t id() const { return id_; }
  WebMediaStreamTrack track() const { return track_; }
  void set_track(WebMediaStreamTrack track) { track_ = std::move(track); }

 private:
  const uintptr_t id_;
  WebMediaStreamTrack track_;
};

uintptr_t DummyRtpSenderInternal::last_id_ = 0;

class DummyWebRTCRtpSender : public WebRTCRtpSender {
 public:
  explicit DummyWebRTCRtpSender(WebMediaStreamTrack track)
      : internal_(new DummyRtpSenderInternal(std::move(track))) {}
  DummyWebRTCRtpSender(const DummyWebRTCRtpSender& other)
      : internal_(other.internal_) {}
  ~DummyWebRTCRtpSender() override {}

  scoped_refptr<DummyRtpSenderInternal> internal() const { return internal_; }

  std::unique_ptr<WebRTCRtpSender> ShallowCopy() const override {
    return nullptr;
  }
  uintptr_t Id() const override { return internal_->id(); }
  rtc::scoped_refptr<webrtc::DtlsTransportInterface> DtlsTransport() override {
    return nullptr;
  }
  webrtc::DtlsTransportInformation DtlsTransportInformation() override {
    static webrtc::DtlsTransportInformation dummy(
        webrtc::DtlsTransportState::kNew);
    return dummy;
  }
  WebMediaStreamTrack Track() const override { return internal_->track(); }
  WebVector<WebString> StreamIds() const override {
    return std::vector<WebString>({WebString::FromUTF8("DummyStringId")});
  }
  void ReplaceTrack(WebMediaStreamTrack, WebRTCVoidRequest) override {}
  std::unique_ptr<WebRTCDTMFSenderHandler> GetDtmfSender() const override {
    return nullptr;
  }
  std::unique_ptr<webrtc::RtpParameters> GetParameters() const override {
    return std::unique_ptr<webrtc::RtpParameters>();
  }
  void SetParameters(blink::WebVector<webrtc::RtpEncodingParameters>,
                     webrtc::DegradationPreference,
                     WebRTCVoidRequest) override {}
  void GetStats(std::unique_ptr<blink::WebRTCStatsReportCallback>,
                blink::RTCStatsFilter) override {}

 private:
  scoped_refptr<DummyRtpSenderInternal> internal_;
};

class DummyWebRTCRtpReceiver : public WebRTCRtpReceiver {
 private:
  static uintptr_t last_id_;

 public:
  explicit DummyWebRTCRtpReceiver(WebMediaStreamSource::Type type)
      : id_(++last_id_), track_() {
    if (type == WebMediaStreamSource::Type::kTypeAudio) {
      WebMediaStreamSource web_source;
      web_source.Initialize(WebString::FromUTF8("remoteAudioId"),
                            WebMediaStreamSource::Type::kTypeAudio,
                            WebString::FromUTF8("remoteAudioName"),
                            true /* remote */);
      track_.Initialize(web_source.Id(), web_source);
    } else {
      DCHECK_EQ(type, WebMediaStreamSource::Type::kTypeVideo);
      WebMediaStreamSource web_source;
      web_source.Initialize(WebString::FromUTF8("remoteVideoId"),
                            WebMediaStreamSource::Type::kTypeVideo,
                            WebString::FromUTF8("remoteVideoName"),
                            true /* remote */);
      track_.Initialize(web_source.Id(), web_source);
    }
  }
  DummyWebRTCRtpReceiver(const DummyWebRTCRtpReceiver& other)
      : id_(other.id_), track_(other.track_) {}
  ~DummyWebRTCRtpReceiver() override {}

  std::unique_ptr<WebRTCRtpReceiver> ShallowCopy() const override {
    return nullptr;
  }
  uintptr_t Id() const override { return id_; }
  rtc::scoped_refptr<webrtc::DtlsTransportInterface> DtlsTransport() override {
    return nullptr;
  }
  webrtc::DtlsTransportInformation DtlsTransportInformation() override {
    static webrtc::DtlsTransportInformation dummy(
        webrtc::DtlsTransportState::kNew);
    return dummy;
  }
  const WebMediaStreamTrack& Track() const override { return track_; }
  WebVector<WebString> StreamIds() const override {
    return WebVector<WebString>();
  }
  WebVector<std::unique_ptr<WebRTCRtpSource>> GetSources() override {
    return WebVector<std::unique_ptr<WebRTCRtpSource>>();
  }
  void GetStats(std::unique_ptr<blink::WebRTCStatsReportCallback>,
                RTCStatsFilter) override {}
  std::unique_ptr<webrtc::RtpParameters> GetParameters() const override {
    return nullptr;
  }

 private:
  const uintptr_t id_;
  WebMediaStreamTrack track_;
};

uintptr_t DummyWebRTCRtpReceiver::last_id_ = 0;

// Having a refcounted helper class allows multiple DummyWebRTCRtpTransceivers
// to share the same internal states.
class DummyTransceiverInternal
    : public base::RefCountedThreadSafe<DummyTransceiverInternal> {
 private:
  static uintptr_t last_id_;

 public:
  DummyTransceiverInternal(WebMediaStreamSource::Type type,
                           WebMediaStreamTrack sender_track)
      : id_(++last_id_),
        sender_(std::move(sender_track)),
        receiver_(type),
        direction_(webrtc::RtpTransceiverDirection::kSendRecv) {
    DCHECK(sender_.Track().IsNull() ||
           sender_.Track().Source().GetType() == type);
  }

  uintptr_t id() const { return id_; }
  DummyWebRTCRtpSender* sender() { return &sender_; }
  std::unique_ptr<DummyWebRTCRtpSender> Sender() const {
    return std::make_unique<DummyWebRTCRtpSender>(sender_);
  }
  DummyWebRTCRtpReceiver* receiver() { return &receiver_; }
  std::unique_ptr<DummyWebRTCRtpReceiver> Receiver() const {
    return std::make_unique<DummyWebRTCRtpReceiver>(receiver_);
  }
  webrtc::RtpTransceiverDirection direction() const { return direction_; }
  void set_direction(webrtc::RtpTransceiverDirection direction) {
    direction_ = direction;
  }

 private:
  const uintptr_t id_;
  DummyWebRTCRtpSender sender_;
  DummyWebRTCRtpReceiver receiver_;
  webrtc::RtpTransceiverDirection direction_;
};

uintptr_t DummyTransceiverInternal::last_id_ = 0;

}  // namespace

class MockWebRTCPeerConnectionHandler::DummyWebRTCRtpTransceiver
    : public WebRTCRtpTransceiver {
 public:
  DummyWebRTCRtpTransceiver(WebMediaStreamSource::Type type,
                            WebMediaStreamTrack track)
      : internal_(new DummyTransceiverInternal(type, track)) {}
  DummyWebRTCRtpTransceiver(const DummyWebRTCRtpTransceiver& other)
      : internal_(other.internal_) {}
  ~DummyWebRTCRtpTransceiver() override {}

  scoped_refptr<DummyTransceiverInternal> internal() const { return internal_; }

  WebRTCRtpTransceiverImplementationType ImplementationType() const override {
    return WebRTCRtpTransceiverImplementationType::kFullTransceiver;
  }
  uintptr_t Id() const override { return internal_->id(); }
  WebString Mid() const override { return WebString(); }
  std::unique_ptr<WebRTCRtpSender> Sender() const override {
    return internal_->Sender();
  }
  std::unique_ptr<WebRTCRtpReceiver> Receiver() const override {
    return internal_->Receiver();
  }
  bool Stopped() const override { return true; }
  webrtc::RtpTransceiverDirection Direction() const override {
    return internal_->direction();
  }
  void SetDirection(webrtc::RtpTransceiverDirection direction) override {
    internal_->set_direction(direction);
  }
  base::Optional<webrtc::RtpTransceiverDirection> CurrentDirection()
      const override {
    return base::nullopt;
  }
  base::Optional<webrtc::RtpTransceiverDirection> FiredDirection()
      const override {
    return base::nullopt;
  }

 private:
  scoped_refptr<DummyTransceiverInternal> internal_;
};

MockWebRTCPeerConnectionHandler::MockWebRTCPeerConnectionHandler() = default;

MockWebRTCPeerConnectionHandler::~MockWebRTCPeerConnectionHandler() = default;

bool MockWebRTCPeerConnectionHandler::Initialize(
    const webrtc::PeerConnectionInterface::RTCConfiguration&,
    const WebMediaConstraints&) {
  return true;
}

std::vector<std::unique_ptr<WebRTCRtpTransceiver>>
MockWebRTCPeerConnectionHandler::CreateOffer(
    const WebRTCSessionDescriptionRequest&,
    const WebMediaConstraints&) {
  return {};
}

std::vector<std::unique_ptr<WebRTCRtpTransceiver>>
MockWebRTCPeerConnectionHandler::CreateOffer(
    const WebRTCSessionDescriptionRequest&,
    const WebRTCOfferOptions&) {
  return {};
}

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

WebRTCSessionDescription
MockWebRTCPeerConnectionHandler::CurrentLocalDescription() {
  return WebRTCSessionDescription();
}

WebRTCSessionDescription
MockWebRTCPeerConnectionHandler::CurrentRemoteDescription() {
  return WebRTCSessionDescription();
}

WebRTCSessionDescription
MockWebRTCPeerConnectionHandler::PendingLocalDescription() {
  return WebRTCSessionDescription();
}

WebRTCSessionDescription
MockWebRTCPeerConnectionHandler::PendingRemoteDescription() {
  return WebRTCSessionDescription();
}

const webrtc::PeerConnectionInterface::RTCConfiguration&
MockWebRTCPeerConnectionHandler::GetConfiguration() const {
  static webrtc::PeerConnectionInterface::RTCConfiguration configuration;
  return configuration;
}

webrtc::RTCErrorType MockWebRTCPeerConnectionHandler::SetConfiguration(
    const webrtc::PeerConnectionInterface::RTCConfiguration&) {
  return webrtc::RTCErrorType::NONE;
}

void MockWebRTCPeerConnectionHandler::GetStats(const WebRTCStatsRequest&) {}

void MockWebRTCPeerConnectionHandler::GetStats(
    std::unique_ptr<WebRTCStatsReportCallback>,
    blink::RTCStatsFilter) {}

webrtc::RTCErrorOr<std::unique_ptr<WebRTCRtpTransceiver>>
MockWebRTCPeerConnectionHandler::AddTransceiverWithTrack(
    const WebMediaStreamTrack& track,
    const webrtc::RtpTransceiverInit&) {
  transceivers_.push_back(std::unique_ptr<DummyWebRTCRtpTransceiver>(
      new DummyWebRTCRtpTransceiver(track.Source().GetType(), track)));
  std::unique_ptr<DummyWebRTCRtpTransceiver> copy(
      new DummyWebRTCRtpTransceiver(*transceivers_.back()));
  return std::unique_ptr<WebRTCRtpTransceiver>(std::move(copy));
}

webrtc::RTCErrorOr<std::unique_ptr<WebRTCRtpTransceiver>>
MockWebRTCPeerConnectionHandler::AddTransceiverWithKind(
    std::string kind,
    const webrtc::RtpTransceiverInit&) {
  transceivers_.push_back(
      std::unique_ptr<DummyWebRTCRtpTransceiver>(new DummyWebRTCRtpTransceiver(
          kind == "audio" ? WebMediaStreamSource::Type::kTypeAudio
                          : WebMediaStreamSource::Type::kTypeVideo,
          WebMediaStreamTrack())));
  std::unique_ptr<DummyWebRTCRtpTransceiver> copy(
      new DummyWebRTCRtpTransceiver(*transceivers_.back()));
  return std::unique_ptr<WebRTCRtpTransceiver>(std::move(copy));
}

webrtc::RTCErrorOr<std::unique_ptr<WebRTCRtpTransceiver>>
MockWebRTCPeerConnectionHandler::AddTrack(const WebMediaStreamTrack& track,
                                          const WebVector<WebMediaStream>&) {
  transceivers_.push_back(std::unique_ptr<DummyWebRTCRtpTransceiver>(
      new DummyWebRTCRtpTransceiver(track.Source().GetType(), track)));
  std::unique_ptr<DummyWebRTCRtpTransceiver> copy(
      new DummyWebRTCRtpTransceiver(*transceivers_.back()));
  return std::unique_ptr<WebRTCRtpTransceiver>(std::move(copy));
}

webrtc::RTCErrorOr<std::unique_ptr<WebRTCRtpTransceiver>>
MockWebRTCPeerConnectionHandler::RemoveTrack(WebRTCRtpSender* sender) {
  const DummyWebRTCRtpTransceiver* transceiver_of_sender = nullptr;
  for (const auto& transceiver : transceivers_) {
    if (transceiver->Sender()->Id() == sender->Id()) {
      transceiver_of_sender = transceiver.get();
      break;
    }
  }
  transceiver_of_sender->internal()->sender()->internal()->set_track(
      WebMediaStreamTrack());
  std::unique_ptr<DummyWebRTCRtpTransceiver> copy(
      new DummyWebRTCRtpTransceiver(*transceiver_of_sender));
  return std::unique_ptr<WebRTCRtpTransceiver>(std::move(copy));
}

scoped_refptr<webrtc::DataChannelInterface>
MockWebRTCPeerConnectionHandler::CreateDataChannel(
    const WebString& label,
    const WebRTCDataChannelInit&) {
  return nullptr;
}

void MockWebRTCPeerConnectionHandler::Stop() {}

webrtc::PeerConnectionInterface*
MockWebRTCPeerConnectionHandler::NativePeerConnection() {
  return nullptr;
}

void MockWebRTCPeerConnectionHandler::
    RunSynchronousOnceClosureOnSignalingThread(base::OnceClosure closure,
                                               const char* trace_event_name) {}

void MockWebRTCPeerConnectionHandler::
    RunSynchronousRepeatingClosureOnSignalingThread(
        const base::RepeatingClosure& closure,
        const char* trace_event_name) {}
std::unique_ptr<WebRTCPeerConnectionHandler>
TestingPlatformSupportWithWebRTC::CreateRTCPeerConnectionHandler(
    WebRTCPeerConnectionHandlerClient*,
    scoped_refptr<base::SingleThreadTaskRunner>) {
  return std::make_unique<MockWebRTCPeerConnectionHandler>();
}

}  // namespace blink
