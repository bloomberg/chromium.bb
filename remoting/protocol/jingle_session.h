// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_JINGLE_SESSION_H_
#define REMOTING_PROTOCOL_JINGLE_SESSION_H_

#include "base/crypto/rsa_private_key.h"
#include "base/ref_counted.h"
#include "net/base/completion_callback.h"
#include "remoting/protocol/session.h"
#include "third_party/libjingle/source/talk/base/sigslot.h"
#include "third_party/libjingle/source/talk/p2p/base/session.h"

namespace cricket {
class PseudoTcpChannel;
}  // namespace cricket

namespace net {
class CertVerifier;
class ClientSocket;
class ClientSocketFactory;
class Socket;
class X509Certificate;
}  // namespace net

namespace remoting {

class StreamSocketAdapter;
class TransportChannelSocketAdapter;

namespace protocol {

class JingleSessionManager;

// Implements protocol::Session that work over libjingle session (the
// cricket::Session object is passed to Init() method). Created
// by JingleSessionManager for incoming and outgoing connections.
class JingleSession : public protocol::Session,
                      public sigslot::has_slots<> {
 public:
  static const char kChromotingContentName[];

  // Create a JingleSession used in client mode. A server certificate is
  // required.
  static JingleSession* CreateClientSession(JingleSessionManager* manager);

  // Create a JingleSession used in server mode. A server certificate and
  // private key is provided. |key| is copied in the constructor.
  static JingleSession* CreateServerSession(
      JingleSessionManager* manager,
      scoped_refptr<net::X509Certificate> certificate,
      base::RSAPrivateKey* key);

  // Chromotocol Session interface.
  virtual void SetStateChangeCallback(StateChangeCallback* callback);

  virtual net::Socket* control_channel();
  virtual net::Socket* event_channel();
  virtual net::Socket* video_channel();

  virtual net::Socket* video_rtp_channel();
  virtual net::Socket* video_rtcp_channel();

  virtual const std::string& jid();
  virtual MessageLoop* message_loop();

  virtual const SessionConfig* config();
  virtual void set_config(const SessionConfig* config);

  virtual const std::string& initiator_token();
  virtual void set_initiator_token(const std::string& initiator_token);
  virtual const std::string& receiver_token();
  virtual void set_receiver_token(const std::string& receiver_token);

  // These fields are only set on the receiving side.
  virtual const CandidateSessionConfig* candidate_config();

  virtual void Close(Task* closed_task);

 private:
  friend class JingleSessionManager;

  JingleSession(JingleSessionManager* client,
                scoped_refptr<net::X509Certificate> server_cert,
                base::RSAPrivateKey* key);
  virtual ~JingleSession();

  // Called by JingleSessionManager.
  void set_candidate_config(const CandidateSessionConfig* candidate_config);
  scoped_refptr<net::X509Certificate> server_certificate() const;
  void Init(cricket::Session* cricket_session);

  // Close all the channels and terminate the session.
  void CloseInternal(int result, bool failed);

  bool HasSession(cricket::Session* cricket_session);
  cricket::Session* ReleaseSession();

  // Helper method to create and initialize SSL socket using the adapter
  // provided. The resultant SSL socket is written to |ssl_socket|.
  // Return true if successful.
  bool EstablishSSLConnection(net::ClientSocket* adapter,
                              scoped_ptr<net::Socket>* ssl_socket);

  // Used for Session.SignalState sigslot.
  void OnSessionState(cricket::BaseSession* session,
                      cricket::BaseSession::State state);
  // Used for Session.SignalError sigslot.
  void OnSessionError(cricket::BaseSession* session,
                      cricket::BaseSession::Error error);

  void OnInitiate();
  void OnAccept();
  void OnTerminate();

  // Called by SSL socket to notify connect event.
  void OnSSLConnect(int result);

  void SetState(State new_state);

  // JingleSessionManager that created this session.
  scoped_refptr<JingleSessionManager> jingle_session_manager_;

  // Server certificate used in SSL connections.
  scoped_refptr<net::X509Certificate> server_cert_;

  // Private key used in SSL server sockets.
  scoped_ptr<base::RSAPrivateKey> key_;

  State state_;
  scoped_ptr<StateChangeCallback> state_change_callback_;

  bool closed_;
  bool closing_;

  // JID of the other side. Set when the connection is initialized,
  // and never changed after that.
  std::string jid_;

  // The corresponding libjingle session.
  cricket::Session* cricket_session_;

  scoped_ptr<const SessionConfig> config_;

  std::string initiator_token_;
  std::string receiver_token_;

  // These data members are only set on the receiving side.
  scoped_ptr<const CandidateSessionConfig> candidate_config_;

  // A channel is the the base channel created by libjingle.
  // A channel adapter is used to convert a jingle channel to net::Socket.
  // A SSL socket is a wrapper over a net::Socket to provide SSL functionality.
  cricket::PseudoTcpChannel* control_channel_;
  scoped_ptr<StreamSocketAdapter> control_channel_adapter_;
  scoped_ptr<net::Socket> control_ssl_socket_;

  cricket::PseudoTcpChannel* event_channel_;
  scoped_ptr<StreamSocketAdapter> event_channel_adapter_;
  scoped_ptr<net::Socket> event_ssl_socket_;

  cricket::PseudoTcpChannel* video_channel_;
  scoped_ptr<StreamSocketAdapter> video_channel_adapter_;
  scoped_ptr<net::Socket> video_ssl_socket_;

  // Count the number of SSL connections esblished.
  int ssl_connections_;

  // Used to verify the certificate received in SSLClientSocket.
  scoped_ptr<net::CertVerifier> cert_verifier_;

  scoped_ptr<TransportChannelSocketAdapter> video_rtp_channel_;
  scoped_ptr<TransportChannelSocketAdapter> video_rtcp_channel_;

  // Callback called by the SSL layer.
  scoped_ptr<net::CompletionCallback> connect_callback_;

  DISALLOW_COPY_AND_ASSIGN(JingleSession);
};

}  // namespace protocol

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_JINGLE_SESSION_H_
