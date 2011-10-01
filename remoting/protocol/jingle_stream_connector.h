// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_JINGLE_STREAM_CONNECTOR_H_
#define REMOTING_PROTOCOL_JINGLE_STREAM_CONNECTOR_H_

#include "base/memory/scoped_ptr.h"
#include "net/base/completion_callback.h"
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/jingle_channel_connector.h"
#include "remoting/protocol/session.h"

namespace cricket {
class TransportChannel;
}  // namespace cricket

namespace jingle_glue {
class TransportChannelSocketAdapter;
}  // namespace jingle_glue

namespace net {
class CertVerifier;
class StreamSocket;
class SSLClientSocket;
class SSLServerSocket;
}  // namespace net

namespace remoting {
namespace protocol {

class JingleSession;

// JingleStreamConnector creates the named datagram channel in the supplied
// JingleSession, and uses PseudoTcp to turn it into a stream channel.  Within
// the stream channel SSL is used to secure the protocol stream.  Finally, the
// initiator authenticates the channel to the recipient by sending a digest
// based on a secret shared by the two parties, and keying material derived
// from the SSL session's master secret and nonces.
class JingleStreamConnector : public JingleChannelConnector {
 public:
  JingleStreamConnector(JingleSession* session,
                        const std::string& name,
                        const Session::StreamChannelCallback& callback);
  virtual ~JingleStreamConnector();

  // Starts connection process for the channel. |local_private_key| is
  // owned by the caller, and must exist until this object is
  // destroyed.
  virtual void Connect(bool initiator,
                       const std::string& local_cert,
                       const std::string& remote_cert,
                       crypto::RSAPrivateKey* local_private_key,
                       cricket::TransportChannel* raw_channel) OVERRIDE;

 private:
  bool EstablishTCPConnection(net::Socket* socket);
  void OnTCPConnect(int result);

  bool EstablishSSLConnection();
  void OnSSLConnect(int result);

  void AuthenticateChannel();
  void OnAuthenticationDone(ChannelAuthenticator::Result result);

  void NotifyDone(net::StreamSocket* socket);
  void NotifyError();

  JingleSession* session_;
  std::string name_;
  Session::StreamChannelCallback callback_;

  bool initiator_;
  std::string local_cert_;
  std::string remote_cert_;
  crypto::RSAPrivateKey* local_private_key_;

  cricket::TransportChannel* raw_channel_;
  scoped_ptr<net::StreamSocket> socket_;

  // TODO(wez): Ugly up-casts needed so we can fetch SSL keying material.
  net::SSLClientSocket* ssl_client_socket_;
  net::SSLServerSocket* ssl_server_socket_;

  // Used to verify the certificate received in SSLClientSocket.
  scoped_ptr<net::CertVerifier> cert_verifier_;

  scoped_ptr<ChannelAuthenticator> authenticator_;

  // Callback called by the TCP and SSL layers.
  net::OldCompletionCallbackImpl<JingleStreamConnector> tcp_connect_callback_;
  net::OldCompletionCallbackImpl<JingleStreamConnector> ssl_connect_callback_;

  DISALLOW_COPY_AND_ASSIGN(JingleStreamConnector);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_JINGLE_STREAM_CONNECTOR_H_
