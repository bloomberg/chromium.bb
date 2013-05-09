// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/embedded_test_server.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "net/test/embedded_test_server/http_connection.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/tools/fetch/http_listen_socket.h"

namespace net {
namespace test_server {

namespace {

const int kPort = 8040;
const char kIp[] = "127.0.0.1";
const int kRetries = 10;

// Callback to handle requests with default predefined response for requests
// matching the address |url|.
scoped_ptr<HttpResponse> HandleDefaultRequest(const GURL& url,
                                              const HttpResponse& response,
                                              const HttpRequest& request) {
  const GURL request_url = url.Resolve(request.relative_url);
  if (url.path() != request_url.path())
    return scoped_ptr<HttpResponse>(NULL);
  return scoped_ptr<HttpResponse>(new HttpResponse(response));
}

}  // namespace

HttpListenSocket::HttpListenSocket(const SocketDescriptor socket_descriptor,
                                   StreamListenSocket::Delegate* delegate)
    : TCPListenSocket(socket_descriptor, delegate) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void HttpListenSocket::Listen() {
  DCHECK(thread_checker_.CalledOnValidThread());
  TCPListenSocket::Listen();
}

HttpListenSocket::~HttpListenSocket() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

EmbeddedTestServer::EmbeddedTestServer(
    const scoped_refptr<base::SingleThreadTaskRunner>& io_thread)
    : io_thread_(io_thread),
      port_(-1),
      weak_factory_(this) {
  DCHECK(io_thread_);
  DCHECK(thread_checker_.CalledOnValidThread());
}

EmbeddedTestServer::~EmbeddedTestServer() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

bool EmbeddedTestServer::InitializeAndWaitUntilReady() {
  DCHECK(thread_checker_.CalledOnValidThread());

  base::RunLoop run_loop;
  if (!io_thread_->PostTaskAndReply(
          FROM_HERE,
          base::Bind(&EmbeddedTestServer::InitializeOnIOThread,
                     base::Unretained(this)),
          run_loop.QuitClosure())) {
    return false;
  }
  run_loop.Run();

  return Started();
}

bool EmbeddedTestServer::ShutdownAndWaitUntilComplete() {
  DCHECK(thread_checker_.CalledOnValidThread());

  base::RunLoop run_loop;
  if (!io_thread_->PostTaskAndReply(
          FROM_HERE,
          base::Bind(&EmbeddedTestServer::ShutdownOnIOThread,
                     base::Unretained(this)),
          run_loop.QuitClosure())) {
    return false;
  }
  run_loop.Run();

  return true;
}

void EmbeddedTestServer::InitializeOnIOThread() {
  DCHECK(io_thread_->BelongsToCurrentThread());
  DCHECK(!Started());

  int retries_left = kRetries + 1;
  int try_port = kPort;

  while (retries_left > 0) {
    SocketDescriptor socket_descriptor = TCPListenSocket::CreateAndBind(
        kIp,
        try_port);
    if (socket_descriptor != TCPListenSocket::kInvalidSocket) {
      listen_socket_ = new HttpListenSocket(socket_descriptor, this);
      listen_socket_->Listen();
      base_url_ = GURL(base::StringPrintf("http://%s:%d", kIp, try_port));
      port_ = try_port;
      break;
    }
    retries_left--;
    try_port++;
  }
}

void EmbeddedTestServer::ShutdownOnIOThread() {
  DCHECK(io_thread_->BelongsToCurrentThread());

  listen_socket_ = NULL;  // Release the listen socket.
  STLDeleteContainerPairSecondPointers(connections_.begin(),
                                       connections_.end());
  connections_.clear();
}

void EmbeddedTestServer::HandleRequest(HttpConnection* connection,
                               scoped_ptr<HttpRequest> request) {
  DCHECK(io_thread_->BelongsToCurrentThread());

  for (size_t i = 0; i < request_handlers_.size(); ++i) {
    scoped_ptr<HttpResponse> response =
        request_handlers_[i].Run(*request.get());
    if (response.get()) {
      connection->SendResponse(response.Pass());
      return;
    }
  }

  LOG(WARNING) << "Request not handled. Returning 404: "
               << request->relative_url;
  scoped_ptr<HttpResponse> not_found_response(new HttpResponse());
  not_found_response->set_code(NOT_FOUND);
  connection->SendResponse(not_found_response.Pass());

  // Drop the connection, since we do not support multiple requests per
  // connection.
  connections_.erase(connection->socket_.get());
  delete connection;
}

GURL EmbeddedTestServer::GetURL(const std::string& relative_url) const {
  DCHECK(StartsWithASCII(relative_url, "/", true /* case_sensitive */))
      << relative_url;
  return base_url_.Resolve(relative_url);
}

void EmbeddedTestServer::RegisterRequestHandler(
    const HandleRequestCallback& callback) {
  request_handlers_.push_back(callback);
}

void EmbeddedTestServer::DidAccept(StreamListenSocket* server,
                           StreamListenSocket* connection) {
  DCHECK(io_thread_->BelongsToCurrentThread());

  HttpConnection* http_connection = new HttpConnection(
      connection,
      base::Bind(&EmbeddedTestServer::HandleRequest,
                 weak_factory_.GetWeakPtr()));
  connections_[connection] = http_connection;
}

void EmbeddedTestServer::DidRead(StreamListenSocket* connection,
                         const char* data,
                         int length) {
  DCHECK(io_thread_->BelongsToCurrentThread());

  HttpConnection* http_connection = FindConnection(connection);
  if (http_connection == NULL) {
    LOG(WARNING) << "Unknown connection.";
    return;
  }
  http_connection->ReceiveData(std::string(data, length));
}

void EmbeddedTestServer::DidClose(StreamListenSocket* connection) {
  DCHECK(io_thread_->BelongsToCurrentThread());

  HttpConnection* http_connection = FindConnection(connection);
  if (http_connection == NULL) {
    LOG(WARNING) << "Unknown connection.";
    return;
  }
  delete http_connection;
  connections_.erase(connection);
}

HttpConnection* EmbeddedTestServer::FindConnection(
    StreamListenSocket* socket) {
  DCHECK(io_thread_->BelongsToCurrentThread());

  std::map<StreamListenSocket*, HttpConnection*>::iterator it =
      connections_.find(socket);
  if (it == connections_.end()) {
    return NULL;
  }
  return it->second;
}

}  // namespace test_server
}  // namespace net
