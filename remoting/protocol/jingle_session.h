// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_JINGLE_SESSION_H_
#define REMOTING_PROTOCOL_JINGLE_SESSION_H_

#include "base/memory/ref_counted.h"
#include "crypto/rsa_private_key.h"
#include "net/base/completion_callback.h"
#include "remoting/protocol/session.h"
#include "third_party/libjingle/source/talk/base/sigslot.h"
#include "third_party/libjingle/source/talk/p2p/base/session.h"

namespace jingle_glue {
class PseudoTcpAdapter;
class StreamSocketAdapter;
class TransportChannelSocketAdapter;
}  // namespace jingle_glue

namespace net {
class CertVerifier;
class ClientSocketFactory;
class Socket;
class StreamSocket;
class X509Certificate;
}  // namespace net

namespace remoting {

namespace protocol {

class JingleSessionManager;
class SocketWrapper;

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
      crypto::RSAPrivateKey* key);

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
                crypto::RSAPrivateKey* key);
  virtual ~JingleSession();

  // Called by JingleSessionManager.
  void set_candidate_config(const CandidateSessionConfig* candidate_config);
  scoped_refptr<net::X509Certificate> server_certificate() const;
  void Init(cricket::Session* cricket_session);

  // Close all the channels and terminate the session.
  void CloseInternal(int result, bool failed);

  bool HasSession(cricket::Session* cricket_session);
  cricket::Session* ReleaseSession();

  // Initialize the session configuration from a received connection response
  // stanza.
  bool InitializeConfigFromDescription(
    const cricket::SessionDescription* description);

  // Configures channels and calls InitializeSSL().
  void InitializeChannels();

  // Initialize PseudoTCP + SSL on each of the video, control and input
  // channels. The channels must have been created before this is called.
  bool InitializeSSL();

  // Helper method to create and initialize PseudoTCP + SSL socket on
  // top of the provided |channel|. The resultant SSL socket is
  // written to |ssl_socket|. Return true if successful.
  bool EstablishSSLConnection(net::Socket* channel,
                              scoped_ptr<SocketWrapper>* ssl_socket);

  // Used for Session.SignalState sigslot.
  void OnSessionState(cricket::BaseSession* session,
                      cricket::BaseSession::State state);
  // Used for Session.SignalError sigslot.
  void OnSessionError(cricket::BaseSession* session,
                      cricket::BaseSession::Error error);

  void OnInitiate();
  void OnAccept();
  void OnTerminate();

  void OnConnect(int result);

  // Called by SSL socket to notify connect event.
  void OnSSLConnect(int result);

  void SetState(State new_state);

  // JingleSessionManager that created this session.
  scoped_refptr<JingleSessionManager> jingle_session_manager_;

  // Server certificate used in SSL connections.
  scoped_refptr<net::X509Certificate> server_cert_;

  // Private key used in SSL server sockets.
  scoped_ptr<crypto::RSAPrivateKey> key_;

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

  // |control_channel_| holds a channel until SSL socket is
  // created. After that |control_ssl_socket_| owns the channel. The
  // same is the case fo |event_channel_| and |video_channel_|.
  cricket::TransportChannel* raw_control_channel_;
  scoped_ptr<jingle_glue::TransportChannelSocketAdapter> control_channel_;
  scoped_ptr<SocketWrapper> control_ssl_socket_;

  cricket::TransportChannel* raw_event_channel_;
  scoped_ptr<jingle_glue::TransportChannelSocketAdapter> event_channel_;
  scoped_ptr<SocketWrapper> event_ssl_socket_;

  cricket::TransportChannel* raw_video_channel_;
  scoped_ptr<jingle_glue::TransportChannelSocketAdapter> video_channel_;
  scoped_ptr<SocketWrapper> video_ssl_socket_;

  // Count the number of SSL connections esblished.
  int ssl_connections_;

  // Used to verify the certificate received in SSLClientSocket.
  scoped_ptr<net::CertVerifier> cert_verifier_;

  cricket::TransportChannel* raw_video_rtp_channel_;
  scoped_ptr<jingle_glue::TransportChannelSocketAdapter> video_rtp_channel_;
  cricket::TransportChannel* raw_video_rtcp_channel_;
  scoped_ptr<jingle_glue::TransportChannelSocketAdapter> video_rtcp_channel_;

  // Callback called by the SSL layer.
  net::CompletionCallbackImpl<JingleSession> connect_callback_;
  net::CompletionCallbackImpl<JingleSession> ssl_connect_callback_;

  DISALLOW_COPY_AND_ASSIGN(JingleSession);
};

}  // namespace protocol

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_JINGLE_SESSION_H_
