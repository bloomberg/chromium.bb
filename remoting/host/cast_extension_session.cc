// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/cast_extension_session.h"

#include <utility>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/thread_task_runner_handle.h"
#include "remoting/host/client_session.h"
#include "remoting/proto/control.pb.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/port_allocator_factory.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/protocol/webrtc_video_capturer_adapter.h"
#include "third_party/webrtc/api/mediastreaminterface.h"
#include "third_party/webrtc/api/test/fakeconstraints.h"
#include "third_party/webrtc/api/videosourceinterface.h"

namespace remoting {

// Used as the type attribute of all Cast protocol::ExtensionMessages.
const char kExtensionMessageType[] = "cast_message";

// Top-level keys used in all extension messages between host and client.
// Must keep synced with webapp.
const char kTopLevelData[] = "chromoting_data";
const char kTopLevelSubject[] = "subject";

// Keys used to describe the subject of a cast extension message. WebRTC-related
// message subjects are prepended with "webrtc_".
// Must keep synced with webapp.
const char kSubjectReady[] = "ready";
const char kSubjectTest[] = "test";
const char kSubjectNewCandidate[] = "webrtc_candidate";
const char kSubjectOffer[] = "webrtc_offer";
const char kSubjectAnswer[] = "webrtc_answer";

// WebRTC headers used inside messages with subject = "webrtc_*".
const char kWebRtcCandidate[] = "candidate";
const char kWebRtcSessionDescType[] = "type";
const char kWebRtcSessionDescSDP[] = "sdp";
const char kWebRtcSDPMid[] = "sdpMid";
const char kWebRtcSDPMLineIndex[] = "sdpMLineIndex";

// Media labels used over the PeerConnection.
const char kVideoLabel[] = "cast_video_label";
const char kStreamLabel[] = "stream_label";

const char kWorkerThreadName[] = "CastExtensionSessionWorkerThread";

// Interval between each call to PollPeerConnectionStats().
const int kStatsLogIntervalSec = 10;

// Minimum frame rate for video streaming over the PeerConnection in frames per
// second, added as a media constraint when constructing the video source for
// the Peer Connection.
const int kMinFramesPerSecond = 5;

// A webrtc::SetSessionDescriptionObserver implementation used to receive the
// results of setting local and remote descriptions of the PeerConnection.
class CastSetSessionDescriptionObserver
    : public webrtc::SetSessionDescriptionObserver {
 public:
  static CastSetSessionDescriptionObserver* Create() {
    return new rtc::RefCountedObject<CastSetSessionDescriptionObserver>();
  }
  void OnSuccess() override {
    VLOG(1) << "Setting session description succeeded.";
  }
  void OnFailure(const std::string& error) override {
    LOG(ERROR) << "Setting session description failed: " << error;
  }

 protected:
  CastSetSessionDescriptionObserver() {}
  ~CastSetSessionDescriptionObserver() override {}

  DISALLOW_COPY_AND_ASSIGN(CastSetSessionDescriptionObserver);
};

// A webrtc::CreateSessionDescriptionObserver implementation used to receive the
// results of creating descriptions for this end of the PeerConnection.
class CastCreateSessionDescriptionObserver
    : public webrtc::CreateSessionDescriptionObserver {
 public:
  static CastCreateSessionDescriptionObserver* Create(
      CastExtensionSession* session) {
    return new rtc::RefCountedObject<CastCreateSessionDescriptionObserver>(
        session);
  }
  void OnSuccess(webrtc::SessionDescriptionInterface* desc) override {
    if (cast_extension_session_ == nullptr) {
      LOG(ERROR)
          << "No CastExtensionSession. Creating session description succeeded.";
      return;
    }
    cast_extension_session_->OnCreateSessionDescription(desc);
  }
  void OnFailure(const std::string& error) override {
    if (cast_extension_session_ == nullptr) {
      LOG(ERROR)
          << "No CastExtensionSession. Creating session description failed.";
      return;
    }
    cast_extension_session_->OnCreateSessionDescriptionFailure(error);
  }
  void SetCastExtensionSession(CastExtensionSession* cast_extension_session) {
    cast_extension_session_ = cast_extension_session;
  }

 protected:
  explicit CastCreateSessionDescriptionObserver(CastExtensionSession* session)
      : cast_extension_session_(session) {}
  ~CastCreateSessionDescriptionObserver() override {}

 private:
  CastExtensionSession* cast_extension_session_;

  DISALLOW_COPY_AND_ASSIGN(CastCreateSessionDescriptionObserver);
};

// A webrtc::StatsObserver implementation used to receive statistics about the
// current PeerConnection.
class CastStatsObserver : public webrtc::StatsObserver {
 public:
  static CastStatsObserver* Create() {
    return new rtc::RefCountedObject<CastStatsObserver>();
  }

