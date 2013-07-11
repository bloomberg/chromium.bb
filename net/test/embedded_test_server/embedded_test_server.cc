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
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
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

class CustomHttpResponse : public HttpResponse {
 public:
  CustomHttpResponse(const std::string& headers, const std::string& contents)
      : headers_(headers), contents_(contents) {
  }

  virtual std::string ToResponseString() const OVERRIDE {
    return headers_ + "\r\n" + contents_;
  }

 private:
  std::string headers_;
  std::string contents_;

  DISALLOW_COPY_AND_ASSIGN(CustomHttpResponse);
};

// Handles |request| by serving a file from under |server_root|.
scoped_ptr<HttpResponse> HandleFileRequest(
    const base::FilePath& server_root,
    const HttpRequest& request) {
  // This is a test-only server. Ignore I/O thread restrictions.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  // Trim the first byte ('/').
  std::string request_path(request.relative_url.substr(1));

  // Remove the query string if present.
  size_t query_pos = request_path.find('?');
  if (query_pos != std::string::npos)
    request_path = request_path.substr(0, query_pos);

  base::FilePath file_path(server_root.AppendASCII(request_path));
  std::string file_contents;
  if (!file_util::ReadFileToString(file_path, &file_contents))
    return scoped_ptr<HttpResponse>();

  base::FilePath headers_path(
      file_path.AddExtension(FILE_PATH_LITERAL("mock-http-headers")));

  if (base::PathExists(headers_path)) {
    std::string headers_contents;
    if (!file_util::ReadFileToString(headers_path, &headers_contents))
      return scoped_ptr<HttpResponse>();

    scoped_ptr<CustomHttpResponse> http_response(
        new CustomHttpResponse(headers_contents, file_contents));
    return http_response.PassAs<HttpResponse>();
  }

  scoped_ptr<BasicHttpResponse> http_response(new BasicHttpResponse);
  http_response->set_code(HTTP_OK);
  http_response->set_content(file_contents);
  return http_response.PassAs<HttpResponse>();
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
  DCHECK(io_thread_.get());
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

  bool request_handled = false;

  for (size_t i = 0; i < request_handlers_.size(); ++i) {
    scoped_ptr<HttpResponse> response =
        request_handlers_[i].Run(*request.get());
    if (response.get()) {
      connection->SendResponse(response.Pass());
      request_handled = true;
      break;
    }
  }

  if (!request_handled) {
    LOG(WARNING) << "Request not handled. Returning 404: "
                 << request->relative_url;
    scoped_ptr<BasicHttpResponse> not_found_response(new BasicHttpResponse);
    not_found_response->set_code(HTTP_NOT_FOUND);
    connection->SendResponse(
        not_found_response.PassAs<HttpResponse>());
  }

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
