// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A server specific QuicSession subclass.

#ifndef NET_TOOLS_QUIC_QUIC_SERVER_SESSION_H_
#define NET_TOOLS_QUIC_QUIC_SERVER_SESSION_H_

#include <set>
#include <vector>

#include "base/hash_tables.h"
#include "net/quic/quic_crypto_server_stream.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_session.h"

namespace net {

class QuicConnection;
class ReliableQuicStream;

// An interface from the session to the entity owning the session.
// This lets the session notify its owner (the Dispatcher) when the connection
// is closed.
class QuicSessionOwner {
 public:
  virtual ~QuicSessionOwner() {}

  virtual void OnConnectionClose(QuicGuid guid, QuicErrorCode error) = 0;
};

class QuicServerSession : public QuicSession {
 public:
  QuicServerSession(QuicConnection *connection, QuicSessionOwner* owner);

  // Override the base class to notify the owner of the connection close.
  virtual void ConnectionClose(QuicErrorCode error, bool from_peer) OVERRIDE;

  virtual ~QuicServerSession();

 protected:
  // QuicSession methods:
  virtual ReliableQuicStream* CreateIncomingReliableStream(
      QuicStreamId id) OVERRIDE;
  virtual ReliableQuicStream* CreateOutgoingReliableStream() OVERRIDE;
  virtual QuicCryptoServerStream* GetCryptoStream() OVERRIDE;

  // If we should create an incoming stream, returns true. Otherwise
  // does error handling, including communicating the error to the client and
  // possibly closing the connection, and returns false.
  virtual bool ShouldCreateIncomingReliableStream(QuicStreamId id);

 private:
  QuicCryptoServerStream crypto_stream_;
  QuicSessionOwner* owner_;

  DISALLOW_COPY_AND_ASSIGN(QuicServerSession);
};

}  // namespace net

#endif  // NET_TOOLS_QUIC_QUIC_SERVER_SESSION_H_