  void OnComplete(const webrtc::StatsReports& reports) override {
    VLOG(1) << "Received " << reports.size() << " new StatsReports.";

    int index = 0;
    for (const auto* report : reports) {
      VLOG(1) << "Report " << index++ << ":";
      for (const auto& v : report->values()) {
        VLOG(1) << "Stat: " << v.second->display_name() << "="
                << v.second->ToString() << ".";
      }
    }
  }

 protected:
  CastStatsObserver() {}
  ~CastStatsObserver() override {}

  DISALLOW_COPY_AND_ASSIGN(CastStatsObserver);
};

// TODO(aiguha): Fix PeerConnnection-related tear down crash caused by premature
// destruction of cricket::CaptureManager (which occurs on releasing
// |peer_conn_factory_|). See crbug.com/403840.
CastExtensionSession::~CastExtensionSession() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  // Explicitly clear |create_session_desc_observer_|'s pointer to |this|,
  // since the CastExtensionSession is destructing. Otherwise,
  // |create_session_desc_observer_| would be left with a dangling pointer.
  create_session_desc_observer_->SetCastExtensionSession(nullptr);

  CleanupPeerConnection();
}

// static
scoped_ptr<CastExtensionSession> CastExtensionSession::Create(
    scoped_refptr<protocol::TransportContext> transport_context,
    ClientSessionControl* client_session_control,
    protocol::ClientStub* client_stub) {
  scoped_ptr<CastExtensionSession> cast_extension_session(
      new CastExtensionSession(transport_context,
                               client_session_control, client_stub));
  if (!cast_extension_session->WrapTasksAndSave() ||
      !cast_extension_session->InitializePeerConnection()) {
    return nullptr;
  }
  return cast_extension_session;
}

void CastExtensionSession::OnCreateSessionDescription(
    webrtc::SessionDescriptionInterface* desc) {
  if (!caller_task_runner_->BelongsToCurrentThread()) {
    caller_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&CastExtensionSession::OnCreateSessionDescription,
                   base::Unretained(this),
                   desc));
    return;
  }

  peer_connection_->SetLocalDescription(
      CastSetSessionDescriptionObserver::Create(), desc);

  base::DictionaryValue json;
  json.SetString(kWebRtcSessionDescType, desc->type());
  std::string subject =
      (desc->type() == "offer") ? kSubjectOffer : kSubjectAnswer;
  std::string desc_str;
  desc->ToString(&desc_str);
  json.SetString(kWebRtcSessionDescSDP, desc_str);
  std::string json_str;
  if (!base::JSONWriter::Write(json, &json_str)) {
    LOG(ERROR) << "Failed to serialize sdp message.";
    return;
  }

  SendMessageToClient(subject.c_str(), json_str);
}

void CastExtensionSession::OnCreateSessionDescriptionFailure(
    const std::string& error) {
  VLOG(1) << "Creating Session Description failed: " << error;
}

// TODO(aiguha): Support the case(s) where we've grabbed the capturer already,
// but another extension reset the video pipeline. We should remove the
// stream from the peer connection here, and then attempt to re-setup the
// peer connection in the OnRenegotiationNeeded() callback.
// See crbug.com/403843.
void CastExtensionSession::OnCreateVideoCapturer(
    scoped_ptr<webrtc::DesktopCapturer>* capturer) {
  if (has_grabbed_capturer_) {
    LOG(ERROR) << "The video pipeline was reset unexpectedly.";
    has_grabbed_capturer_ = false;
    peer_connection_->RemoveStream(stream_.release());
    return;
  }

  if (received_offer_) {
    has_grabbed_capturer_ = true;
    if (SetupVideoStream(std::move(*capturer))) {
      peer_connection_->CreateAnswer(create_session_desc_observer_, nullptr);
    } else {
      has_grabbed_capturer_ = false;
      // Ignore the received offer, since we failed to setup a video stream.
      received_offer_ = false;
    }
    return;
  }
}

bool CastExtensionSession::ModifiesVideoPipeline() const {
  return true;
}

