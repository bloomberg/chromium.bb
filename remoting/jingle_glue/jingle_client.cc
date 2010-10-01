// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/jingle_glue/jingle_client.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "jingle/notifier/communicator/gaia_token_pre_xmpp_auth.h"
#include "remoting/jingle_glue/iq_request.h"
#include "remoting/jingle_glue/jingle_thread.h"
#include "remoting/jingle_glue/relay_port_allocator.h"
#include "remoting/jingle_glue/xmpp_socket_adapter.h"
#include "third_party/libjingle/source/talk/base/asyncsocket.h"
#include "third_party/libjingle/source/talk/base/ssladapter.h"
#include "third_party/libjingle/source/talk/p2p/base/sessionmanager.h"
#include "third_party/libjingle/source/talk/p2p/base/transport.h"
#include "third_party/libjingle/source/talk/p2p/client/sessionmanagertask.h"
#include "third_party/libjingle/source/talk/session/tunnel/tunnelsessionclient.h"
#include "third_party/libjingle/source/talk/xmpp/prexmppauth.h"
#include "third_party/libjingle/source/talk/xmpp/saslcookiemechanism.h"

namespace remoting {

namespace {
// A TunnelSessionClient that allows local connections.
class LocalTunnelSessionClient : public cricket::TunnelSessionClient {
 public:
  LocalTunnelSessionClient(const buzz::Jid& jid,
                           cricket::SessionManager* manager)
      : TunnelSessionClient(jid, manager) {
  }

