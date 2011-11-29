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
                    crypto::RSAPrivateKey* private_key,
                    const std::string& certificate,
                    bool allow_nat_traversal) OVERRIDE;
  virtual Session* Connect(
      const std::string& host_jid,
      const std::string& host_public_key,
      const std::string& client_token,
      CandidateSessionConfig* config,
      const Session::StateChangeCallback& state_change_callback) OVERRIDE;
  virtual void Close() OVERRIDE;

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

  // Called by JingleSession when a new connection is
  // initiated. Returns true if session is accepted.
  bool AcceptConnection(JingleSession* jingle_session,
                        cricket::Session* cricket_session);

  // Called by JingleSession when it is being destroyed.
  void SessionDestroyed(JingleSession* jingle_session);

  // Callback for JingleInfoRequest.
  void OnJingleInfo(
      const std::string& token,
      const std::vector<std::string>& relay_hosts,
      const std::vector<talk_base::SocketAddress>& stun_hosts);

  // Creates session description for outgoing session.
  static cricket::SessionDescription* CreateClientSessionDescription(
      const CandidateSessionConfig* candidate_config,
      const std::string& auth_token);
  // Creates session description for incoming session.
  static cricket::SessionDescription* CreateHostSessionDescription(
      const CandidateSessionConfig* candidate_config,
      const std::string& certificate);

  scoped_refptr<base::MessageLoopProxy> message_loop_;

  scoped_ptr<talk_base::NetworkManager> network_manager_;
  scoped_ptr<talk_base::PacketSocketFactory> socket_factory_;

  std::string local_jid_;  // Full jid for the local side of the session.
  SignalStrategy* signal_strategy_;
  Listener* listener_;
  std::string certificate_;
  scoped_ptr<crypto::RSAPrivateKey> private_key_;
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
