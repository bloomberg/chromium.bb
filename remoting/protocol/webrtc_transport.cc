// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_transport.h"

#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task_runner_util.h"
#include "base/thread_task_runner_handle.h"
#include "jingle/glue/thread_wrapper.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/port_allocator_factory.h"
#include "remoting/protocol/stream_message_pipe_adapter.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/protocol/webrtc_video_encoder.h"
#include "third_party/webrtc/api/test/fakeconstraints.h"
#include "third_party/webrtc/libjingle/xmllite/xmlelement.h"
#include "third_party/webrtc/modules/audio_device/include/fake_audio_device.h"

using buzz::QName;
using buzz::XmlElement;

namespace remoting {
namespace protocol {

namespace {

// Delay after candidate creation before sending transport-info message to
// accumulate multiple candidates. This is an optimization to reduce number of
// transport-info messages.
const int kTransportInfoSendDelayMs = 20;

// XML namespace for the transport elements.
const char kTransportNamespace[] = "google:remoting:webrtc";

#if !defined(NDEBUG)
// Command line switch used to disable signature verification.
// TODO(sergeyu): Remove this flag.
const char kDisableAuthenticationSwitchName[] = "disable-authentication";
#endif

bool IsValidSessionDescriptionType(const std::string& type) {
  return type == webrtc::SessionDescriptionInterface::kOffer ||
         type == webrtc::SessionDescriptionInterface::kAnswer;
}

// Normalizes the SDP message to make sure the text used for HMAC signatures
// verifications is the same that was signed on the sending side. This is
// necessary because WebRTC generates SDP with CRLF line endings which are
// sometimes converted to LF after passing the signaling channel.
std::string NormalizeSessionDescription(const std::string& sdp) {
  // Split SDP lines. The CR symbols is removed by the TRIM_WHITESPACE flag.
  std::vector<std::string> lines =
      SplitString(sdp, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  return base::JoinString(lines, "\n") + "\n";
}

// A webrtc::CreateSessionDescriptionObserver implementation used to receive the
// results of creating descriptions for this end of the PeerConnection.
class CreateSessionDescriptionObserver
    : public webrtc::CreateSessionDescriptionObserver {
 public:
  typedef base::Callback<void(
      std::unique_ptr<webrtc::SessionDescriptionInterface> description,
      const std::string& error)>
      ResultCallback;

  static CreateSessionDescriptionObserver* Create(
      const ResultCallback& result_callback) {
    return new rtc::RefCountedObject<CreateSessionDescriptionObserver>(
        result_callback);
  }
  void OnSuccess(webrtc::SessionDescriptionInterface* desc) override {
    base::ResetAndReturn(&result_callback_)
        .Run(base::WrapUnique(desc), std::string());
  }
  void OnFailure(const std::string& error) override {
    base::ResetAndReturn(&result_callback_).Run(nullptr, error);
  }

 protected:
  explicit CreateSessionDescriptionObserver(
      const ResultCallback& result_callback)
      : result_callback_(result_callback) {}
  ~CreateSessionDescriptionObserver() override {}

 private:
  ResultCallback result_callback_;

  DISALLOW_COPY_AND_ASSIGN(CreateSessionDescriptionObserver);
};

// A webrtc::SetSessionDescriptionObserver implementation used to receive the
// results of setting local and remote descriptions of the PeerConnection.
class SetSessionDescriptionObserver
    : public webrtc::SetSessionDescriptionObserver {
 public:
  typedef base::Callback<void(bool success, const std::string& error)>
      ResultCallback;

  static SetSessionDescriptionObserver* Create(
      const ResultCallback& result_callback) {
    return new rtc::RefCountedObject<SetSessionDescriptionObserver>(
        result_callback);
  }

  void OnSuccess() override {
    base::ResetAndReturn(&result_callback_).Run(true, std::string());
  }

  void OnFailure(const std::string& error) override {
    base::ResetAndReturn(&result_callback_).Run(false, error);
  }

 protected:
  explicit SetSessionDescriptionObserver(const ResultCallback& result_callback)
      : result_callback_(result_callback) {}
  ~SetSessionDescriptionObserver() override {}

 private:
  ResultCallback result_callback_;

  DISALLOW_COPY_AND_ASSIGN(SetSessionDescriptionObserver);
};


}  // namespace

WebrtcTransport::WebrtcTransport(
    rtc::Thread* worker_thread,
    scoped_refptr<TransportContext> transport_context,
    EventHandler* event_handler)
    : worker_thread_(worker_thread),
      transport_context_(transport_context),
      event_handler_(event_handler),
      handshake_hmac_(crypto::HMAC::SHA256),
      outgoing_data_stream_adapter_(
          true,
          base::Bind(&WebrtcTransport::Close, base::Unretained(this))),
      incoming_data_stream_adapter_(
          false,
          base::Bind(&WebrtcTransport::Close, base::Unretained(this))),
      weak_factory_(this) {
  transport_context_->set_relay_mode(TransportContext::RelayMode::TURN);
}

WebrtcTransport::~WebrtcTransport() {}

void WebrtcTransport::Start(
    Authenticator* authenticator,
    SendTransportInfoCallback send_transport_info_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(send_transport_info_callback_.is_null());

  jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();

  // TODO(sergeyu): Investigate if it's possible to avoid Send().
  jingle_glue::JingleThreadWrapper::current()->set_send_allowed(true);

  send_transport_info_callback_ = std::move(send_transport_info_callback);

  if (!handshake_hmac_.Init(authenticator->GetAuthKey())) {
    LOG(FATAL) << "HMAC::Init() failed.";
  }

  fake_audio_device_module_.reset(new webrtc::FakeAudioDeviceModule());
  video_encoder_factory_ = new remoting::WebRtcVideoEncoderFactory();

  // Takes ownership of video_encoder_factory_
  peer_connection_factory_ = webrtc::CreatePeerConnectionFactory(
      worker_thread_, rtc::Thread::Current(), fake_audio_device_module_.get(),
      video_encoder_factory_, nullptr);

  webrtc::PeerConnectionInterface::IceServer stun_server;
  stun_server.urls.push_back("stun:stun.l.google.com:19302");
  webrtc::PeerConnectionInterface::RTCConfiguration rtc_config;
  rtc_config.servers.push_back(stun_server);

  webrtc::FakeConstraints constraints;
  constraints.AddMandatory(webrtc::MediaConstraintsInterface::kEnableDtlsSrtp,
                           webrtc::MediaConstraintsInterface::kValueTrue);

  std::unique_ptr<cricket::PortAllocator> port_allocator =
      transport_context_->port_allocator_factory()->CreatePortAllocator(
          transport_context_);
  peer_connection_ = peer_connection_factory_->CreatePeerConnection(
      rtc_config, &constraints, std::move(port_allocator), nullptr, this);

  outgoing_data_stream_adapter_.Initialize(peer_connection_);
  incoming_data_stream_adapter_.Initialize(peer_connection_);

  event_handler_->OnWebrtcTransportConnecting();

  if (transport_context_->role() == TransportRole::SERVER)
    RequestNegotiation();
}

bool WebrtcTransport::ProcessTransportInfo(XmlElement* transport_info) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (transport_info->Name() != QName(kTransportNamespace, "transport"))
    return false;