 protected:
  virtual cricket::TunnelSession* MakeTunnelSession(
      cricket::Session* session, talk_base::Thread* stream_thread,
      cricket::TunnelSessionRole role) {
    session->set_allow_local_ips(true);
    return new cricket::TunnelSession(this, session, stream_thread);
  }
};
}  // namespace

JingleClient::JingleClient(JingleThread* thread)
    : thread_(thread),
      callback_(NULL),
      client_(NULL),
      state_(START),
      initialized_(false),
      closed_(false) {
}

JingleClient::~JingleClient() {
  AutoLock auto_lock(state_lock_);
  DCHECK(!initialized_ || closed_);
}

void JingleClient::Init(
    const std::string& username, const std::string& auth_token,
    const std::string& auth_token_service, Callback* callback) {
  DCHECK_NE(username, "");

  {
    AutoLock auto_lock(state_lock_);
    DCHECK(!initialized_ && !closed_);
    initialized_ = true;

    DCHECK(callback != NULL);
    callback_ = callback;
  }

  message_loop()->PostTask(
      FROM_HERE, NewRunnableMethod(this, &JingleClient::DoInitialize,
                                   username, auth_token, auth_token_service));
}

void JingleClient::DoInitialize(const std::string& username,
                                const std::string& auth_token,
                                const std::string& auth_token_service) {
  DCHECK_EQ(message_loop(), MessageLoop::current());

  buzz::Jid login_jid(username);

  buzz::XmppClientSettings settings;
  settings.set_user(login_jid.node());
  settings.set_host(login_jid.domain());
  settings.set_resource("chromoting");
  settings.set_use_tls(true);
  settings.set_token_service(auth_token_service);
  settings.set_auth_cookie(auth_token);
  settings.set_server(talk_base::SocketAddress("talk.google.com", 5222));

  client_ = new buzz::XmppClient(thread_->task_pump());
  client_->SignalStateChange.connect(
      this, &JingleClient::OnConnectionStateChanged);

  buzz::AsyncSocket* socket = new XmppSocketAdapter(settings, false);

  client_->Connect(settings, "", socket, CreatePreXmppAuth(settings));
  client_->Start();

  network_manager_.reset(new talk_base::NetworkManager());

  RelayPortAllocator* port_allocator =
      new RelayPortAllocator(network_manager_.get(), "transp2");
  port_allocator_.reset(port_allocator);
  port_allocator->SetJingleInfo(client_);

  session_manager_.reset(new cricket::SessionManager(port_allocator_.get()));

  tunnel_session_client_.reset(
      new LocalTunnelSessionClient(client_->jid(),
                                   session_manager_.get()));

  cricket::SessionManagerTask* receiver =
      new cricket::SessionManagerTask(client_, session_manager_.get());
  receiver->EnableOutgoingMessages();
  receiver->Start();

  tunnel_session_client_->SignalIncomingTunnel.connect(
      this, &JingleClient::OnIncomingTunnel);
}

JingleChannel* JingleClient::Connect(const std::string& host_jid,
                                     JingleChannel::Callback* callback) {
  DCHECK(initialized_ && !closed_);

  // Ownership if channel is given to DoConnect.
  scoped_refptr<JingleChannel> channel = new JingleChannel(callback);
  message_loop()->PostTask(
      FROM_HERE, NewRunnableMethod(this, &JingleClient::DoConnect,
                                   channel, host_jid, callback));
  return channel;
}

void JingleClient::DoConnect(scoped_refptr<JingleChannel> channel,
                             const std::string& host_jid,
                             JingleChannel::Callback* callback) {
  DCHECK_EQ(message_loop(), MessageLoop::current());

  talk_base::StreamInterface* stream =
      tunnel_session_client_->CreateTunnel(buzz::Jid(host_jid), "");
  DCHECK(stream != NULL);

  channel->Init(thread_, stream, host_jid);
}

void JingleClient::Close() {
  Close(NULL);
}

void JingleClient::Close(Task* closed_task) {
  {
    AutoLock auto_lock(state_lock_);
    // If the client is already closed then don't close again.
    if (closed_) {
      if (closed_task)
        thread_->message_loop()->PostTask(FROM_HERE, closed_task);
      return;
    }
    closed_task_.reset(closed_task);
    closed_ = true;
  }

  message_loop()->PostTask(
      FROM_HERE, NewRunnableMethod(this, &JingleClient::DoClose));
}

void JingleClient::DoClose() {
  DCHECK_EQ(message_loop(), MessageLoop::current());
  DCHECK(closed_);

  tunnel_session_client_.reset();
  session_manager_.reset();
  port_allocator_.reset();
  network_manager_.reset();

  if (client_) {
    client_->Disconnect();
    // Client is deleted by TaskRunner.
    client_ = NULL;
  }

  if (closed_task_.get()) {
    closed_task_->Run();
    closed_task_.reset();
  }
}

std::string JingleClient::GetFullJid() {
  AutoLock auto_lock(full_jid_lock_);
  return full_jid_;
}

IqRequest* JingleClient::CreateIqRequest() {
  return new IqRequest(this);
}

MessageLoop* JingleClient::message_loop() {
  return thread_->message_loop();
}

cricket::SessionManager* JingleClient::session_manager() {
  DCHECK_EQ(message_loop(), MessageLoop::current());
  return session_manager_.get();
}

void JingleClient::OnConnectionStateChanged(buzz::XmppEngine::State state) {
  switch (state) {
    case buzz::XmppEngine::STATE_START:
      UpdateState(START);
      break;
    case buzz::XmppEngine::STATE_OPENING:
      UpdateState(CONNECTING);
      break;
    case buzz::XmppEngine::STATE_OPEN:
      SetFullJid(client_->jid().Str());
      UpdateState(CONNECTED);
      break;
    case buzz::XmppEngine::STATE_CLOSED:
      UpdateState(CLOSED);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void JingleClient::OnIncomingTunnel(
    cricket::TunnelSessionClient* client, buzz::Jid jid,
    std::string description, cricket::Session* session) {
  // Must always lock state and check closed_ when calling callback.
  AutoLock auto_lock(state_lock_);

  if (closed_) {
    // Always reject connection if we are closed.
    client->DeclineTunnel(session);
    return;
  }

  JingleChannel::Callback* channel_callback;
  if (callback_->OnAcceptConnection(this, jid.Str(), &channel_callback)) {
    DCHECK(channel_callback != NULL);
    talk_base::StreamInterface* stream = client->AcceptTunnel(session);
    scoped_refptr<JingleChannel> channel(new JingleChannel(channel_callback));
    channel->Init(thread_, stream, jid.Str());
    callback_->OnNewConnection(this, channel);
  } else {
    client->DeclineTunnel(session);
  }
}

void JingleClient::SetFullJid(const std::string& full_jid) {
  AutoLock auto_lock(full_jid_lock_);
  full_jid_ = full_jid;
}

void JingleClient::UpdateState(State new_state) {
  if (new_state != state_) {
    state_ = new_state;
    {
      // We have to have the lock held, otherwise we cannot be sure that
      // the client hasn't been closed when we call the callback.
      AutoLock auto_lock(state_lock_);
      if (!closed_)
        callback_->OnStateChange(this, new_state);
    }
  }
}

buzz::PreXmppAuth* JingleClient::CreatePreXmppAuth(
    const buzz::XmppClientSettings& settings) {
  buzz::Jid jid(settings.user(), settings.host(), buzz::STR_EMPTY);
  return new notifier::GaiaTokenPreXmppAuth(jid.Str(), settings.auth_cookie(),
                                            settings.token_service());
}

}  // namespace remoting
