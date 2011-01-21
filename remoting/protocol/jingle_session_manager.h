// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_JINGLE_SESSION_MANAGER_H_
#define REMOTING_PROTOCOL_JINGLE_SESSION_MANAGER_H_

#include <list>
#include <string>

#include "base/ref_counted.h"
#include "net/base/x509_certificate.h"
#include "remoting/protocol/jingle_session.h"
#include "remoting/protocol/session_manager.h"
#include "third_party/libjingle/source/talk/p2p/base/session.h"
#include "third_party/libjingle/source/talk/p2p/base/sessionclient.h"
#include "third_party/libjingle/source/talk/p2p/base/sessiondescription.h"

class MessageLoop;

namespace base {
class RSAPrivateKey;
}  // namespace base

namespace cricket {
class SessionManager;
}  // namespace cricket

namespace remoting {

class JingleThread;

namespace protocol {

// ContentDescription used for chromoting sessions. It contains the information
// from the content description stanza in the session intialization handshake.
//
// This class also provides a type abstraction so that the Chromotocol Session
// interface does not need to depend on libjingle.
class ContentDescription : public cricket::ContentDescription {
 public:
  explicit ContentDescription(const CandidateSessionConfig* config,
                              const std::string& auth_token,
                              scoped_refptr<net::X509Certificate> certificate);
  ~ContentDescription();

  const CandidateSessionConfig* config() const {
    return candidate_config_.get();
  }

  const std::string& auth_token() const { return auth_token_; }

  scoped_refptr<net::X509Certificate> certificate() const {
    return certificate_;
  }

 private:
  scoped_ptr<const CandidateSessionConfig> candidate_config_;

  // This may contain the initiating, or the accepting token depending on
  // context.
  std::string auth_token_;

  scoped_refptr<net::X509Certificate> certificate_;
};

// This class implements SessionClient for Chromoting sessions. It acts as a
// server that accepts chromoting connections and can also make new connections
// to other hosts.
class JingleSessionManager
    : public protocol::SessionManager,
      public cricket::SessionClient {
 public:
  explicit JingleSessionManager(remoting::JingleThread* jingle_thread);

  // Initializes the session client. Doesn't accept ownership of the
  // |session_manager|. Close() must be called _before_ the |session_manager|
  // is destroyed.
  virtual void Init(const std::string& local_jid,
                    cricket::SessionManager* cricket_session_manager,
                    IncomingSessionCallback* incoming_session_callback);

  // SessionManager interface.
  virtual scoped_refptr<protocol::Session> Connect(
      const std::string& jid,
      const std::string& client_token,
      CandidateSessionConfig* candidate_config,
      protocol::Session::StateChangeCallback* state_change_callback);
  virtual void Close(Task* closed_task);

  void set_allow_local_ips(bool allow_local_ips);

  // cricket::SessionClient interface.
  virtual void OnSessionCreate(cricket::Session* cricket_session,
                               bool received_initiate);
  virtual void OnSessionDestroy(cricket::Session* cricket_session);

  virtual bool ParseContent(cricket::SignalingProtocol protocol,
                            const buzz::XmlElement* elem,
                            const cricket::ContentDescription** content,
                            cricket::ParseError* error);
  virtual bool WriteContent(cricket::SignalingProtocol protocol,
                            const cricket::ContentDescription* content,
                            buzz::XmlElement** elem,
                            cricket::WriteError* error);

  // Set the certificate and private key if they are provided externally.
  // TODO(hclam): Combine these two methods.
  virtual void SetCertificate(net::X509Certificate* certificate);
  virtual void SetPrivateKey(base::RSAPrivateKey* private_key);

 protected:
  virtual ~JingleSessionManager();

 private:
  friend class JingleSession;

  // The jingle thread used by this object.
  JingleThread* jingle_thread();

  // Message loop that corresponds to jingle_thread().
  MessageLoop* message_loop();

  // Called by JingleChromotocolConnection when a new connection is initiated.
  void AcceptConnection(JingleSession* jingle_session,
                        cricket::Session* cricket_session);

  void DoConnect(
      scoped_refptr<JingleSession> jingle_session,
      const std::string& host_jid,
      const std::string& client_token,
      protocol::Session::StateChangeCallback* state_change_callback);

  // Creates outgoing session description for an incoming session.
  cricket::SessionDescription* CreateSessionDescription(
      const CandidateSessionConfig* candidate_config,
      const std::string& auth_token,
      scoped_refptr<net::X509Certificate> certificate);

  std::string local_jid_;  // Full jid for the local side of the session.
  JingleThread* jingle_thread_;
  cricket::SessionManager* cricket_session_manager_;
  scoped_ptr<IncomingSessionCallback> incoming_session_callback_;
  bool allow_local_ips_;
  bool closed_;

  std::list<scoped_refptr<JingleSession> > sessions_;

  scoped_refptr<net::X509Certificate> certificate_;
  scoped_ptr<base::RSAPrivateKey> private_key_;

  DISALLOW_COPY_AND_ASSIGN(JingleSessionManager);
};

}  // namespace protocol

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_JINGLE_SESSION_MANAGER_H_