  if (!peer_connection_)
    return false;

  XmlElement* session_description = transport_info->FirstNamed(
      QName(kTransportNamespace, "session-description"));
  if (session_description) {
    webrtc::PeerConnectionInterface::SignalingState expected_state =
        transport_context_->role() == TransportRole::CLIENT
            ? webrtc::PeerConnectionInterface::kStable
            : webrtc::PeerConnectionInterface::kHaveLocalOffer;
    if (peer_connection_->signaling_state() != expected_state) {
      LOG(ERROR) << "Received unexpected WebRTC session_description.";
      return false;
    }

    std::string type = session_description->Attr(QName(std::string(), "type"));
    std::string sdp =
        NormalizeSessionDescription(session_description->BodyText());
    if (!IsValidSessionDescriptionType(type) || sdp.empty()) {
      LOG(ERROR) << "Incorrect session description format.";
      return false;
    }

    std::string signature_base64 =
        session_description->Attr(QName(std::string(), "signature"));
    std::string signature;
    if (!base::Base64Decode(signature_base64, &signature) ||
        !handshake_hmac_.Verify(type + " " + sdp, signature)) {
      LOG(WARNING) << "Received session-description with invalid signature.";
      bool ignore_error = false;
#if !defined(NDEBUG)
      ignore_error = base::CommandLine::ForCurrentProcess()->HasSwitch(
          kDisableAuthenticationSwitchName);
#endif
      if (!ignore_error) {
        Close(AUTHENTICATION_FAILED);
        return true;
      }
    }

    // TODO(isheriff): The need for this should go away once we have a proper
    // API to provide max bitrate for the case of handing over encoded
    // frames to webrtc.
    // Increase max bitrate from 600 kbps to 5 Mbps.
    std::string vp8line = "a=rtpmap:100 VP8/90000\n";
    std::string vp8line_with_bitrate =
        vp8line + "a=fmtp:100 x-google-max-bitrate=5000\n";
    base::ReplaceSubstringsAfterOffset(&sdp, 0, vp8line, vp8line_with_bitrate);

    webrtc::SdpParseError error;
    std::unique_ptr<webrtc::SessionDescriptionInterface> session_description(
        webrtc::CreateSessionDescription(type, sdp, &error));
    if (!session_description) {
      LOG(ERROR) << "Failed to parse the session description: "
                 << error.description << " line: " << error.line;
      return false;
    }

    peer_connection_->SetRemoteDescription(
        SetSessionDescriptionObserver::Create(
            base::Bind(&WebrtcTransport::OnRemoteDescriptionSet,
                       weak_factory_.GetWeakPtr(),
                       type == webrtc::SessionDescriptionInterface::kOffer)),
        session_description.release());
  }

