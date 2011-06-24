// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/jingle_glue/jingle_client.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "remoting/jingle_glue/http_port_allocator.h"
#include "remoting/jingle_glue/iq_request.h"
#include "remoting/jingle_glue/jingle_info_request.h"
#include "remoting/jingle_glue/jingle_thread.h"
#include "third_party/libjingle/source/talk/base/basicpacketsocketfactory.h"
#include "third_party/libjingle/source/talk/base/ssladapter.h"
#include "third_party/libjingle/source/talk/p2p/base/sessionmanager.h"
#include "third_party/libjingle/source/talk/p2p/base/transport.h"
#include "third_party/libjingle/source/talk/session/tunnel/tunnelsessionclient.h"

namespace remoting {

JingleClient::JingleClient(MessageLoop* message_loop,
                           SignalStrategy* signal_strategy,
                           talk_base::NetworkManager* network_manager,
                           talk_base::PacketSocketFactory* socket_factory,
                           PortAllocatorSessionFactory* session_factory,
                           Callback* callback)
    : enable_nat_traversing_(false),
      message_loop_(message_loop),
      state_(START),
      initialized_(false),
      closed_(false),
      initialized_finished_(false),
      callback_(callback),
      signal_strategy_(signal_strategy),
      network_manager_(network_manager),
      socket_factory_(socket_factory),
      port_allocator_session_factory_(session_factory) {
  DCHECK(message_loop_);
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

  message_loop_->PostTask(
      FROM_HERE, NewRunnableMethod(this, &JingleClient::DoInitialize));
}

void JingleClient::DoInitialize() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

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
  if (!enable_nat_traversing_) {
    port_allocator_->set_flags(cricket::PORTALLOCATOR_DISABLE_STUN |
                               cricket::PORTALLOCATOR_DISABLE_RELAY);
  }

  // TODO(ajwong): The strategy needs a "start" command or something.  Right
  // now, Init() implicitly starts processing events.  Thus, we must have the
  // other fields of JingleClient initialized first, otherwise the state-change
  // may occur and callback into class before we're done initializing.
  signal_strategy_->Init(this);

  if (enable_nat_traversing_) {
    jingle_info_request_.reset(
        new JingleInfoRequest(signal_strategy_->CreateIqRequest()));
    jingle_info_request_->SetCallback(
        NewCallback(this, &JingleClient::OnJingleInfo));
    jingle_info_request_->Run(
        NewRunnableMethod(this, &JingleClient::DoStartSession));
  } else {
    DoStartSession();
  }
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

void JingleClient::Close(const base::Closure& closed_task) {
  {
    base::AutoLock auto_lock(state_lock_);
    // If the client is already closed then don't close again.
    if (closed_) {
      message_loop_->PostTask(FROM_HERE, closed_task);
      return;
    }
  }

  message_loop_->PostTask(
      FROM_HERE, NewRunnableMethod(this, &JingleClient::DoClose, closed_task));
}

void JingleClient::DoClose(const base::Closure& closed_task) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  session_manager_.reset();
  if (signal_strategy_) {
    signal_strategy_->EndSession();
    // TODO(ajwong): SignalStrategy should drop all resources at EndSession().
    signal_strategy_ = NULL;
  }

  closed_ = true;
  closed_task.Run();
}

std::string JingleClient::GetFullJid() {
  base::AutoLock auto_lock(jid_lock_);
  return full_jid_;
}

IqRequest* JingleClient::CreateIqRequest() {
  return signal_strategy_->CreateIqRequest();
}

MessageLoop* JingleClient::message_loop() {
  return message_loop_;
}

cricket::SessionManager* JingleClient::session_manager() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
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
