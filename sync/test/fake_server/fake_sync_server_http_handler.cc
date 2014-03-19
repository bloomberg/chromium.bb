// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/server/http_server_request_info.h"
#include "net/server/http_server_response_info.h"
#include "net/socket/tcp_listen_socket.h"
#include "sync/test/fake_server/fake_sync_server_http_handler.h"

namespace fake_server {

FakeSyncServerHttpHandler::FakeSyncServerHttpHandler() {
}

FakeSyncServerHttpHandler::~FakeSyncServerHttpHandler() {
}

// Note that this must be called from within an IO MessageLoop because it
// initializes a net::HttpServer.
void FakeSyncServerHttpHandler::Start() {
  VLOG(1) << "Starting web server";
  net::TCPListenSocketFactory factory("0.0.0.0", 0);
  server_ = new net::HttpServer(factory, this);
  net::IPEndPoint address;
  int error = server_->GetLocalAddress(&address);
  CHECK_EQ(net::OK, error) << base::StringPrintf(
        "Error %d while trying to choose a port: %s",
        error,
        net::ErrorToString(error));

  LOG(INFO) << base::StringPrintf("Listening on port %d", address.port());
}

void FakeSyncServerHttpHandler::OnHttpRequest(
    int connection_id,
    const net::HttpServerRequestInfo& info) {

  // Hand http requests over to the sync FakeServer implementation to process
  VLOG(1) << "Request path: " << info.path;
  int response_code = -1;
  std::string response;
  int server_return_value = fake_sync_server_.HandleCommand(info.data,
                                                            &response_code,
                                                            &response);
  if (server_return_value == 0) {
    // A '0' error code indicates a successful request to FakeServer
    server_->Send(connection_id, net::HttpStatusCode(response_code),
                  response, "text/html");
    VLOG(1) << "Sync response sent: " << response;
  } else {
    // The FakeServer returned a non-0 error code.
    std::string error_message = base::StringPrintf(
        "Error processing sync request: error code %d. (%s)",
        server_return_value,
        net::ErrorToString(server_return_value));
    server_->Send500(connection_id, error_message);
    LOG(ERROR) << error_message;
  }
}

void FakeSyncServerHttpHandler::OnWebSocketRequest(
      int connection_id,
      const net::HttpServerRequestInfo& info) {}

void FakeSyncServerHttpHandler::OnWebSocketMessage(int connection_id,
                                  const std::string& data) {}

void FakeSyncServerHttpHandler::OnClose(int connection_id) {}

} // namespace fake_server
