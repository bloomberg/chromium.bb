// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/cast_extension_session.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/synchronization/waitable_event.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/host/cast_video_capturer_adapter.h"
#include "remoting/host/chromium_port_allocator_factory.h"
#include "remoting/host/client_session.h"
#include "remoting/proto/control.pb.h"
#include "remoting/protocol/client_stub.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"
#include "third_party/libjingle/source/talk/app/webrtc/test/fakeconstraints.h"
#include "third_party/libjingle/source/talk/app/webrtc/videosourceinterface.h"

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

// Default STUN server used to construct
// webrtc::PeerConnectionInterface::RTCConfiguration for the PeerConnection.
const char kDefaultStunURI[] = "stun:stun.l.google.com:19302";

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
  virtual void OnSuccess() OVERRIDE {
    VLOG(1) << "Setting session description succeeded.";
  }
  virtual void OnFailure(const std::string& error) OVERRIDE {
    LOG(ERROR) << "Setting session description failed: " << error;
  }

 protected:
  CastSetSessionDescriptionObserver() {}
  virtual ~CastSetSessionDescriptionObserver() {}

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
  virtual void OnSuccess(webrtc::SessionDescriptionInterface* desc) OVERRIDE {
    if (cast_extension_session_ == NULL) {
      LOG(ERROR)
          << "No CastExtensionSession. Creating session description succeeded.";
      return;
    }
    cast_extension_session_->OnCreateSessionDescription(desc);
  }
  virtual void OnFailure(const std::string& error) OVERRIDE {
    if (cast_extension_session_ == NULL) {
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
  virtual ~CastCreateSessionDescriptionObserver() {}

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

  virtual void OnComplete(
      const std::vector<webrtc::StatsReport>& reports) OVERRIDE {
    typedef webrtc::StatsReport::Values::iterator ValuesIterator;

    VLOG(1) << "Received " << reports.size() << " new StatsReports.";

    int index;
    std::vector<webrtc::StatsReport>::const_iterator it;
    for (it = reports.begin(), index = 0; it != reports.end(); ++it, ++index) {
      webrtc::StatsReport::Values v = it->values;
      VLOG(1) << "Report " << index << ":";
      for (ValuesIterator vIt = v.begin(); vIt != v.end(); ++vIt) {
        VLOG(1) << "Stat: " << vIt->name << "=" << vIt->value << ".";
      }
    }
  }

 protected:
  CastStatsObserver() {}
  virtual ~CastStatsObserver() {}

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
  create_session_desc_observer_->SetCastExtensionSession(NULL);

  CleanupPeerConnection();
}

// static
scoped_ptr<CastExtensionSession> CastExtensionSession::Create(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter,
    const protocol::NetworkSettings& network_settings,
    ClientSessionControl* client_session_control,
    protocol::ClientStub* client_stub) {
  scoped_ptr<CastExtensionSession> cast_extension_session(
      new CastExtensionSession(caller_task_runner,
                               url_request_context_getter,
                               network_settings,
                               client_session_control,
                               client_stub));
  if (!cast_extension_session->WrapTasksAndSave()) {
    return scoped_ptr<CastExtensionSession>();
  }
  if (!cast_extension_session->InitializePeerConnection()) {
    return scoped_ptr<CastExtensionSession>();
  }
  return cast_extension_session.Pass();
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

  scoped_ptr<base::DictionaryValue> json(new base::DictionaryValue());
  json->SetString(kWebRtcSessionDescType, desc->type());
  std::string subject =
      (desc->type() == "offer") ? kSubjectOffer : kSubjectAnswer;
  std::string desc_str;
  desc->ToString(&desc_str);
  json->SetString(kWebRtcSessionDescSDP, desc_str);
  std::string json_str;
  if (!base::JSONWriter::Write(json.get(), &json_str)) {
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
scoped_ptr<webrtc::DesktopCapturer> CastExtensionSession::OnCreateVideoCapturer(
    scoped_ptr<webrtc::DesktopCapturer> capturer) {
  if (has_grabbed_capturer_) {
    LOG(ERROR) << "The video pipeline was reset unexpectedly.";
    has_grabbed_capturer_ = false;
    peer_connection_->RemoveStream(stream_.release());
    return capturer.Pass();
  }

  if (received_offer_) {
    has_grabbed_capturer_ = true;
    if (SetupVideoStream(capturer.Pass())) {
      peer_connection_->CreateAnswer(create_session_desc_observer_, NULL);
    } else {
      has_grabbed_capturer_ = false;
      // Ignore the received offer, since we failed to setup a video stream.
      received_offer_ = false;
    }
    return scoped_ptr<webrtc::DesktopCapturer>();
  }

  return capturer.Pass();
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

  scoped_ptr<base::Value> value(base::JSONReader::Read(message.data()));
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
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter,
    const protocol::NetworkSettings& network_settings,
    ClientSessionControl* client_session_control,
    protocol::ClientStub* client_stub)
    : caller_task_runner_(caller_task_runner),
      url_request_context_getter_(url_request_context_getter),
      network_settings_(network_settings),
      client_session_control_(client_session_control),
      client_stub_(client_stub),
      stats_observer_(CastStatsObserver::Create()),
      received_offer_(false),
      has_grabbed_capturer_(false),
      signaling_thread_wrapper_(NULL),
      worker_thread_wrapper_(NULL),
      worker_thread_(kWorkerThreadName) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(url_request_context_getter_);
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
  DCHECK(peer_connection_.get() != NULL);

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
  DCHECK(peer_connection_.get() != NULL);

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
      webrtc::CreateIceCandidate(sdp_mid, sdp_mlineindex, candidate_str));
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

  if (client_stub_ == NULL) {
    LOG(ERROR) << "No Client Stub. Cannot send message to client.";
    return false;
  }

  base::DictionaryValue message_dict;
  message_dict.SetString(kTopLevelSubject, subject);
  message_dict.SetString(kTopLevelData, data);
  std::string message_json;

  if (!base::JSONWriter::Write(&message_dict, &message_json)) {
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

  if (event != NULL) {
    event->Signal();
  }
}

bool CastExtensionSession::WrapTasksAndSave() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  EnsureTaskAndSetSend(&signaling_thread_wrapper_);
  if (signaling_thread_wrapper_ == NULL)
    return false;

  base::WaitableEvent wrap_worker_thread_event(true, false);
  worker_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&CastExtensionSession::EnsureTaskAndSetSend,
                 base::Unretained(this),
                 &worker_thread_wrapper_,
                 &wrap_worker_thread_event));
  wrap_worker_thread_event.Wait();

  return (worker_thread_wrapper_ != NULL);
}

