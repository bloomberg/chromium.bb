// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_TEST_FAKE_SERVER_FAKE_SERVER_HTTP_HANDLER_H_
#define SYNC_TEST_FAKE_SERVER_FAKE_SERVER_HTTP_HANDLER_H_

#include "net/server/http_server.h"
#include "sync/test/fake_server/fake_server.h"

namespace fake_server {

// An HTTP server that forwards requests to FakeServer, providing a quick way
// to test Sync client code.
class FakeSyncServerHttpHandler : public net::HttpServer::Delegate {
 public:
  FakeSyncServerHttpHandler();
  virtual ~FakeSyncServerHttpHandler();

  // Begin accepting HTTP requests. Must be called from an IO MessageLoop.
  void Start();

  // Overridden from net::HttpServer::Delegate.
  // HTTP requests are processed and WebSocket requests are currently
  // unimplemented.
  virtual void OnHttpRequest(int connection_id,
                             const net::HttpServerRequestInfo& info) OVERRIDE;
  virtual void OnWebSocketRequest(
      int connection_id,
      const net::HttpServerRequestInfo& info) OVERRIDE;

  virtual void OnWebSocketMessage(int connection_id,
                                  const std::string& data) OVERRIDE;

  virtual void OnClose(int connection_id) OVERRIDE;

 private:
  scoped_refptr<net::HttpServer> server_;

  fake_server::FakeServer fake_sync_server_;

  DISALLOW_COPY_AND_ASSIGN(FakeSyncServerHttpHandler);
};

}  // namespace fake_server

#endif  // SYNC_TEST_FAKE_SERVER_FAKE_SERVER_HTTP_HANDLER_H_
