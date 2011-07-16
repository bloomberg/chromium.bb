// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_JINGLE_STREAM_CONNECTOR_H_
#define REMOTING_PROTOCOL_JINGLE_STREAM_CONNECTOR_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/completion_callback.h"
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
}  // namespace net

namespace remoting {
namespace protocol {

class JingleSession;

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

  // Used to verify the certificate received in SSLClientSocket.
  scoped_ptr<net::CertVerifier> cert_verifier_;

  // Callback called by the TCP and SSL layers.
  net::CompletionCallbackImpl<JingleStreamConnector> tcp_connect_callback_;
  net::CompletionCallbackImpl<JingleStreamConnector> ssl_connect_callback_;

  DISALLOW_COPY_AND_ASSIGN(JingleStreamConnector);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_JINGLE_STREAM_CONNECTOR_H_