// Returns true if the |message| is a Cast ExtensionMessage, even if
// it was badly formed or a resulting action failed. This is done so that
// the host does not continue to attempt to pass |message| to other
// HostExtensionSessions.
bool CastExtensionSession::OnExtensionMessage(
    ClientSessionControl* client_session_control,
    protocol::ClientStub* client_stub,
    const protocol::ExtensionMessage& message) {
  if (message.type() != kExtensionMessageType) {
    return false;
  }

  scoped_ptr<base::Value> value = base::JSONReader::Read(message.data());
  base::DictionaryValue* client_message;
  if (!(value && value->GetAsDictionary(&client_message))) {
    LOG(ERROR) << "Could not read cast extension message.";
    return true;
  }

  std::string subject;
  if (!client_message->GetString(kTopLevelSubject, &subject)) {
    LOG(ERROR) << "Invalid Cast Extension Message (missing subject header).";
    return true;
  }

  if (subject == kSubjectOffer && !received_offer_) {
    // Reset the video pipeline so we can grab the screen capturer and setup
    // a video stream.
    if (ParseAndSetRemoteDescription(client_message)) {
      received_offer_ = true;
      LOG(INFO) << "About to ResetVideoPipeline.";
      client_session_control_->ResetVideoPipeline();

    }
  } else if (subject == kSubjectAnswer) {
    ParseAndSetRemoteDescription(client_message);
  } else if (subject == kSubjectNewCandidate) {
    ParseAndAddICECandidate(client_message);
  } else {
    VLOG(1) << "Unexpected CastExtension Message: " << message.data();
  }
  return true;
}

// Private methods ------------------------------------------------------------

CastExtensionSession::CastExtensionSession(
    scoped_refptr<protocol::TransportContext> transport_context,
    ClientSessionControl* client_session_control,
    protocol::ClientStub* client_stub)
    : caller_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      transport_context_(transport_context),
      client_session_control_(client_session_control),
      client_stub_(client_stub),
      stats_observer_(CastStatsObserver::Create()),
      received_offer_(false),
      has_grabbed_capturer_(false),
      signaling_thread_wrapper_(nullptr),
      worker_thread_wrapper_(nullptr),
      worker_thread_(kWorkerThreadName) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(client_session_control_);
  DCHECK(client_stub_);

  // The worker thread is created with base::MessageLoop::TYPE_IO because
  // the PeerConnection performs some port allocation operations on this thread
  // that require it. See crbug.com/404013.
  base::Thread::Options options(base::MessageLoop::TYPE_IO, 0);
  worker_thread_.StartWithOptions(options);
  worker_task_runner_ = worker_thread_.task_runner();
}

bool CastExtensionSession::ParseAndSetRemoteDescription(
    base::DictionaryValue* message) {
  DCHECK(peer_connection_.get() != nullptr);

  base::DictionaryValue* message_data;
  if (!message->GetDictionary(kTopLevelData, &message_data)) {
    LOG(ERROR) << "Invalid Cast Extension Message (missing data).";
    return false;
  }

  std::string webrtc_type;
  if (!message_data->GetString(kWebRtcSessionDescType, &webrtc_type)) {
    LOG(ERROR)
        << "Invalid Cast Extension Message (missing webrtc type header).";
    return false;
  }

  std::string sdp;
  if (!message_data->GetString(kWebRtcSessionDescSDP, &sdp)) {
    LOG(ERROR) << "Invalid Cast Extension Message (missing webrtc sdp header).";
    return false;
  }

  webrtc::SdpParseError error;
  webrtc::SessionDescriptionInterface* session_description(
      webrtc::CreateSessionDescription(webrtc_type, sdp, &error));

  if (!session_description) {
    LOG(ERROR) << "Invalid Cast Extension Message (could not parse sdp).";
    VLOG(1) << "SdpParseError was: " << error.description;
    return false;
  }

  peer_connection_->SetRemoteDescription(
      CastSetSessionDescriptionObserver::Create(), session_description);
  return true;
}

bool CastExtensionSession::ParseAndAddICECandidate(
    base::DictionaryValue* message) {
  DCHECK(peer_connection_.get() != nullptr);

  base::DictionaryValue* message_data;
  if (!message->GetDictionary(kTopLevelData, &message_data)) {
    LOG(ERROR) << "Invalid Cast Extension Message (missing data).";
    return false;
  }

  std::string candidate_str;
  std::string sdp_mid;
  int sdp_mlineindex = 0;
  if (!message_data->GetString(kWebRtcSDPMid, &sdp_mid) ||
      !message_data->GetInteger(kWebRtcSDPMLineIndex, &sdp_mlineindex) ||
      !message_data->GetString(kWebRtcCandidate, &candidate_str)) {
    LOG(ERROR) << "Invalid Cast Extension Message (could not parse).";
    return false;
  }

  rtc::scoped_ptr<webrtc::IceCandidateInterface> candidate(
      webrtc::CreateIceCandidate(sdp_mid, sdp_mlineindex, candidate_str,
                                 nullptr));
  if (!candidate.get()) {
    LOG(ERROR)
        << "Invalid Cast Extension Message (could not create candidate).";
    return false;
  }

  if (!peer_connection_->AddIceCandidate(candidate.get())) {
    LOG(ERROR) << "Failed to apply received ICE Candidate to PeerConnection.";
    return false;
  }

  VLOG(1) << "Received and Added ICE Candidate: " << candidate_str;

  return true;
}