bool CastExtensionSession::InitializePeerConnection() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!peer_conn_factory_);
  DCHECK(!peer_connection_);
  DCHECK(worker_thread_wrapper_ != NULL);
  DCHECK(signaling_thread_wrapper_ != NULL);

  peer_conn_factory_ = webrtc::CreatePeerConnectionFactory(
      worker_thread_wrapper_, signaling_thread_wrapper_, NULL, NULL, NULL);

  if (!peer_conn_factory_.get()) {
    CleanupPeerConnection();
    return false;
  }

  VLOG(1) << "Created PeerConnectionFactory successfully.";

  webrtc::PeerConnectionInterface::IceServers servers;
  webrtc::PeerConnectionInterface::IceServer server;
  server.uri = kDefaultStunURI;
  servers.push_back(server);
  webrtc::PeerConnectionInterface::RTCConfiguration rtc_config;
  rtc_config.servers = servers;

  // DTLS-SRTP is the preferred encryption method. If set to kValueFalse, the
  // peer connection uses SDES. Disabling SDES as well will cause the peer
  // connection to fail to connect.
  // Note: For protection and unprotection of SRTP packets, the libjingle
  // ENABLE_EXTERNAL_AUTH flag must not be set.
  webrtc::FakeConstraints constraints;
  constraints.AddMandatory(webrtc::MediaConstraintsInterface::kEnableDtlsSrtp,
                           webrtc::MediaConstraintsInterface::kValueTrue);

  rtc::scoped_refptr<webrtc::PortAllocatorFactoryInterface>
      port_allocator_factory = ChromiumPortAllocatorFactory::Create(
          network_settings_, url_request_context_getter_);

  peer_connection_ = peer_conn_factory_->CreatePeerConnection(
      rtc_config, &constraints, port_allocator_factory, NULL, this);

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

  scoped_ptr<CastVideoCapturerAdapter> cast_video_capturer_adapter(
      new CastVideoCapturerAdapter(desktop_capturer.Pass()));

  // Set video stream constraints.
  webrtc::FakeConstraints video_constraints;
  video_constraints.AddMandatory(
      webrtc::MediaConstraintsInterface::kMinFrameRate, kMinFramesPerSecond);

  rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track =
      peer_conn_factory_->CreateVideoTrack(
          kVideoLabel,
          peer_conn_factory_->CreateVideoSource(
              cast_video_capturer_adapter.release(), &video_constraints));

  stream_ = peer_conn_factory_->CreateLocalMediaStream(kStreamLabel);

  if (!stream_->AddTrack(video_track) ||
      !peer_connection_->AddStream(stream_, NULL)) {
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
      stats_observer_,
      video_track.release(),
      webrtc::PeerConnectionInterface::kStatsOutputLevelStandard);
}

void CastExtensionSession::CleanupPeerConnection() {
  peer_connection_->Close();
  peer_connection_ = NULL;
  stream_ = NULL;
  peer_conn_factory_ = NULL;
  worker_thread_.Stop();
}

bool CastExtensionSession::connection_active() const {
  return peer_connection_.get() != NULL;
}

// webrtc::PeerConnectionObserver implementation -------------------------------

void CastExtensionSession::OnError() {
  VLOG(1) << "PeerConnectionObserver: an error occurred.";
}

void CastExtensionSession::OnSignalingChange(
    webrtc::PeerConnectionInterface::SignalingState new_state) {
  VLOG(1) << "PeerConnectionObserver: SignalingState changed to:" << new_state;
}

void CastExtensionSession::OnStateChange(
    webrtc::PeerConnectionObserver::StateType state_changed) {
  VLOG(1) << "PeerConnectionObserver: StateType changed to: " << state_changed;
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

void CastExtensionSession::OnIceComplete() {
  VLOG(1) << "PeerConnectionObserver: all ICE candidates found.";
}

void CastExtensionSession::OnIceCandidate(
    const webrtc::IceCandidateInterface* candidate) {
  std::string candidate_str;
  if (!candidate->ToString(&candidate_str)) {
    LOG(ERROR) << "PeerConnectionObserver: failed to serialize candidate.";
    return;
  }
  scoped_ptr<base::DictionaryValue> json(new base::DictionaryValue());
  json->SetString(kWebRtcSDPMid, candidate->sdp_mid());
  json->SetInteger(kWebRtcSDPMLineIndex, candidate->sdp_mline_index());
  json->SetString(kWebRtcCandidate, candidate_str);
  std::string json_str;
  if (!base::JSONWriter::Write(json.get(), &json_str)) {
    LOG(ERROR) << "Failed to serialize candidate message.";
    return;
  }
  SendMessageToClient(kSubjectNewCandidate, json_str);
}

}  // namespace remoting

