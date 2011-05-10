// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/jingle_session.h"

#include "base/message_loop.h"
#include "crypto/rsa_private_key.h"
#include "jingle/glue/channel_socket_adapter.h"
#include "jingle/glue/pseudotcp_adapter.h"
#include "net/base/cert_status_flags.h"
#include "net/base/cert_verifier.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_config_service.h"
#include "net/base/x509_certificate.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/ssl_server_socket.h"
#include "remoting/base/constants.h"
#include "remoting/jingle_glue/jingle_thread.h"
#include "remoting/protocol/jingle_session_manager.h"
#include "remoting/protocol/socket_wrapper.h"
#include "third_party/libjingle/source/talk/base/thread.h"
#include "third_party/libjingle/source/talk/p2p/base/session.h"
#include "third_party/libjingle/source/talk/p2p/base/p2ptransportchannel.h"

using cricket::BaseSession;

namespace remoting {

namespace protocol {

const char JingleSession::kChromotingContentName[] = "chromoting";

namespace {
const char kControlChannelName[] = "control";
const char kEventChannelName[] = "event";
const char kVideoChannelName[] = "video";
const char kVideoRtpChannelName[] = "videortp";
const char kVideoRtcpChannelName[] = "videortcp";

// Helper method to create a SSL client socket.
net::SSLClientSocket* CreateSSLClientSocket(
    net::StreamSocket* socket, scoped_refptr<net::X509Certificate> cert,
    net::CertVerifier* cert_verifier) {
  net::SSLConfig ssl_config;
  ssl_config.false_start_enabled = false;
  ssl_config.ssl3_enabled = true;
  ssl_config.tls1_enabled = true;

  // Certificate provided by the host doesn't need authority.
  net::SSLConfig::CertAndStatus cert_and_status;
  cert_and_status.cert_status = net::CERT_STATUS_AUTHORITY_INVALID;
  cert_and_status.cert = cert;
  ssl_config.allowed_bad_certs.push_back(cert_and_status);

  // SSLClientSocket takes ownership of the adapter.
  net::HostPortPair host_and_pair(JingleSession::kChromotingContentName, 0);
  net::SSLClientSocket* ssl_socket =
      net::ClientSocketFactory::GetDefaultFactory()->CreateSSLClientSocket(
          socket, host_and_pair, ssl_config, NULL, cert_verifier);
  return ssl_socket;
}

}  // namespace

// static
JingleSession* JingleSession::CreateClientSession(
    JingleSessionManager* manager) {
  return new JingleSession(manager, NULL, NULL);
}

// static
JingleSession* JingleSession::CreateServerSession(
    JingleSessionManager* manager,
    scoped_refptr<net::X509Certificate> certificate,
    crypto::RSAPrivateKey* key) {
  return new JingleSession(manager, certificate, key);
}

JingleSession::JingleSession(
    JingleSessionManager* jingle_session_manager,
    scoped_refptr<net::X509Certificate> server_cert, crypto::RSAPrivateKey* key)
    : jingle_session_manager_(jingle_session_manager),
      server_cert_(server_cert),
      state_(INITIALIZING),
      closed_(false),
      closing_(false),
      cricket_session_(NULL),
      ssl_connections_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(connect_callback_(
          this, &JingleSession::OnConnect)),
      ALLOW_THIS_IN_INITIALIZER_LIST(ssl_connect_callback_(
          this, &JingleSession::OnSSLConnect)) {
  // TODO(hclam): Need a better way to clone a key.
  if (key) {
    std::vector<uint8> key_bytes;
    CHECK(key->ExportPrivateKey(&key_bytes));
    key_.reset(crypto::RSAPrivateKey::CreateFromPrivateKeyInfo(key_bytes));
    CHECK(key_.get());
  }
}

JingleSession::~JingleSession() {
  DCHECK(closed_);
}

void JingleSession::Init(cricket::Session* cricket_session) {
  DCHECK_EQ(jingle_session_manager_->message_loop(), MessageLoop::current());

  cricket_session_ = cricket_session;
  jid_ = cricket_session_->remote_name();
  cert_verifier_.reset(new net::CertVerifier());
  cricket_session_->SignalState.connect(
      this, &JingleSession::OnSessionState);
  cricket_session_->SignalError.connect(
      this, &JingleSession::OnSessionError);
}

void JingleSession::CloseInternal(int result, bool failed) {
  DCHECK_EQ(jingle_session_manager_->message_loop(), MessageLoop::current());

  if (!closed_ && !closing_) {
    closing_ = true;

    if (control_channel_.get())
      control_channel_->Close(result);
    if (control_ssl_socket_.get())
      control_ssl_socket_->Disconnect();

    if (event_channel_.get())
      event_channel_->Close(result);
    if (event_ssl_socket_.get())
      event_ssl_socket_->Disconnect();

    if (video_channel_.get())
      video_channel_->Close(result);
    if (video_ssl_socket_.get())
      video_ssl_socket_->Disconnect();

    if (video_rtp_channel_.get())
      video_rtp_channel_->Close(result);
    if (video_rtcp_channel_.get())
      video_rtcp_channel_->Close(result);

    if (cricket_session_)
      cricket_session_->Terminate();

    if (failed)
      SetState(FAILED);
    else
      SetState(CLOSED);

    closed_ = true;
  }
  cert_verifier_.reset();
}

bool JingleSession::HasSession(cricket::Session* cricket_session) {
  return cricket_session_ == cricket_session;
}

cricket::Session* JingleSession::ReleaseSession() {
  DCHECK_EQ(jingle_session_manager_->message_loop(), MessageLoop::current());

  // Session may be destroyed only after it is closed.
  DCHECK(closed_);

  cricket::Session* session = cricket_session_;
  if (cricket_session_)
    cricket_session_->SignalState.disconnect(this);
  cricket_session_ = NULL;
  return session;
}

void JingleSession::SetStateChangeCallback(StateChangeCallback* callback) {
  DCHECK_EQ(jingle_session_manager_->message_loop(), MessageLoop::current());
  DCHECK(callback);
  state_change_callback_.reset(callback);
}

net::Socket* JingleSession::control_channel() {
  DCHECK_EQ(jingle_session_manager_->message_loop(), MessageLoop::current());
  return control_ssl_socket_.get();
}

net::Socket* JingleSession::event_channel() {
  DCHECK_EQ(jingle_session_manager_->message_loop(), MessageLoop::current());
  return event_ssl_socket_.get();
}

// TODO(sergeyu): Remove this method after we switch to RTP.
net::Socket* JingleSession::video_channel() {
  DCHECK_EQ(jingle_session_manager_->message_loop(), MessageLoop::current());
  return video_ssl_socket_.get();
}

net::Socket* JingleSession::video_rtp_channel() {
  DCHECK_EQ(jingle_session_manager_->message_loop(), MessageLoop::current());
  return video_rtp_channel_.get();
}

net::Socket* JingleSession::video_rtcp_channel() {
  DCHECK_EQ(jingle_session_manager_->message_loop(), MessageLoop::current());
  return video_rtcp_channel_.get();
}

const std::string& JingleSession::jid() {
  // No synchronization is needed because jid_ is not changed
  // after new connection is passed to JingleChromotocolServer callback.
  return jid_;
}

MessageLoop* JingleSession::message_loop() {
  return jingle_session_manager_->message_loop();
}

const CandidateSessionConfig* JingleSession::candidate_config() {
  DCHECK(candidate_config_.get());
  return candidate_config_.get();
}

void JingleSession::set_candidate_config(
    const CandidateSessionConfig* candidate_config) {
  DCHECK(!candidate_config_.get());
  DCHECK(candidate_config);
  candidate_config_.reset(candidate_config);
}

scoped_refptr<net::X509Certificate> JingleSession::server_certificate() const {
  return server_cert_;
}

const SessionConfig* JingleSession::config() {
  DCHECK(config_.get());
  return config_.get();
}

void JingleSession::set_config(const SessionConfig* config) {
  DCHECK(!config_.get());
  DCHECK(config);
  config_.reset(config);
}

const std::string& JingleSession::initiator_token() {
  return initiator_token_;
}

void JingleSession::set_initiator_token(const std::string& initiator_token) {
  initiator_token_ = initiator_token;
}

const std::string& JingleSession::receiver_token() {
  return receiver_token_;
}

void JingleSession::set_receiver_token(const std::string& receiver_token) {
  receiver_token_ = receiver_token;
}

void JingleSession::Close(Task* closed_task) {
  if (MessageLoop::current() != jingle_session_manager_->message_loop()) {
    jingle_session_manager_->message_loop()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &JingleSession::Close, closed_task));
    return;
  }