  XmlElement* candidate_element;
  QName candidate_qname(kTransportNamespace, "candidate");
  for (candidate_element = transport_info->FirstNamed(candidate_qname);
       candidate_element;
       candidate_element = candidate_element->NextNamed(candidate_qname)) {
    std::string candidate_str = candidate_element->BodyText();
    std::string sdp_mid =
        candidate_element->Attr(QName(std::string(), "sdpMid"));
    std::string sdp_mlineindex_str =
        candidate_element->Attr(QName(std::string(), "sdpMLineIndex"));
    int sdp_mlineindex;
    if (candidate_str.empty() || sdp_mid.empty() ||
        !base::StringToInt(sdp_mlineindex_str, &sdp_mlineindex)) {
      LOG(ERROR) << "Failed to parse incoming candidates.";
      return false;
    }

    webrtc::SdpParseError error;
    std::unique_ptr<webrtc::IceCandidateInterface> candidate(
        webrtc::CreateIceCandidate(sdp_mid, sdp_mlineindex, candidate_str,
                                   &error));
    if (!candidate) {
      LOG(ERROR) << "Failed to parse incoming candidate: " << error.description
                 << " line: " << error.line;
      return false;
    }

    if (peer_connection_->signaling_state() ==
        webrtc::PeerConnectionInterface::kStable) {
      if (!peer_connection_->AddIceCandidate(candidate.get())) {
        LOG(ERROR) << "Failed to add incoming ICE candidate.";
        return false;
      }
    } else {
      pending_incoming_candidates_.push_back(std::move(candidate));
    }
  }

  return true;
}

void WebrtcTransport::OnLocalSessionDescriptionCreated(
    std::unique_ptr<webrtc::SessionDescriptionInterface> description,
    const std::string& error) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!peer_connection_)
    return;

  if (!description) {
    LOG(ERROR) << "PeerConnection offer creation failed: " << error;
    Close(CHANNEL_CONNECTION_ERROR);
    return;
  }

