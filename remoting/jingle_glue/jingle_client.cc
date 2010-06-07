// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/jingle_glue/jingle_client.h"

#include "base/logging.h"
#include "base/waitable_event.h"
#include "base/message_loop.h"
#include "chrome/common/net/notifier/communicator/xmpp_socket_adapter.h"
#include "remoting/jingle_glue/jingle_thread.h"
#include "remoting/jingle_glue/relay_port_allocator.h"
#include "talk/base/asyncsocket.h"
#include "talk/base/ssladapter.h"
#include "talk/p2p/base/sessionmanager.h"
#include "talk/p2p/client/sessionmanagertask.h"
#ifdef USE_SSL_TUNNEL
#include "talk/session/tunnel/securetunnelsessionclient.h"
#endif
#include "talk/session/tunnel/tunnelsessionclient.h"

namespace remoting {

JingleClient::JingleClient()
    : callback_(NULL),
      state_(START) { }

JingleClient::~JingleClient() {
  // JingleClient can be destroyed only after it's closed.
  DCHECK(state_ == CLOSED);
}

void JingleClient::Init(const std::string& username,
                        const std::string& password,
                        Callback* callback) {
  DCHECK(username != "");
  DCHECK(callback != NULL);
  DCHECK(thread_ == NULL);  // Init() can be called only once.

  callback_ = callback;

  username_ = username;
  password_ = password;

  thread_.reset(new JingleThread());
  thread_->Start();
  thread_->message_loop()->PostTask(
      FROM_HERE, NewRunnableMethod(this, &JingleClient::DoInitialize));
}

class JingleClient::ConnectRequest {
 public:
  ConnectRequest()
      : completed_event_(true, false) { }

  JingleChannel* Wait() {
    completed_event_.Wait();
    return channel_;
  };

  void Done(JingleChannel* channel) {
    channel_ = channel;
    completed_event_.Signal();
  };

 private:
  base::WaitableEvent completed_event_;
  JingleChannel* channel_;
};

JingleChannel* JingleClient::Connect(const std::string& host_jid,
                                     JingleChannel::Callback* callback) {
  ConnectRequest request;
  thread_->message_loop()->PostTask(
      FROM_HERE, NewRunnableMethod(this, &JingleClient::DoConnect,
                                   &request, host_jid, callback));
  return request.Wait();
}

void JingleClient::DoConnect(ConnectRequest* request,
                             const std::string& host_jid,
                             JingleChannel::Callback* callback) {
  talk_base::StreamInterface* stream =
      tunnel_session_client_->CreateTunnel(buzz::Jid(host_jid), "");
  DCHECK(stream != NULL);

  JingleChannel* channel = new JingleChannel(callback);
  channel->Init(thread_.get(), stream, host_jid);
  request->Done(channel);
}

void JingleClient::Close() {
  DCHECK(thread_ != NULL);  // Close() be called only after Init().
  message_loop()->PostTask(
      FROM_HERE, NewRunnableMethod(this, &JingleClient::DoClose));
  thread_->Stop();
  thread_.reset(NULL);
}

void JingleClient::DoClose() {
  client_->Disconnect();
  // Client is deleted by TaskRunner.
  client_ = NULL;
  tunnel_session_client_.reset();
  port_allocator_.reset();
  session_manager_.reset();
  network_manager_.reset();
  UpdateState(CLOSED);
}

void JingleClient::DoInitialize() {
  buzz::Jid login_jid(username_);
  talk_base::InsecureCryptStringImpl password;
  password.password() = password_;

  buzz::XmppClientSettings xcs;
  xcs.set_user(login_jid.node());
  xcs.set_host(login_jid.domain());
  xcs.set_resource("chromoting");
  xcs.set_use_tls(true);
  xcs.set_pass(talk_base::CryptString(password));
  xcs.set_server(talk_base::SocketAddress("talk.google.com", 5222));

  client_ = new buzz::XmppClient(thread_->task_pump());
  client_->SignalStateChange.connect(
      this, &JingleClient::OnConnectionStateChanged);

  buzz::AsyncSocket* socket =
      new notifier::XmppSocketAdapter(xcs, false);

  client_->Connect(xcs, "", socket, NULL);
  client_->Start();

  network_manager_.reset(new talk_base::NetworkManager());

  RelayPortAllocator* port_allocator =
      new RelayPortAllocator(network_manager_.get(), "transp2");
  port_allocator_.reset(port_allocator);
  port_allocator->SetJingleInfo(client_);

  session_manager_.reset(new cricket::SessionManager(port_allocator_.get()));
#ifdef USE_SSL_TUNNEL
  cricket::SecureTunnelSessionClient* session_client =
      new cricket::SecureTunnelSessionClient(client_->jid(),
                                             session_manager_.get());
  if (!session_client->GenerateIdentity())
    return false;
  tunnel_session_client_.reset(session_client);
#else  // !USE_SSL_TUNNEL
  tunnel_session_client_.reset(
      new cricket::TunnelSessionClient(client_->jid(),
                                       session_manager_.get()));
#endif  // USE_SSL_TUNNEL

  receiver_ = new cricket::SessionManagerTask(client_, session_manager_.get());
  receiver_->EnableOutgoingMessages();
  receiver_->Start();

  tunnel_session_client_->SignalIncomingTunnel.connect(
      this, &JingleClient::OnIncomingTunnel);
}

std::string JingleClient::GetFullJid() {
  AutoLock auto_lock(full_jid_lock_);
  return full_jid_;
}

MessageLoop* JingleClient::message_loop() {
  if (thread_ == NULL) {
    return NULL;
  }
  return thread_->message_loop();
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
      {
        AutoLock auto_lock(full_jid_lock_);
        full_jid_ = client_->jid().Str();
      }
      UpdateState(CONNECTED);
      break;
    case buzz::XmppEngine::STATE_CLOSED:
      UpdateState(CLOSED);
      break;
  }
}

void JingleClient::OnIncomingTunnel(
    cricket::TunnelSessionClient* client, buzz::Jid jid,
    std::string description, cricket::Session* session) {
  // Decline connection if we don't have callback.
  if (!callback_) {
    client->DeclineTunnel(session);
    return;
  }

  JingleChannel::Callback* channel_callback;
  if (callback_->OnAcceptConnection(this, jid.Str(), &channel_callback)) {
    DCHECK(channel_callback != NULL);
    talk_base::StreamInterface* stream =
        client->AcceptTunnel(session);
    scoped_refptr<JingleChannel> channel(new JingleChannel(channel_callback));
    channel->Init(thread_.get(), stream, jid.Str());
    callback_->OnNewConnection(this, channel);
  } else {
    client->DeclineTunnel(session);
    return;
  }
}

void JingleClient::UpdateState(State new_state) {
  if (new_state != state_) {
    state_ = new_state;
    if (callback_)
      callback_->OnStateChange(this, new_state);
  }
}

}  // namespace remoting
