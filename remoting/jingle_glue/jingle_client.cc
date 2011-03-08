// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/jingle_glue/jingle_client.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "jingle/notifier/communicator/gaia_token_pre_xmpp_auth.h"
#include "remoting/jingle_glue/http_port_allocator.h"
#include "remoting/jingle_glue/iq_request.h"
#include "remoting/jingle_glue/jingle_thread.h"
#include "remoting/jingle_glue/xmpp_socket_adapter.h"
#include "third_party/libjingle/source/talk/base/asyncsocket.h"
#include "third_party/libjingle/source/talk/base/basicpacketsocketfactory.h"
#include "third_party/libjingle/source/talk/base/ssladapter.h"
#include "third_party/libjingle/source/talk/p2p/base/sessionmanager.h"
#include "third_party/libjingle/source/talk/p2p/base/transport.h"
#include "third_party/libjingle/source/talk/p2p/client/sessionmanagertask.h"
#include "third_party/libjingle/source/talk/session/tunnel/tunnelsessionclient.h"
#include "third_party/libjingle/source/talk/xmpp/prexmppauth.h"
#include "third_party/libjingle/source/talk/xmpp/saslcookiemechanism.h"

namespace remoting {

// The XmppSignalStrategy encapsulates all the logic to perform the signaling
// STUN/ICE for jingle via a direct XMPP connection.
//
// This class is not threadsafe.
XmppSignalStrategy::XmppSignalStrategy(JingleThread* jingle_thread,
                                       const std::string& username,
                                       const std::string& auth_token,
                                       const std::string& auth_token_service)
   : thread_(jingle_thread),
     username_(username),
     auth_token_(auth_token),
     auth_token_service_(auth_token_service),
     xmpp_client_(NULL),
     observer_(NULL) {
}

XmppSignalStrategy::~XmppSignalStrategy() {
}

void XmppSignalStrategy::Init(StatusObserver* observer) {
  observer_ = observer;

  buzz::Jid login_jid(username_);

  buzz::XmppClientSettings settings;
  settings.set_user(login_jid.node());
  settings.set_host(login_jid.domain());
  settings.set_resource("chromoting");
  settings.set_use_tls(true);
  settings.set_token_service(auth_token_service_);
  settings.set_auth_cookie(auth_token_);
  settings.set_server(talk_base::SocketAddress("talk.google.com", 5222));

  buzz::AsyncSocket* socket = new XmppSocketAdapter(settings, false);

  xmpp_client_ = new buzz::XmppClient(thread_->task_pump());
  xmpp_client_->Connect(settings, "", socket, CreatePreXmppAuth(settings));
  xmpp_client_->SignalStateChange.connect(
      this, &XmppSignalStrategy::OnConnectionStateChanged);
  xmpp_client_->Start();
}

void XmppSignalStrategy::StartSession(
    cricket::SessionManager* session_manager) {
  cricket::SessionManagerTask* receiver =
      new cricket::SessionManagerTask(xmpp_client_, session_manager);
  receiver->EnableOutgoingMessages();
  receiver->Start();
}

void XmppSignalStrategy::EndSession() {
  if (xmpp_client_) {
    xmpp_client_->Disconnect();
    // Client is deleted by TaskRunner.
    xmpp_client_ = NULL;
  }
}

IqRequest* XmppSignalStrategy::CreateIqRequest() {
  return new XmppIqRequest(thread_->message_loop(), xmpp_client_);
}

void XmppSignalStrategy::OnConnectionStateChanged(
    buzz::XmppEngine::State state) {
  switch (state) {
    case buzz::XmppEngine::STATE_START:
      observer_->OnStateChange(StatusObserver::START);
      break;
    case buzz::XmppEngine::STATE_OPENING:
      observer_->OnStateChange(StatusObserver::CONNECTING);
      break;
    case buzz::XmppEngine::STATE_OPEN:
      observer_->OnJidChange(xmpp_client_->jid().Str());
      observer_->OnStateChange(StatusObserver::CONNECTED);
      break;
    case buzz::XmppEngine::STATE_CLOSED:
      observer_->OnStateChange(StatusObserver::CLOSED);
      // Client is destroyed by the TaskRunner after the client is
      // closed. Reset the pointer so we don't try to use it later.
      xmpp_client_ = NULL;
      break;
    default:
      NOTREACHED();
      break;
  }
}

buzz::PreXmppAuth* XmppSignalStrategy::CreatePreXmppAuth(
    const buzz::XmppClientSettings& settings) {
  buzz::Jid jid(settings.user(), settings.host(), buzz::STR_EMPTY);
  return new notifier::GaiaTokenPreXmppAuth(jid.Str(), settings.auth_cookie(),
                                            settings.token_service());
}


JavascriptSignalStrategy::JavascriptSignalStrategy(const std::string& your_jid)
    : your_jid_(your_jid) {
}

JavascriptSignalStrategy::~JavascriptSignalStrategy() {
}

void JavascriptSignalStrategy::Init(StatusObserver* observer) {
  // Blast through each state since for a JavascriptSignalStrategy, we're
  // already connected.
  //
  // TODO(ajwong): Clarify the status API contract to see if we have to actually
  // walk through each state.
  observer->OnStateChange(StatusObserver::START);
  observer->OnStateChange(StatusObserver::CONNECTING);
  observer->OnJidChange(your_jid_);
  observer->OnStateChange(StatusObserver::CONNECTED);
}

void JavascriptSignalStrategy::StartSession(
    cricket::SessionManager* session_manager) {
  session_start_request_.reset(
      new SessionStartRequest(CreateIqRequest(), session_manager));
  session_start_request_->Run();
}

void JavascriptSignalStrategy::EndSession() {
  if (xmpp_proxy_) {
    xmpp_proxy_->DetachCallback();
  }
  xmpp_proxy_ = NULL;
}

void JavascriptSignalStrategy::AttachXmppProxy(
    scoped_refptr<XmppProxy> xmpp_proxy) {
  xmpp_proxy_ = xmpp_proxy;
  xmpp_proxy_->AttachCallback(iq_registry_.AsWeakPtr());
}

JavascriptIqRequest* JavascriptSignalStrategy::CreateIqRequest() {
  return new JavascriptIqRequest(&iq_registry_, xmpp_proxy_);
}

JingleClient::JingleClient(JingleThread* thread,
                           SignalStrategy* signal_strategy,
                           PortAllocatorSessionFactory* session_factory,
                           Callback* callback)
    : thread_(thread),
      state_(START),
      initialized_(false),
      closed_(false),
      initialized_finished_(false),
      callback_(callback),
      signal_strategy_(signal_strategy),
      port_allocator_session_factory_(session_factory) {
}

JingleClient::JingleClient(JingleThread* thread,
                           SignalStrategy* signal_strategy,
                           talk_base::NetworkManager* network_manager,
                           talk_base::PacketSocketFactory* socket_factory,
                           PortAllocatorSessionFactory* session_factory,
                           Callback* callback)
    : thread_(thread),
      state_(START),
      initialized_(false),
      closed_(false),
      initialized_finished_(false),
      callback_(callback),
      signal_strategy_(signal_strategy),
      network_manager_(network_manager),
      socket_factory_(socket_factory),
      port_allocator_session_factory_(session_factory) {
}

JingleClient::~JingleClient() {
  base::AutoLock auto_lock(state_lock_);
  DCHECK(!initialized_ || closed_);
}

void JingleClient::Init() {
  {
    base::AutoLock auto_lock(state_lock_);
    DCHECK(!initialized_ && !closed_);
    initialized_ = true;
  }

  message_loop()->PostTask(
      FROM_HERE, NewRunnableMethod(this, &JingleClient::DoInitialize));
}

void JingleClient::DoInitialize() {
  DCHECK_EQ(message_loop(), MessageLoop::current());

  if (!network_manager_.get()) {
    VLOG(1) << "Creating talk_base::NetworkManager.";
    network_manager_.reset(new talk_base::NetworkManager());
  }
  if (!socket_factory_.get()) {
    VLOG(1) << "Creating talk_base::BasicPacketSocketFactory.";
    socket_factory_.reset(new talk_base::BasicPacketSocketFactory(
        talk_base::Thread::Current()));
  }

  port_allocator_.reset(
      new remoting::HttpPortAllocator(
          network_manager_.get(), socket_factory_.get(),
          port_allocator_session_factory_.get(), "transp2"));

  // TODO(ajwong): The strategy needs a "start" command or something.  Right
  // now, Init() implicitly starts processing events.  Thus, we must have the
  // other fields of JingleClient initialized first, otherwise the state-change
  // may occur and callback into class before we're done initializing.
  signal_strategy_->Init(this);

  jingle_info_request_.reset(
      new JingleInfoRequest(signal_strategy_->CreateIqRequest()));
  jingle_info_request_->SetCallback(
      NewCallback(this, &JingleClient::OnJingleInfo));
  jingle_info_request_->Run(
      NewRunnableMethod(this, &JingleClient::DoStartSession));
}

void JingleClient::DoStartSession() {
  session_manager_.reset(
      new cricket::SessionManager(port_allocator_.get()));
  signal_strategy_->StartSession(session_manager_.get());

  // TODO(ajwong): Major hack to synchronize state change logic.  Since the Xmpp
  // connection starts first, it move the state to CONNECTED before we've gotten
  // the jingle stun and relay information. Thus, we have to delay signaling
  // until now.  There is a parallel if to disable signaling in the
  // OnStateChange logic.
  initialized_finished_ = true;
  if (!closed_ && state_ == CONNECTED) {
    callback_->OnStateChange(this, state_);
  }
}

void JingleClient::Close() {
  Close(NULL);
}

void JingleClient::Close(Task* closed_task) {
  {
    base::AutoLock auto_lock(state_lock_);
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

  session_manager_.reset();
  signal_strategy_->EndSession();
  // TODO(ajwong): SignalStrategy should drop all resources at EndSession().
  signal_strategy_ = NULL;

  if (closed_task_.get()) {
    closed_task_->Run();
    closed_task_.reset();
  }
}

std::string JingleClient::GetFullJid() {
  base::AutoLock auto_lock(jid_lock_);
  return full_jid_;
}

IqRequest* JingleClient::CreateIqRequest() {
  return signal_strategy_->CreateIqRequest();
}

MessageLoop* JingleClient::message_loop() {
  return thread_->message_loop();
}

cricket::SessionManager* JingleClient::session_manager() {
  DCHECK_EQ(message_loop(), MessageLoop::current());
  return session_manager_.get();
}

void JingleClient::OnStateChange(State new_state) {
  if (new_state != state_) {
    state_ = new_state;
    {
      // We have to have the lock held, otherwise we cannot be sure that
      // the client hasn't been closed when we call the callback.
      base::AutoLock auto_lock(state_lock_);
      if (!closed_) {
        // TODO(ajwong): HACK! remove this.  See DoStartSession() for details.
        //
        // If state is connected, only signal if initialized_finished_ is also
        // finished.
        if (state_ != CONNECTED || initialized_finished_) {
          callback_->OnStateChange(this, new_state);
        }
      }
    }
  }
}

void JingleClient::OnJidChange(const std::string& full_jid) {
  base::AutoLock auto_lock(jid_lock_);
  full_jid_ = full_jid;
}

void JingleClient::OnJingleInfo(
    const std::string& token,
    const std::vector<std::string>& relay_hosts,
    const std::vector<talk_base::SocketAddress>& stun_hosts) {
  if (port_allocator_.get()) {
    // TODO(ajwong): Avoid string processing if log-level is low.
    std::string stun_servers;
    for (size_t i = 0; i < stun_hosts.size(); ++i) {
      stun_servers += stun_hosts[i].ToString() + "; ";
    }
    LOG(INFO) << "Configuring with relay token: " << token
              << ", relays: " << JoinString(relay_hosts, ';')
              << ", stun: " << stun_servers;
    port_allocator_->SetRelayToken(token);
    port_allocator_->SetStunHosts(stun_hosts);
    port_allocator_->SetRelayHosts(relay_hosts);
  } else {
    LOG(INFO) << "Jingle info found but no port allocator.";
  }
}

}  // namespace remoting
