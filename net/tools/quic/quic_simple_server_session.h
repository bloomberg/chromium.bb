// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A toy server specific QuicSession subclass.

#ifndef NET_TOOLS_QUIC_QUIC_SIMPLE_SERVER_SESSION_H_
#define NET_TOOLS_QUIC_QUIC_SIMPLE_SERVER_SESSION_H_

#include <stdint.h>

#include <set>
#include <string>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "net/quic/quic_crypto_server_stream.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_spdy_session.h"
#include "net/tools/quic/quic_server_session_base.h"
#include "net/tools/quic/quic_simple_server_stream.h"

namespace net {

class QuicBlockedWriterInterface;
class QuicConfig;
class QuicConnection;
class QuicCryptoServerConfig;
class ReliableQuicStream;

namespace tools {

namespace test {
class QuicSimpleServerSessionPeer;
}  // namespace test

class QuicSimpleServerSession : public QuicServerSessionBase {
 public:
  QuicSimpleServerSession(const QuicConfig& config,
                          QuicConnection* connection,
                          QuicServerSessionVisitor* visitor,
                          const QuicCryptoServerConfig* crypto_config);

  ~QuicSimpleServerSession() override;

 protected:
  // QuicSession methods:
  QuicSpdyStream* CreateIncomingDynamicStream(QuicStreamId id) override;
  QuicSimpleServerStream* CreateOutgoingDynamicStream(
      SpdyPriority priority) override;

  QuicCryptoServerStreamBase* CreateQuicCryptoServerStream(
      const QuicCryptoServerConfig* crypto_config) override;

 private:
  friend class test::QuicSimpleServerSessionPeer;

  DISALLOW_COPY_AND_ASSIGN(QuicSimpleServerSession);
};

}  // namespace tools
}  // namespace net

#endif  // NET_TOOLS_QUIC_QUIC_SERVER_SESSION_H_