  CloseInternal(net::ERR_CONNECTION_CLOSED, false);

  if (closed_task) {
    closed_task->Run();
    delete closed_task;
  }
}

void JingleSession::OnSessionState(
    BaseSession* session, BaseSession::State state) {
  DCHECK_EQ(cricket_session_, session);

  if (closed_) {
    // Don't do anything if we already closed.
    return;
  }

  switch (state) {
    case cricket::Session::STATE_SENTINITIATE:
    case cricket::Session::STATE_RECEIVEDINITIATE:
      OnInitiate();
      break;

    case cricket::Session::STATE_SENTACCEPT:
    case cricket::Session::STATE_RECEIVEDACCEPT:
      OnAccept();
      break;

    case cricket::Session::STATE_SENTTERMINATE:
    case cricket::Session::STATE_RECEIVEDTERMINATE:
    case cricket::Session::STATE_SENTREJECT:
    case cricket::Session::STATE_RECEIVEDREJECT:
      OnTerminate();
      break;

    case cricket::Session::STATE_DEINIT:
      // Close() must have been called before this.
      NOTREACHED();
      break;

    default:
      // We don't care about other steates.
      break;
  }
}

void JingleSession::OnSessionError(
    BaseSession* session, BaseSession::Error error) {
  DCHECK_EQ(cricket_session_, session);

  if (error != cricket::Session::ERROR_NONE) {
    CloseInternal(net::ERR_CONNECTION_ABORTED, true);
  }
}

