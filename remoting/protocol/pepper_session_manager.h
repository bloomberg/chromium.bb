// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PEPPER_SESSION_MANAGER_H_
#define REMOTING_PROTOCOL_PEPPER_SESSION_MANAGER_H_

#include <map>
#include <list>
#include <string>

#include "base/memory/ref_counted.h"
#include "net/base/x509_certificate.h"
#include "remoting/jingle_glue/signal_strategy.h"
#include "remoting/protocol/pepper_channel.h"
#include "remoting/protocol/session_manager.h"
#include "remoting/protocol/transport_config.h"

namespace pp {
class Instance;
}  // namespace pp

namespace buzz {
class XmlElement;
}  // namespace buzz

namespace talk_base {
class SocketAddress;
}  // namespace talk_base

namespace remoting {

class JingleInfoRequest;

namespace protocol {

struct JingleMessage;
struct JingleMessageReply;
class PepperSession;

// This class implements SessionManager interface on top of the Pepper
// P2P Transport API.
class PepperSessionManager : public SessionManager,
                             public SignalStrategy::Listener {
 public:
  explicit PepperSessionManager(pp::Instance* pp_instance);
  virtual ~PepperSessionManager();

  // SessionManager interface.
  virtual void Init(const std::string& local_jid,
                    SignalStrategy* signal_strategy,
                    SessionManager::Listener* listener,
                    crypto::RSAPrivateKey* private_key,
                    const std::string& certificate,
                    bool allow_nat_traversal) OVERRIDE;
  virtual Session* Connect(
      const std::string& host_jid,
      const std::string& host_public_key,
      const std::string& client_token,
      CandidateSessionConfig* config,
      Session::StateChangeCallback* state_change_callback) OVERRIDE;
  virtual void Close() OVERRIDE;

  // SignalStrategy::Listener interface.
  virtual bool OnIncomingStanza(const buzz::XmlElement* stanza) OVERRIDE;

 private:
  friend class PepperSession;

  typedef std::map<std::string, PepperSession*> SessionsMap;

  void OnJingleInfo(
      const std::string& relay_token,
      const std::vector<std::string>& relay_hosts,
      const std::vector<talk_base::SocketAddress>& stun_hosts);

  IqRequest* CreateIqRequest();
  void SendReply(const buzz::XmlElement* original_stanza,
                 const JingleMessageReply& reply);

  // Called by PepperSession when it is being destroyed.
  void SessionDestroyed(PepperSession* session);

  pp::Instance* pp_instance_;

  std::string local_jid_;
  SignalStrategy* signal_strategy_;
  SessionManager::Listener* listener_;
  scoped_ptr<crypto::RSAPrivateKey> private_key_;
  std::string certificate_;
  bool allow_nat_traversal_;

  TransportConfig transport_config_;

  scoped_ptr<JingleInfoRequest> jingle_info_request_;

  SessionsMap sessions_;

  DISALLOW_COPY_AND_ASSIGN(PepperSessionManager);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_PEPPER_SESSION_MANAGER_H_
