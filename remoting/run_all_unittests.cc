// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_suite.h"
#include "net/socket/ssl_server_socket.h"

int main(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);

  // Enable support for SSL server sockets, which must be done while
  // single-threaded.
  net::EnableSSLServerSockets();

  return test_suite.Run();
}