bool CastExtensionSession::SendMessageToClient(const std::string& subject,
                                               const std::string& data) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  if (client_stub_ == nullptr) {
    LOG(ERROR) << "No Client Stub. Cannot send message to client.";
    return false;
  }

  base::DictionaryValue message_dict;
  message_dict.SetString(kTopLevelSubject, subject);
  message_dict.SetString(kTopLevelData, data);
  std::string message_json;

  if (!base::JSONWriter::Write(message_dict, &message_json)) {
    LOG(ERROR) << "Failed to serialize JSON message.";
    return false;
  }

  protocol::ExtensionMessage message;
  message.set_type(kExtensionMessageType);
  message.set_data(message_json);
  client_stub_->DeliverHostMessage(message);
  return true;
}

void CastExtensionSession::EnsureTaskAndSetSend(rtc::Thread** ptr,
                                                base::WaitableEvent* event) {
  jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();
  jingle_glue::JingleThreadWrapper::current()->set_send_allowed(true);
  *ptr = jingle_glue::JingleThreadWrapper::current();

  if (event != nullptr) {
    event->Signal();
  }
}

bool CastExtensionSession::WrapTasksAndSave() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  EnsureTaskAndSetSend(&signaling_thread_wrapper_);
  if (signaling_thread_wrapper_ == nullptr)
    return false;

  base::WaitableEvent wrap_worker_thread_event(true, false);
  worker_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&CastExtensionSession::EnsureTaskAndSetSend,
                 base::Unretained(this),
                 &worker_thread_wrapper_,
                 &wrap_worker_thread_event));
  wrap_worker_thread_event.Wait();

  return (worker_thread_wrapper_ != nullptr);
}

bool CastExtensionSession::InitializePeerConnection() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!peer_conn_factory_);
  DCHECK(!peer_connection_);
  DCHECK(worker_thread_wrapper_ != nullptr);
  DCHECK(signaling_thread_wrapper_ != nullptr);

  peer_conn_factory_ = webrtc::CreatePeerConnectionFactory(
      worker_thread_wrapper_, signaling_thread_wrapper_, nullptr, nullptr,
      nullptr);

  if (!peer_conn_factory_.get()) {
    CleanupPeerConnection();
    return false;
  }

  VLOG(1) << "Created PeerConnectionFactory successfully.";

  // DTLS-SRTP is the preferred encryption method. If set to kValueFalse, the
  // peer connection uses SDES. Disabling SDES as well will cause the peer
  // connection to fail to connect.
  // Note: For protection and unprotection of SRTP packets, the libjingle
  // ENABLE_EXTERNAL_AUTH flag must not be set.
  webrtc::FakeConstraints constraints;
  constraints.AddMandatory(webrtc::MediaConstraintsInterface::kEnableDtlsSrtp,
                           webrtc::MediaConstraintsInterface::kValueTrue);

  scoped_ptr<cricket::PortAllocator> port_allocator =
      transport_context_->port_allocator_factory()->CreatePortAllocator(
          transport_context_);

  webrtc::PeerConnectionInterface::RTCConfiguration rtc_config;
  peer_connection_ = peer_conn_factory_->CreatePeerConnection(
      rtc_config, &constraints,
      rtc::scoped_ptr<cricket::PortAllocator>(port_allocator.release()),
      nullptr, this);

  if (!peer_connection_.get()) {
    CleanupPeerConnection();
    return false;
  }

  VLOG(1) << "Created PeerConnection successfully.";

  create_session_desc_observer_ =
      CastCreateSessionDescriptionObserver::Create(this);

  // Send a test message to the client. Then, notify the client to start
  // webrtc offer/answer negotiation.
  if (!SendMessageToClient(kSubjectTest, "Hello, client.") ||
      !SendMessageToClient(kSubjectReady, "Host ready to receive offers.")) {
    LOG(ERROR) << "Failed to send messages to client.";
    return false;
  }

  return true;
}

