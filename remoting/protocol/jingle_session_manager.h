// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_JINGLE_SESSION_MANAGER_H_
#define REMOTING_PROTOCOL_JINGLE_SESSION_MANAGER_H_

#include <list>
#include <map>
#include <string>

#include "base/memory/ref_counted.h"
#include "net/cert/x509_certificate.h"
#include "remoting/protocol/jingle_messages.h"
#include "remoting/protocol/session_manager.h"
#include "remoting/signaling/signal_strategy.h"

namespace pp {
class Instance;
}  // namespace pp

namespace buzz {
class XmlElement;
}  // namespace buzz

namespace rtc {
class SocketAddress;
}  // namespace rtc

namespace remoting {

class IqSender;

namespace protocol {

class JingleSession;
class TransportFactory;

// JingleSessionManager and JingleSession implement the subset of the
// Jingle protocol used in Chromoting. JingleSessionManager provides
// the protocol::SessionManager interface for accepting incoming and
// creating outgoing sessions.
class JingleSessionManager : public SessionManager,
                             public SignalStrategy::Listener {
 public:
  explicit JingleSessionManager(scoped_ptr<TransportFactory> transport_factory);
  virtual ~JingleSessionManager();

  // SessionManager interface.
  virtual void Init(SignalStrategy* signal_strategy,
                    SessionManager::Listener* listener) OVERRIDE;
  virtual scoped_ptr<Session> Connect(
      const std::string& host_jid,
      scoped_ptr<Authenticator> authenticator,
      scoped_ptr<CandidateSessionConfig> config) OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void set_authenticator_factory(
      scoped_ptr<AuthenticatorFactory> authenticator_factory) OVERRIDE;

  // SignalStrategy::Listener interface.
  virtual void OnSignalStrategyStateChange(
      SignalStrategy::State state) OVERRIDE;
  virtual bool OnSignalStrategyIncomingStanza(
      const buzz::XmlElement* stanza) OVERRIDE;

 private:
  friend class JingleSession;

  typedef std::map<std::string, JingleSession*> SessionsMap;

  void OnJingleInfo(
      const std::string& relay_token,
      const std::vector<std::string>& relay_hosts,
      const std::vector<rtc::SocketAddress>& stun_hosts);

  IqSender* iq_sender() { return iq_sender_.get(); }
  void SendReply(const buzz::XmlElement* original_stanza,
                 JingleMessageReply::ErrorType error);

  // Called by JingleSession when it is being destroyed.
  void SessionDestroyed(JingleSession* session);

  scoped_ptr<TransportFactory> transport_factory_;
  bool fetch_stun_relay_config_;

  SignalStrategy* signal_strategy_;
  scoped_ptr<AuthenticatorFactory> authenticator_factory_;
  scoped_ptr<IqSender> iq_sender_;
  SessionManager::Listener* listener_;

  bool ready_;

  SessionsMap sessions_;

  DISALLOW_COPY_AND_ASSIGN(JingleSessionManager);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_JINGLE_SESSION_MANAGER_H_
