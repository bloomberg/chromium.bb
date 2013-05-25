// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/embedded_test_server.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/test/embedded_test_server/http_connection.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/tools/fetch/http_listen_socket.h"

namespace net {
namespace test_server {

namespace {

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

// Handles |request| by serving a file from under |server_root|.
scoped_ptr<HttpResponse> HandleFileRequest(const base::FilePath& server_root,
                                           const HttpRequest& request) {
  // This is a test-only server. Ignore I/O thread restrictions.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  // Trim the first byte ('/').
  std::string request_path(request.relative_url.substr(1));

  std::string file_contents;
  if (!file_util::ReadFileToString(
          server_root.AppendASCII(request_path), &file_contents)) {
    return scoped_ptr<HttpResponse>(NULL);
  }

  scoped_ptr<HttpResponse> http_response(new HttpResponse);
  http_response->set_code(net::test_server::SUCCESS);
  http_response->set_content(file_contents);
  return http_response.Pass();
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

  if (Started() && !ShutdownAndWaitUntilComplete()) {
    LOG(ERROR) << "EmbeddedTestServer failed to shut down.";
  }
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

  return Started() && base_url_.is_valid();
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

  SocketDescriptor socket_descriptor =
      TCPListenSocket::CreateAndBindAnyPort("127.0.0.1", &port_);
  if (socket_descriptor == TCPListenSocket::kInvalidSocket)
    return;

  listen_socket_ = new HttpListenSocket(socket_descriptor, this);
  listen_socket_->Listen();

  IPEndPoint address;
  int result = listen_socket_->GetLocalAddress(&address);
  if (result == OK) {
    base_url_ = GURL(std::string("http://") + address.ToString());
  } else {
    LOG(ERROR) << "GetLocalAddress failed: " << ErrorToString(result);
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

void EmbeddedTestServer::ServeFilesFromDirectory(
    const base::FilePath& directory) {
  RegisterRequestHandler(base::Bind(&HandleFileRequest, directory));
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
