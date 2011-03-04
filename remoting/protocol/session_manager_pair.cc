// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/session_manager_pair.h"

#include "base/logging.h"
#include "remoting/jingle_glue/jingle_thread.h"
#include "third_party/libjingle/source/talk/base/network.h"
#include "third_party/libjingle/source/talk/base/basicpacketsocketfactory.h"
#include "third_party/libjingle/source/talk/p2p/base/sessionmanager.h"
#include "third_party/libjingle/source/talk/p2p/client/basicportallocator.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

namespace remoting {

const char SessionManagerPair::kHostJid[] = "host1@gmail.com/123";
const char SessionManagerPair::kClientJid[] = "host2@gmail.com/321";

SessionManagerPair::SessionManagerPair(JingleThread* thread)
    : message_loop_(thread->message_loop()) {
}

SessionManagerPair::~SessionManagerPair() {}

void SessionManagerPair::Init() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  network_manager_.reset(new talk_base::NetworkManager());
  socket_factory_.reset(new talk_base::BasicPacketSocketFactory(
      talk_base::Thread::Current()));

  cricket::BasicPortAllocator* port_allocator =
      new cricket::BasicPortAllocator(network_manager_.get(),
                                      socket_factory_.get());
  port_allocator_.reset(port_allocator);

  host_session_manager_.reset(new cricket::SessionManager(port_allocator));
  host_session_manager_->SignalOutgoingMessage.connect(
      this, &SessionManagerPair::ProcessMessage);
  host_session_manager_->SignalRequestSignaling.connect(
      host_session_manager_.get(), &cricket::SessionManager::OnSignalingReady);

  client_session_manager_.reset(new cricket::SessionManager(port_allocator));
  client_session_manager_->SignalOutgoingMessage.connect(
      this, &SessionManagerPair::ProcessMessage);
  client_session_manager_->SignalRequestSignaling.connect(
      client_session_manager_.get(),
      &cricket::SessionManager::OnSignalingReady);
}

cricket::SessionManager* SessionManagerPair::host_session_manager() {
  return host_session_manager_.get();
}

cricket::SessionManager* SessionManagerPair::client_session_manager() {
  return client_session_manager_.get();
}

void SessionManagerPair::ProcessMessage(cricket::SessionManager* manager,
                                        const buzz::XmlElement* stanza) {
  message_loop_->PostTask(
      FROM_HERE, NewRunnableMethod(this, &SessionManagerPair::DoProcessMessage,
                                   manager, new buzz::XmlElement(*stanza)));
}

void SessionManagerPair::DoProcessMessage(cricket::SessionManager* manager,
                                          buzz::XmlElement* stanza) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK(manager == host_session_manager_.get() ||
         manager == client_session_manager_.get());
  buzz::QName from_attr("", "from");
  buzz::QName to_attr("", "to");
  std::string to = stanza->Attr(to_attr);
  if (to == kHostJid) {
    DCHECK(manager == client_session_manager_.get());
    stanza->SetAttr(from_attr, kClientJid);
    DeliverMessage(host_session_manager_.get(), stanza);
  } else if (to == kClientJid) {
    DCHECK(manager == host_session_manager_.get());
    stanza->SetAttr(from_attr, kHostJid);
    DeliverMessage(client_session_manager_.get(), stanza);
  } else {
    LOG(ERROR) << "Dropping stanza sent to unknown jid " << to;
  }
  delete stanza;
}

void SessionManagerPair::DeliverMessage(cricket::SessionManager* to,
                                        buzz::XmlElement* stanza) {
  if (to->IsSessionMessage(stanza)) {
    to->OnIncomingMessage(stanza);
  }
}


}  // namespace remoting
