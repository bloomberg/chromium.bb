// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/connection_to_host.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/message_loop.h"
#include "remoting/base/constants.h"
#include "remoting/jingle_glue/http_port_allocator.h"
#include "remoting/jingle_glue/javascript_signal_strategy.h"
#include "remoting/jingle_glue/xmpp_signal_strategy.h"
#include "remoting/protocol/auth_token_utils.h"
#include "remoting/protocol/client_message_dispatcher.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/host_control_sender.h"
#include "remoting/protocol/input_sender.h"
#include "remoting/protocol/jingle_session_manager.h"
#include "remoting/protocol/video_reader.h"
#include "remoting/protocol/video_stub.h"
#include "remoting/protocol/util.h"

namespace remoting {
namespace protocol {

ConnectionToHost::ConnectionToHost(
    MessageLoop* message_loop,
    talk_base::NetworkManager* network_manager,
    talk_base::PacketSocketFactory* socket_factory,
    PortAllocatorSessionFactory* session_factory,
    bool allow_nat_traversal)
    : message_loop_(message_loop),
      network_manager_(network_manager),
      socket_factory_(socket_factory),
      port_allocator_session_factory_(session_factory),
      allow_nat_traversal_(allow_nat_traversal),
      state_(STATE_EMPTY),
      event_callback_(NULL),
      dispatcher_(new ClientMessageDispatcher()),
      client_stub_(NULL),
      video_stub_(NULL) {
}

ConnectionToHost::~ConnectionToHost() {
}

InputStub* ConnectionToHost::input_stub() {
  return input_sender_.get();
}

HostStub* ConnectionToHost::host_stub() {
  return host_control_sender_.get();
}

void ConnectionToHost::Connect(scoped_refptr<XmppProxy> xmpp_proxy,
                               const std::string& your_jid,
                               const std::string& host_jid,
                               const std::string& host_public_key,
                               const std::string& access_code,
                               HostEventCallback* event_callback,
                               ClientStub* client_stub,
                               VideoStub* video_stub) {
  event_callback_ = event_callback;
  client_stub_ = client_stub;
  video_stub_ = video_stub;
  access_code_ = access_code;

  // Save jid of the host. The actual connection is created later after
  // |signal_strategy_| is connected.
  host_jid_ = host_jid;
  host_public_key_ = host_public_key;

  // Initialize |signal_strategy_|.
  JavascriptSignalStrategy* strategy = new JavascriptSignalStrategy(your_jid);
  strategy->AttachXmppProxy(xmpp_proxy);
  signal_strategy_.reset(strategy);
  signal_strategy_->Init(this);
}

void ConnectionToHost::Disconnect(const base::Closure& shutdown_task) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(
        FROM_HERE, base::Bind(&ConnectionToHost::Disconnect,
                              base::Unretained(this), shutdown_task));
    return;
  }

  CloseChannels();

  if (session_.get())
    session_.reset();

  if (session_manager_.get())
    session_manager_.reset();

  if (signal_strategy_.get())
    signal_strategy_.reset();

  shutdown_task.Run();
}

void ConnectionToHost::InitSession() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  // Initialize chromotocol |session_manager_|.
  JingleSessionManager* session_manager =
      JingleSessionManager::CreateSandboxed(
          network_manager_.release(), socket_factory_.release(),
          port_allocator_session_factory_.release());

  // TODO(ajwong): Make this a command switch when we're more stable.
  session_manager->set_allow_local_ips(true);

  session_manager_.reset(session_manager);
  session_manager_->Init(
      local_jid_, signal_strategy_.get(), this, NULL, "", allow_nat_traversal_);

  // Set the shared-secret for securing SSL channels.
  session_->set_shared_secret(access_code_);
}

const SessionConfig* ConnectionToHost::config() {
  return session_->config();
}

void ConnectionToHost::OnStateChange(
    SignalStrategy::StatusObserver::State state) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK(event_callback_);

  if (state == SignalStrategy::StatusObserver::CONNECTED) {
    VLOG(1) << "Connected as: " << local_jid_;
    InitSession();
  } else if (state == SignalStrategy::StatusObserver::CLOSED) {
    VLOG(1) << "Connection closed.";
    event_callback_->OnConnectionClosed(this);
  }
}

void ConnectionToHost::OnJidChange(const std::string& full_jid) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  local_jid_ = full_jid;
}

void ConnectionToHost::OnSessionManagerInitialized() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  // After SessionManager is initialized we can try to connect to the host.
  CandidateSessionConfig* candidate_config =
      CandidateSessionConfig::CreateDefault();
  std::string client_token =
      protocol::GenerateSupportAuthToken(local_jid_, access_code_);
  session_.reset(session_manager_->Connect(
      host_jid_, host_public_key_, client_token, candidate_config,
      NewCallback(this, &ConnectionToHost::OnSessionStateChange)));
}

void ConnectionToHost::OnIncomingSession(
    Session* session,
    SessionManager::IncomingSessionResponse* response) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  // Client always rejects incoming sessions.
  *response = SessionManager::DECLINE;
}

void ConnectionToHost::OnSessionStateChange(
    Session::State state) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK(event_callback_);

  switch (state) {
    case Session::FAILED:
      state_ = STATE_FAILED;
      CloseChannels();
      event_callback_->OnConnectionFailed(this);
      break;

    case Session::CLOSED:
      state_ = STATE_CLOSED;
      CloseChannels();
      event_callback_->OnConnectionClosed(this);
      break;

    case Session::CONNECTED:
      state_ = STATE_CONNECTED;
      // Initialize reader and writer.
      video_reader_.reset(VideoReader::Create(session_->config()));
      video_reader_->Init(session_.get(), video_stub_);
      host_control_sender_.reset(
          new HostControlSender(session_->control_channel()));
      dispatcher_->Initialize(session_.get(), client_stub_);
      event_callback_->OnConnectionOpened(this);
      break;

    default:
      // Ignore the other states by default.
      break;
  }
}

void ConnectionToHost::CloseChannels() {
  if (input_sender_.get())
    input_sender_->Close();

  if (host_control_sender_.get())
    host_control_sender_->Close();
}

void ConnectionToHost::OnClientAuthenticated() {
  // TODO(hclam): Don't send anything except authentication request if it is
  // not authenticated.
  state_ = STATE_AUTHENTICATED;

  // Create and enable the input stub now that we're authenticated.
  input_sender_.reset(new InputSender(session_->event_channel()));
}

ConnectionToHost::State ConnectionToHost::state() const {
  return state_;
}

}  // namespace protocol
}  // namespace remoting