  std::string description_sdp;
  if (!description->ToString(&description_sdp)) {
    LOG(ERROR) << "Failed to serialize description.";
    Close(CHANNEL_CONNECTION_ERROR);
    return;
  }
  description_sdp = NormalizeSessionDescription(description_sdp);

  // Format and send the session description to the peer.
  std::unique_ptr<XmlElement> transport_info(
      new XmlElement(QName(kTransportNamespace, "transport"), true));
  XmlElement* offer_tag =
      new XmlElement(QName(kTransportNamespace, "session-description"));
  transport_info->AddElement(offer_tag);
  offer_tag->SetAttr(QName(std::string(), "type"), description->type());
  offer_tag->SetBodyText(description_sdp);

  std::string digest;
  digest.resize(handshake_hmac_.DigestLength());
  CHECK(handshake_hmac_.Sign(description->type() + " " + description_sdp,
                             reinterpret_cast<uint8_t*>(&(digest[0])),
                             digest.size()));
  std::string digest_base64;
  base::Base64Encode(digest, &digest_base64);
  offer_tag->SetAttr(QName(std::string(), "signature"), digest_base64);

  send_transport_info_callback_.Run(std::move(transport_info));

  peer_connection_->SetLocalDescription(
      SetSessionDescriptionObserver::Create(base::Bind(
          &WebrtcTransport::OnLocalDescriptionSet, weak_factory_.GetWeakPtr())),
      description.release());
}

void WebrtcTransport::OnLocalDescriptionSet(bool success,
                                            const std::string& error) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!peer_connection_)
    return;

  if (!success) {
    LOG(ERROR) << "Failed to set local description: " << error;
    Close(CHANNEL_CONNECTION_ERROR);
    return;
  }

  AddPendingCandidatesIfPossible();
}

void WebrtcTransport::OnRemoteDescriptionSet(bool send_answer,
                                             bool success,
                                             const std::string& error) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!peer_connection_)
    return;

  if (!success) {
    LOG(ERROR) << "Failed to set local description: " << error;
    Close(CHANNEL_CONNECTION_ERROR);
    return;
  }

  // Create and send answer on the server.
  if (send_answer) {
    peer_connection_->CreateAnswer(
        CreateSessionDescriptionObserver::Create(
            base::Bind(&WebrtcTransport::OnLocalSessionDescriptionCreated,
                       weak_factory_.GetWeakPtr())),
        nullptr);
  }

  AddPendingCandidatesIfPossible();
}

void WebrtcTransport::OnSignalingChange(
    webrtc::PeerConnectionInterface::SignalingState new_state) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void WebrtcTransport::OnAddStream(webrtc::MediaStreamInterface* stream) {
  DCHECK(thread_checker_.CalledOnValidThread());
  event_handler_->OnWebrtcTransportMediaStreamAdded(stream);
}

void WebrtcTransport::OnRemoveStream(webrtc::MediaStreamInterface* stream) {
  DCHECK(thread_checker_.CalledOnValidThread());
  event_handler_->OnWebrtcTransportMediaStreamRemoved(stream);
}

void WebrtcTransport::OnDataChannel(
    webrtc::DataChannelInterface* data_channel) {
  DCHECK(thread_checker_.CalledOnValidThread());
  incoming_data_stream_adapter_.OnIncomingDataChannel(data_channel);
}

void WebrtcTransport::OnRenegotiationNeeded() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (transport_context_->role() == TransportRole::SERVER) {
    RequestNegotiation();
  } else {
    // TODO(sergeyu): Is it necessary to support renegotiation initiated by the
    // client?
    NOTIMPLEMENTED();
  }
}

void WebrtcTransport::RequestNegotiation() {
  DCHECK(transport_context_->role() == TransportRole::SERVER);

  if (!negotiation_pending_) {
    negotiation_pending_ = true;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&WebrtcTransport::SendOffer, weak_factory_.GetWeakPtr()));
  }
}