void JingleSession::OnInitiate() {
  jid_ = cricket_session_->remote_name();

  std::string content_name;
  // If we initiate the session, we get to specify the content name. When
  // accepting one, the remote end specifies it.
  if (cricket_session_->initiator()) {
    content_name = kChromotingContentName;
  } else {
    const cricket::ContentInfo* content;
    content = cricket_session_->remote_description()->FirstContentByType(
        kChromotingXmlNamespace);
    CHECK(content);
    content_name = content->name;
  }

  // Create video RTP channels.
  raw_video_rtp_channel_ =
      cricket_session_->CreateChannel(content_name, kVideoRtpChannelName);
  video_rtp_channel_.reset(
      new jingle_glue::TransportChannelSocketAdapter(raw_video_rtp_channel_));
  raw_video_rtcp_channel_ =
      cricket_session_->CreateChannel(content_name, kVideoRtcpChannelName);
  video_rtcp_channel_.reset(
      new jingle_glue::TransportChannelSocketAdapter(raw_video_rtcp_channel_));

  // Create control channel.
  raw_control_channel_ =
      cricket_session_->CreateChannel(content_name, kControlChannelName);
  control_channel_.reset(
      new jingle_glue::TransportChannelSocketAdapter(raw_control_channel_));

  // Create event channel.
  raw_event_channel_ =
      cricket_session_->CreateChannel(content_name, kEventChannelName);
  event_channel_.reset(
      new jingle_glue::TransportChannelSocketAdapter(raw_event_channel_));

  // Create video channel.
  // TODO(sergeyu): Remove video channel when we are ready to switch to RTP.
  raw_video_channel_ =
      cricket_session_->CreateChannel(content_name, kVideoChannelName);
  video_channel_.reset(
      new jingle_glue::TransportChannelSocketAdapter(raw_video_channel_));

  if (!cricket_session_->initiator())
    jingle_session_manager_->AcceptConnection(this, cricket_session_);

  if (!closed_) {
    // Set state to CONNECTING if the session is being accepted.
    SetState(CONNECTING);
  }
}

bool JingleSession::EstablishSSLConnection(
    net::Socket* channel, scoped_ptr<SocketWrapper>* ssl_socket) {
  jingle_glue::PseudoTcpAdapter* pseudotcp =
      new jingle_glue::PseudoTcpAdapter(channel);
  pseudotcp->Connect(&connect_callback_);

  if (cricket_session_->initiator()) {
    // Create client SSL socket.
    net::SSLClientSocket* socket = CreateSSLClientSocket(
        pseudotcp, server_cert_, cert_verifier_.get());
    ssl_socket->reset(new SocketWrapper(socket));

    int ret = socket->Connect(&ssl_connect_callback_);
    if (ret == net::ERR_IO_PENDING) {
      return true;
    } else if (ret != net::OK) {
      LOG(ERROR) << "Failed to establish SSL connection";
      cricket_session_->Terminate();
      return false;
    }
  } else {
    // Create server SSL socket.
    net::SSLConfig ssl_config;
    net::SSLServerSocket* socket = net::CreateSSLServerSocket(
        pseudotcp, server_cert_, key_.get(), ssl_config);
    ssl_socket->reset(new SocketWrapper(socket));

    int ret = socket->Accept(&ssl_connect_callback_);
    if (ret == net::ERR_IO_PENDING) {
      return true;
    } else if (ret != net::OK) {
      LOG(ERROR) << "Failed to establish SSL connection";
      cricket_session_->Terminate();
      return false;
    }
  }
  // Reach here if net::OK is received.
  ssl_connect_callback_.Run(net::OK);
  return true;
}

