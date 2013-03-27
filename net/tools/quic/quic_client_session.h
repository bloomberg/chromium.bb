// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A client specific QuicSession subclass.

#ifndef NET_TOOLS_QUIC_QUIC_CLIENT_SESSION_H_
#define NET_TOOLS_QUIC_QUIC_CLIENT_SESSION_H_

#include <string>

#include "net/quic/quic_crypto_client_stream.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_session.h"
#include "net/tools/quic/quic_reliable_client_stream.h"

namespace net {

class QuicConnection;
class QuicReliableClientStream;
class ReliableQuicStream;

class QuicClientSession : public QuicSession {
 public:
  QuicClientSession(const std::string& server_hostname,
                    QuicConnection* connection);
  virtual ~QuicClientSession();

  // QuicSession methods:
  virtual QuicReliableClientStream* CreateOutgoingReliableStream() OVERRIDE;
  virtual QuicCryptoClientStream* GetCryptoStream() OVERRIDE;

  // Performs a crypto handshake with the server. Returns true if the crypto
  // handshake is started successfully.
  bool CryptoConnect();

 protected:
  // QuicSession methods:
  virtual ReliableQuicStream* CreateIncomingReliableStream(
      QuicStreamId id) OVERRIDE;

 private:
  QuicCryptoClientStream crypto_stream_;

  DISALLOW_COPY_AND_ASSIGN(QuicClientSession);
};

}  // namespace net

#endif  // NET_TOOLS_QUIC_QUIC_CLIENT_SESSION_H_
