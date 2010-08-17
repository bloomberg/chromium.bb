// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_UNITTEST_TEST_SERVER_H__
#define WEBKIT_GLUE_UNITTEST_TEST_SERVER_H__

#include "net/base/load_flags.h"
#include "net/test/test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/appcache/appcache_interfaces.h"

class UnittestTestServer : public net::TestServer {
 public:
  UnittestTestServer()
      : net::TestServer(net::TestServer::TYPE_HTTP,
                        FilePath(FILE_PATH_LITERAL("webkit/data"))) {
  }
};

#endif  // WEBKIT_GLUE_UNITTEST_TEST_SERVER_H__