void WebrtcTransport::OnIceConnectionChange(
    webrtc::PeerConnectionInterface::IceConnectionState new_state) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!connected_ &&
      new_state == webrtc::PeerConnectionInterface::kIceConnectionConnected) {
    connected_ = true;
    event_handler_->OnWebrtcTransportConnected();
  }
}

void WebrtcTransport::OnIceGatheringChange(
    webrtc::PeerConnectionInterface::IceGatheringState new_state) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void WebrtcTransport::OnIceCandidate(
    const webrtc::IceCandidateInterface* candidate) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::unique_ptr<XmlElement> candidate_element(
      new XmlElement(QName(kTransportNamespace, "candidate")));
  std::string candidate_str;
  if (!candidate->ToString(&candidate_str)) {
    LOG(ERROR) << "Failed to serialize local candidate.";
    return;
  }
  candidate_element->SetBodyText(candidate_str);
  candidate_element->SetAttr(QName(std::string(), "sdpMid"),
                             candidate->sdp_mid());
  candidate_element->SetAttr(QName(std::string(), "sdpMLineIndex"),
                             base::IntToString(candidate->sdp_mline_index()));

  EnsurePendingTransportInfoMessage();
  pending_transport_info_message_->AddElement(candidate_element.release());
}

void WebrtcTransport::EnsurePendingTransportInfoMessage() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // |transport_info_timer_| must be running iff
  // |pending_transport_info_message_| exists.
  DCHECK_EQ(pending_transport_info_message_ != nullptr,
            transport_info_timer_.IsRunning());

  if (!pending_transport_info_message_) {
    pending_transport_info_message_.reset(
        new XmlElement(QName(kTransportNamespace, "transport"), true));

    // Delay sending the new candidates in case we get more candidates
    // that we can send in one message.
    transport_info_timer_.Start(
        FROM_HERE, base::TimeDelta::FromMilliseconds(kTransportInfoSendDelayMs),
        this, &WebrtcTransport::SendTransportInfo);
  }
}

void WebrtcTransport::SendOffer() {
  DCHECK(transport_context_->role() == TransportRole::SERVER);

  DCHECK(negotiation_pending_);
  negotiation_pending_ = false;

  webrtc::FakeConstraints offer_config;
  offer_config.AddMandatory(
      webrtc::MediaConstraintsInterface::kOfferToReceiveVideo,
      webrtc::MediaConstraintsInterface::kValueTrue);
  offer_config.AddMandatory(
      webrtc::MediaConstraintsInterface::kOfferToReceiveAudio,
      webrtc::MediaConstraintsInterface::kValueFalse);
  offer_config.AddMandatory(webrtc::MediaConstraintsInterface::kEnableDtlsSrtp,
                            webrtc::MediaConstraintsInterface::kValueTrue);
  peer_connection_->CreateOffer(
      CreateSessionDescriptionObserver::Create(
          base::Bind(&WebrtcTransport::OnLocalSessionDescriptionCreated,
                     weak_factory_.GetWeakPtr())),
      &offer_config);
}

void WebrtcTransport::SendTransportInfo() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(pending_transport_info_message_);

  send_transport_info_callback_.Run(std::move(pending_transport_info_message_));
}

void WebrtcTransport::AddPendingCandidatesIfPossible() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (peer_connection_->signaling_state() ==
      webrtc::PeerConnectionInterface::kStable) {
    for (auto candidate : pending_incoming_candidates_) {
      if (!peer_connection_->AddIceCandidate(candidate)) {
        LOG(ERROR) << "Failed to add incoming candidate";
        Close(INCOMPATIBLE_PROTOCOL);
        return;
      }
    }
    pending_incoming_candidates_.clear();
  }
}

void WebrtcTransport::Close(ErrorCode error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!peer_connection_)
    return;

  weak_factory_.InvalidateWeakPtrs();
  peer_connection_->Close();
  peer_connection_ = nullptr;
  peer_connection_factory_ = nullptr;

  if (error != OK)
    event_handler_->OnWebrtcTransportError(error);
}

}  // namespace protocol
}  // namespace remoting