bool JingleSession::InitializeConfigFromDescription(
    const cricket::SessionDescription* description) {
  // We should only be called after ParseContent has succeeded, in which
  // case there will always be a Chromoting session configuration.
  const cricket::ContentInfo* content =
    description->FirstContentByType(kChromotingXmlNamespace);
  CHECK(content);
  const protocol::ContentDescription* content_description =
    static_cast<const protocol::ContentDescription*>(content->description);
  CHECK(content_description);

  server_cert_ = content_description->certificate();
  if (!server_cert_) {
    LOG(ERROR) << "Connection response does not specify certificate";
    return false;
  }

  scoped_ptr<SessionConfig> config(
    content_description->config()->GetFinalConfig());
  if (!config.get()) {
    LOG(ERROR) << "Connection response does not specify configuration";
    return false;
  }
  if (!candidate_config()->IsSupported(config.get())) {
    LOG(ERROR) << "Connection response specifies an invalid configuration";
    return false;
  }

  set_config(config.release());
  return true;
}

bool JingleSession::InitializeSSL() {
  if (!EstablishSSLConnection(control_channel_.release(),
                              &control_ssl_socket_)) {
    LOG(ERROR) << "Establish control channel failed";
    return false;
  }
  if (!EstablishSSLConnection(event_channel_.release(),
                              &event_ssl_socket_)) {
    LOG(ERROR) << "Establish event channel failed";
    return false;
  }
  if (!EstablishSSLConnection(video_channel_.release(),
                              &video_ssl_socket_)) {
    LOG(ERROR) << "Establish video channel failed";
    return false;
  }
  return true;
}

void JingleSession::InitializeChannels() {
  // Disable incoming connections on the host so that we don't travers
  // the firewall.
  if (!cricket_session_->initiator()) {
    raw_control_channel_->GetP2PChannel()->set_incoming_only(true);
    raw_event_channel_->GetP2PChannel()->set_incoming_only(true);
    raw_video_channel_->GetP2PChannel()->set_incoming_only(true);
    raw_video_rtp_channel_->GetP2PChannel()->set_incoming_only(true);
    raw_video_rtcp_channel_->GetP2PChannel()->set_incoming_only(true);
  }

  if (!InitializeSSL()) {
    CloseInternal(net::ERR_CONNECTION_FAILED, true);
    return;
  }
}

void JingleSession::OnAccept() {
  // If we initiated the session, store the candidate configuration that the
  // host responded with, to refer to later.
  if (cricket_session_->initiator()) {
    if (!InitializeConfigFromDescription(
        cricket_session_->remote_description())) {
      CloseInternal(net::ERR_CONNECTION_FAILED, true);
      return;
    }
  }

  // TODO(sergeyu): This is a hack: Currently set_incoming_only()
  // needs to be called on each channel before the channel starts
  // creating candidates but after session is accepted (after
  // TransportChannelProxy::GetP2PChannel() starts returning actual
  // P2P channel). By posting a task here we can call it at the right
  // moment. This problem will go away when we switch to Pepper P2P
  // API.
  jingle_session_manager_->message_loop()->PostTask(
    FROM_HERE, NewRunnableMethod(this, &JingleSession::InitializeChannels));
}

void JingleSession::OnTerminate() {
  CloseInternal(net::ERR_CONNECTION_ABORTED, false);
}

void JingleSession::OnConnect(int result) {
  if (result != net::OK) {
    LOG(ERROR) << "PseudoTCP connection failed: " << result;
    CloseInternal(result, true);
  }
}

void JingleSession::OnSSLConnect(int result) {
  DCHECK(!closed_);
  if (result != net::OK) {
    LOG(ERROR) << "Error during SSL connection: " << result;
    CloseInternal(result, true);
    return;
  }

  // Number of channels for a jingle session.
  const int kChannels = 3;

  // Set the state to connected only of all SSL sockets are connected.
  if (++ssl_connections_ == kChannels) {
    SetState(CONNECTED);
  }
  CHECK(ssl_connections_ <= kChannels) << "Unexpected SSL connect callback";
}

void JingleSession::SetState(State new_state) {
  if (new_state != state_) {
    DCHECK_NE(state_, CLOSED);
    DCHECK_NE(state_, FAILED);

    state_ = new_state;
    if (!closed_ && state_change_callback_.get())
      state_change_callback_->Run(new_state);
  }
}

}  // namespace protocol

}  // namespace remoting
