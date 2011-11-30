// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_JINGLE_SESSION_MANAGER_H_
#define REMOTING_PROTOCOL_JINGLE_SESSION_MANAGER_H_

#include <list>
#include <string>

#include "base/memory/ref_counted.h"
#include "remoting/protocol/content_description.h"
#include "remoting/protocol/jingle_session.h"
#include "remoting/protocol/session_manager.h"
#include "third_party/libjingle/source/talk/p2p/base/session.h"
#include "third_party/libjingle/source/talk/p2p/base/sessionclient.h"

namespace cricket {
class HttpPortAllocator;
class PortAllocator;
class SessionManager;
}  // namespace cricket

namespace remoting {

class JingleInfoRequest;
class JingleSignalingConnector;

namespace protocol {

// This class implements SessionClient for Chromoting sessions. It acts as a
// server that accepts chromoting connections and can also make new connections
// to other hosts.
class JingleSessionManager
    : public SessionManager,
      public cricket::SessionClient {
 public:
  virtual ~JingleSessionManager();

  JingleSessionManager(base::MessageLoopProxy* message_loop);

  // SessionManager interface.
  virtual void Init(const std::string& local_jid,
                    SignalStrategy* signal_strategy,
                    Listener* listener,
                    bool allow_nat_traversal) OVERRIDE;
  virtual Session* Connect(
      const std::string& host_jid,
      Authenticator* authenticator,
      CandidateSessionConfig* config,
      const Session::StateChangeCallback& state_change_callback) OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void set_authenticator_factory(
      AuthenticatorFactory* authenticator_factory) OVERRIDE;

  // cricket::SessionClient interface.
  virtual void OnSessionCreate(cricket::Session* cricket_session,
                               bool received_initiate) OVERRIDE;
  virtual void OnSessionDestroy(cricket::Session* cricket_session) OVERRIDE;

  virtual bool ParseContent(cricket::SignalingProtocol protocol,
                            const buzz::XmlElement* elem,
                            const cricket::ContentDescription** content,
                            cricket::ParseError* error) OVERRIDE;
  virtual bool WriteContent(cricket::SignalingProtocol protocol,
                            const cricket::ContentDescription* content,
                            buzz::XmlElement** elem,
                            cricket::WriteError* error) OVERRIDE;

 private:
  friend class JingleSession;

  // Called by JingleSession when a new connection is initiated.
  SessionManager::IncomingSessionResponse AcceptConnection(
      JingleSession* jingle_session);

  // Creates authenticator for incoming session. Returns NULL if
  // authenticator cannot be created, e.g. if |auth_message| is
  // invalid. Caller reatins ownership of |auth_message| and must
  // accept ownership of the result.
  Authenticator* CreateAuthenticator(const std::string& jid,
                                     const buzz::XmlElement* auth_message);

  // Called by JingleSession when it is being destroyed.
  void SessionDestroyed(JingleSession* jingle_session);

  // Callback for JingleInfoRequest.
  void OnJingleInfo(
      const std::string& token,
      const std::vector<std::string>& relay_hosts,
      const std::vector<talk_base::SocketAddress>& stun_hosts);

  scoped_refptr<base::MessageLoopProxy> message_loop_;

  scoped_ptr<talk_base::NetworkManager> network_manager_;
  scoped_ptr<talk_base::PacketSocketFactory> socket_factory_;

  std::string local_jid_;  // Full jid for the local side of the session.
  SignalStrategy* signal_strategy_;
  scoped_ptr<AuthenticatorFactory> authenticator_factory_;
  Listener* listener_;
  bool allow_nat_traversal_;

  scoped_ptr<cricket::PortAllocator> port_allocator_;
  cricket::HttpPortAllocator* http_port_allocator_;
  scoped_ptr<cricket::SessionManager> cricket_session_manager_;
  scoped_ptr<JingleInfoRequest> jingle_info_request_;
  scoped_ptr<JingleSignalingConnector> jingle_signaling_connector_;

  bool closed_;

  std::list<JingleSession*> sessions_;

  ScopedRunnableMethodFactory<JingleSessionManager> task_factory_;

  DISALLOW_COPY_AND_ASSIGN(JingleSessionManager);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_JINGLE_SESSION_MANAGER_H_