bool CastExtensionSession::SetupVideoStream(
    scoped_ptr<webrtc::DesktopCapturer> desktop_capturer) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(desktop_capturer);

  if (stream_) {
    VLOG(1) << "Already added MediaStream. Aborting Setup.";
    return false;
  }

  scoped_ptr<protocol::WebrtcVideoCapturerAdapter> video_capturer_adapter(
      new protocol::WebrtcVideoCapturerAdapter(std::move(desktop_capturer)));

  // Set video stream constraints.
  webrtc::FakeConstraints video_constraints;
  video_constraints.AddMandatory(
      webrtc::MediaConstraintsInterface::kMinFrameRate, kMinFramesPerSecond);

  rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track =
      peer_conn_factory_->CreateVideoTrack(
          kVideoLabel,
          peer_conn_factory_->CreateVideoSource(
              video_capturer_adapter.release(), &video_constraints));

  stream_ = peer_conn_factory_->CreateLocalMediaStream(kStreamLabel);

  if (!stream_->AddTrack(video_track) ||
      !peer_connection_->AddStream(stream_)) {
    return false;
  }

  VLOG(1) << "Setup video stream successfully.";

  return true;
}

void CastExtensionSession::PollPeerConnectionStats() {
  if (!connection_active()) {
    VLOG(1) << "Cannot poll stats while PeerConnection is inactive.";
  }
  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> video_track =
      stream_->FindVideoTrack(kVideoLabel);
  peer_connection_->GetStats(
      stats_observer_, video_track.release(),
      webrtc::PeerConnectionInterface::kStatsOutputLevelStandard);
}

void CastExtensionSession::CleanupPeerConnection() {
  peer_connection_->Close();
  peer_connection_ = nullptr;
  stream_ = nullptr;
  peer_conn_factory_ = nullptr;
  worker_thread_.Stop();
}

bool CastExtensionSession::connection_active() const {
  return peer_connection_.get() != nullptr;
}

// webrtc::PeerConnectionObserver implementation -------------------------------
void CastExtensionSession::OnSignalingChange(
    webrtc::PeerConnectionInterface::SignalingState new_state) {
  VLOG(1) << "PeerConnectionObserver: SignalingState changed to:" << new_state;
}

void CastExtensionSession::OnAddStream(webrtc::MediaStreamInterface* stream) {
  VLOG(1) << "PeerConnectionObserver: stream added: " << stream->label();
}

void CastExtensionSession::OnRemoveStream(
    webrtc::MediaStreamInterface* stream) {
  VLOG(1) << "PeerConnectionObserver: stream removed: " << stream->label();
}

void CastExtensionSession::OnDataChannel(
    webrtc::DataChannelInterface* data_channel) {
  VLOG(1) << "PeerConnectionObserver: data channel: " << data_channel->label();
}

void CastExtensionSession::OnRenegotiationNeeded() {
  VLOG(1) << "PeerConnectionObserver: renegotiation needed.";
}

void CastExtensionSession::OnIceConnectionChange(
    webrtc::PeerConnectionInterface::IceConnectionState new_state) {
  VLOG(1) << "PeerConnectionObserver: IceConnectionState changed to: "
          << new_state;

  // TODO(aiguha): Maybe start timer only if enabled by command-line flag or
  // at a particular verbosity level.
  if (!stats_polling_timer_.IsRunning() &&
      new_state == webrtc::PeerConnectionInterface::kIceConnectionConnected) {
    stats_polling_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromSeconds(kStatsLogIntervalSec),
        this,
        &CastExtensionSession::PollPeerConnectionStats);
  }
}

void CastExtensionSession::OnIceGatheringChange(
    webrtc::PeerConnectionInterface::IceGatheringState new_state) {
  VLOG(1) << "PeerConnectionObserver: IceGatheringState changed to: "
          << new_state;
}

void CastExtensionSession::OnIceCandidate(
    const webrtc::IceCandidateInterface* candidate) {
  std::string candidate_str;
  if (!candidate->ToString(&candidate_str)) {
    LOG(ERROR) << "PeerConnectionObserver: failed to serialize candidate.";
    return;
  }
  base::DictionaryValue json;
  json.SetString(kWebRtcSDPMid, candidate->sdp_mid());
  json.SetInteger(kWebRtcSDPMLineIndex, candidate->sdp_mline_index());
  json.SetString(kWebRtcCandidate, candidate_str);
  std::string json_str;
  if (!base::JSONWriter::Write(json, &json_str)) {
    LOG(ERROR) << "Failed to serialize candidate message.";
    return;
  }
  SendMessageToClient(kSubjectNewCandidate, json_str);
}

}  // namespace remoting
