// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_QUIC_TEST_TOOLS_QUIC_TEST_SERVER_H_
#define NET_TOOLS_QUIC_TEST_TOOLS_QUIC_TEST_SERVER_H_

#include "net/tools/quic/quic_dispatcher.h"
#include "net/tools/quic/quic_server.h"
#include "net/tools/quic/quic_server_session.h"

namespace net {

namespace tools {

namespace test {

// A test server which enables easy creation of custom QuicServerSessions
//
// Eventually this may be extended to allow custom QuicConnections etc.
class QuicTestServer : public QuicServer {
 public:
  typedef std::function<QuicServerSession*(const QuicConfig& config,
                                           QuicConnection* connection,
                                           QuicServerSessionVisitor* visitor)>
      SessionCreationFunction;

  // Create a custom dispatcher which creates custom sessions.
  QuicDispatcher* CreateQuicDispatcher() override;

  void SetSessionCreator(const SessionCreationFunction& function);
};

// Useful test sessions for the QuicTestServer.

// Test session which sends a GOAWAY immedaitely on creation, before crypto
// credentials have even been established.
class ImmediateGoAwaySession : public QuicServerSession {
 public:
  ImmediateGoAwaySession(const QuicConfig& config,
                         QuicConnection* connection,
                         QuicServerSessionVisitor* visitor);
};

}  // namespace test

}  // namespace tools

}  // namespace net

#endif  // NET_TOOLS_QUIC_TEST_TOOLS_QUIC_TEST_SERVER_H_
