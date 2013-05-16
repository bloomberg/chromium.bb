// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_spdy_server_stream.h"

#include "base/strings/string_piece.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/tools/quic/test_tools/quic_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPiece;
using net::tools::test::MockConnection;
using net::test::MockSession;

namespace net {
namespace tools {
namespace test {
namespace {

class QuicSpdyServerStreamTest : public ::testing::Test {
 public:
  QuicSpdyServerStreamTest()
      : connection_(new MockConnection(1, IPEndPoint(), false)),
        session_(connection_, true),
        stream_(1, &session_) {
  }

  MockConnection* connection_;
  MockSession session_;
  QuicSpdyServerStream stream_;
};

TEST_F(QuicSpdyServerStreamTest, InvalidHeadersWithFin) {
  char arr[] = {
    0x00, 0x00, 0x00, 0x05,  // ....
    0x00, 0x00, 0x00, 0x05,  // ....
    0x3a, 0x68, 0x6f, 0x73,  // :hos
    0x74, 0x00, 0x00, 0x00,  // t...
    0x00, 0x00, 0x00, 0x00,  // ....
    0x07, 0x3a, 0x6d, 0x65,  // .:me
    0x74, 0x68, 0x6f, 0x64,  // thod
    0x00, 0x00, 0x00, 0x03,  // ....
    0x47, 0x45, 0x54, 0x00,  // GET.
    0x00, 0x00, 0x05, 0x3a,  // ...:
    0x70, 0x61, 0x74, 0x68,  // path
    0x00, 0x00, 0x00, 0x04,  // ....
    0x2f, 0x66, 0x6f, 0x6f,  // /foo
    0x00, 0x00, 0x00, 0x07,  // ....
    0x3a, 0x73, 0x63, 0x68,  // :sch
    0x65, 0x6d, 0x65, 0x00,  // eme.
    0x00, 0x00, 0x00, 0x00,  // ....
    0x00, 0x00, 0x08, 0x3a,  // ...:
    0x76, 0x65, 0x72, 0x73,  // vers
    '\x96', 0x6f, 0x6e, 0x00,  // <i(69)>on.
    0x00, 0x00, 0x08, 0x48,  // ...H
    0x54, 0x54, 0x50, 0x2f,  // TTP/
    0x31, 0x2e, 0x31,        // 1.1
  };
  QuicStreamFrame frame(1, true, 0, StringPiece(arr, arraysize(arr)));
  // Verify that we don't crash when we get a invalid headers in stream frame.
  stream_.OnStreamFrame(frame);
}

}  // namespace
}  // namespace test
}  // namespace tools
}  // namespace net
