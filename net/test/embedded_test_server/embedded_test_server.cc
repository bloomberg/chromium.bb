// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/embedded_test_server.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process_metrics.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"
#include "net/socket/tcp_server_socket.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"
#include "net/test/embedded_test_server/http_connection.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace net {
namespace test_server {

namespace {

class CustomHttpResponse : public HttpResponse {
 public:
  CustomHttpResponse(const std::string& headers, const std::string& contents)
      : headers_(headers), contents_(contents) {
  }

  std::string ToResponseString() const override {
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

  std::string relative_url(request.relative_url);
  // A proxy request will have an absolute path. Simulate the proxy by stripping
  // the scheme, host, and port.
  GURL relative_gurl(relative_url);
  if (relative_gurl.is_valid())
    relative_url = relative_gurl.PathForRequest();

  // Trim the first byte ('/').
  std::string request_path = relative_url.substr(1);

  // Remove the query string if present.
  size_t query_pos = request_path.find('?');
  if (query_pos != std::string::npos)
    request_path = request_path.substr(0, query_pos);

  base::FilePath file_path(server_root.AppendASCII(request_path));
  std::string file_contents;
  if (!base::ReadFileToString(file_path, &file_contents))
    return scoped_ptr<HttpResponse>();

  base::FilePath headers_path(
      file_path.AddExtension(FILE_PATH_LITERAL("mock-http-headers")));

  if (base::PathExists(headers_path)) {
    std::string headers_contents;
    if (!base::ReadFileToString(headers_path, &headers_contents))
      return scoped_ptr<HttpResponse>();

    scoped_ptr<CustomHttpResponse> http_response(
        new CustomHttpResponse(headers_contents, file_contents));
    return http_response.Pass();
  }

  scoped_ptr<BasicHttpResponse> http_response(new BasicHttpResponse);
  http_response->set_code(HTTP_OK);
  http_response->set_content(file_contents);
  return http_response.Pass();
}

}  // namespace

EmbeddedTestServer::EmbeddedTestServer()
    : connection_listener_(nullptr), port_(0) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

EmbeddedTestServer::~EmbeddedTestServer() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (Started() && !ShutdownAndWaitUntilComplete()) {
    LOG(ERROR) << "EmbeddedTestServer failed to shut down.";
  }
}

void EmbeddedTestServer::SetConnectionListener(
    EmbeddedTestServerConnectionListener* listener) {
  DCHECK(!Started());
  connection_listener_ = listener;
}

bool EmbeddedTestServer::InitializeAndWaitUntilReady() {
  bool success = InitializeAndListen();
  if (!success)
    return false;
  StartAcceptingConnections();
  return true;
}

bool EmbeddedTestServer::InitializeAndListen() {
  DCHECK(!Started());

  listen_socket_.reset(new TCPServerSocket(nullptr, NetLog::Source()));

  int result = listen_socket_->ListenWithAddressAndPort("127.0.0.1", 0, 10);
  if (result) {
    LOG(ERROR) << "Listen failed: " << ErrorToString(result);
    listen_socket_.reset();
    return false;
  }

  result = listen_socket_->GetLocalAddress(&local_endpoint_);
  if (result != OK) {
    LOG(ERROR) << "GetLocalAddress failed: " << ErrorToString(result);
    listen_socket_.reset();
    return false;
  }

  base_url_ = GURL(std::string("http://") + local_endpoint_.ToString());
  port_ = local_endpoint_.port();

  listen_socket_->DetachFromThread();
  return true;
}

void EmbeddedTestServer::StartAcceptingConnections() {
  DCHECK(!io_thread_.get());
  base::Thread::Options thread_options;
  thread_options.message_loop_type = base::MessageLoop::TYPE_IO;
  io_thread_.reset(new base::Thread("EmbeddedTestServer IO Thread"));
  CHECK(io_thread_->StartWithOptions(thread_options));
  CHECK(io_thread_->WaitUntilThreadStarted());

  io_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&EmbeddedTestServer::DoAcceptLoop, base::Unretained(this)));
}

bool EmbeddedTestServer::ShutdownAndWaitUntilComplete() {
  DCHECK(thread_checker_.CalledOnValidThread());

  return PostTaskToIOThreadAndWait(base::Bind(
      &EmbeddedTestServer::ShutdownOnIOThread, base::Unretained(this)));
}

void EmbeddedTestServer::ShutdownOnIOThread() {
  DCHECK(io_thread_->task_runner()->BelongsToCurrentThread());
  listen_socket_.reset();
  STLDeleteContainerPairSecondPointers(connections_.begin(),
                                       connections_.end());
  connections_.clear();
}

void EmbeddedTestServer::HandleRequest(HttpConnection* connection,
                                       scoped_ptr<HttpRequest> request) {
  DCHECK(io_thread_->task_runner()->BelongsToCurrentThread());

  scoped_ptr<HttpResponse> response;

  for (size_t i = 0; i < request_handlers_.size(); ++i) {
    response = request_handlers_[i].Run(*request);
    if (response)
      break;
  }

  if (!response) {
    LOG(WARNING) << "Request not handled. Returning 404: "
                 << request->relative_url;
    scoped_ptr<BasicHttpResponse> not_found_response(new BasicHttpResponse);
    not_found_response->set_code(HTTP_NOT_FOUND);
    response = not_found_response.Pass();
  }

  connection->SendResponse(response.Pass(),
                           base::Bind(&EmbeddedTestServer::DidClose,
                                      base::Unretained(this), connection));
}

GURL EmbeddedTestServer::GetURL(const std::string& relative_url) const {
  DCHECK(Started()) << "You must start the server first.";
  DCHECK(base::StartsWith(relative_url, "/", base::CompareCase::SENSITIVE))
      << relative_url;
  return base_url_.Resolve(relative_url);
}

GURL EmbeddedTestServer::GetURL(
    const std::string& hostname,
    const std::string& relative_url) const {
  GURL local_url = GetURL(relative_url);
  GURL::Replacements replace_host;
  replace_host.SetHostStr(hostname);
  return local_url.ReplaceComponents(replace_host);
}

bool EmbeddedTestServer::GetAddressList(AddressList* address_list) const {
  *address_list = AddressList(local_endpoint_);
  return true;
}

void EmbeddedTestServer::ServeFilesFromDirectory(
    const base::FilePath& directory) {
  RegisterRequestHandler(base::Bind(&HandleFileRequest, directory));
}

void EmbeddedTestServer::RegisterRequestHandler(
    const HandleRequestCallback& callback) {
  request_handlers_.push_back(callback);
}

void EmbeddedTestServer::DoAcceptLoop() {
  int rv = OK;
  while (rv == OK) {
    rv = listen_socket_->Accept(
        &accepted_socket_, base::Bind(&EmbeddedTestServer::OnAcceptCompleted,
                                      base::Unretained(this)));
    if (rv == ERR_IO_PENDING)
      return;
    HandleAcceptResult(accepted_socket_.Pass());
  }
}

void EmbeddedTestServer::OnAcceptCompleted(int rv) {
  DCHECK_NE(ERR_IO_PENDING, rv);
  HandleAcceptResult(accepted_socket_.Pass());
  DoAcceptLoop();
}

void EmbeddedTestServer::HandleAcceptResult(scoped_ptr<StreamSocket> socket) {
  DCHECK(io_thread_->task_runner()->BelongsToCurrentThread());
  if (connection_listener_)
    connection_listener_->AcceptedSocket(*socket);

  HttpConnection* http_connection = new HttpConnection(
      socket.Pass(),
      base::Bind(&EmbeddedTestServer::HandleRequest, base::Unretained(this)));
  connections_[http_connection->socket_.get()] = http_connection;

  ReadData(http_connection);
}

void EmbeddedTestServer::ReadData(HttpConnection* connection) {
  while (true) {
    int rv =
        connection->ReadData(base::Bind(&EmbeddedTestServer::OnReadCompleted,
                                        base::Unretained(this), connection));
    if (rv == ERR_IO_PENDING)
      return;
    if (!HandleReadResult(connection, rv))
      return;
  }
}

void EmbeddedTestServer::OnReadCompleted(HttpConnection* connection, int rv) {
  DCHECK_NE(ERR_IO_PENDING, rv);
  if (HandleReadResult(connection, rv))
    ReadData(connection);
}

bool EmbeddedTestServer::HandleReadResult(HttpConnection* connection, int rv) {
  DCHECK(io_thread_->task_runner()->BelongsToCurrentThread());
  if (connection_listener_)
    connection_listener_->ReadFromSocket(*connection->socket_);
  if (rv <= 0) {
    DidClose(connection);
    return false;
  }

  // Once a single complete request has been received, there is no further need
  // for the connection and it may be destroyed once the response has been sent.
  if (connection->ConsumeData(rv))
    return false;

  return true;
}

void EmbeddedTestServer::DidClose(HttpConnection* connection) {
  DCHECK(io_thread_->task_runner()->BelongsToCurrentThread());
  DCHECK(connection);
  DCHECK_EQ(1u, connections_.count(connection->socket_.get()));

  connections_.erase(connection->socket_.get());
  delete connection;
}

HttpConnection* EmbeddedTestServer::FindConnection(StreamSocket* socket) {
  DCHECK(io_thread_->task_runner()->BelongsToCurrentThread());

  std::map<StreamSocket*, HttpConnection*>::iterator it =
      connections_.find(socket);
  if (it == connections_.end()) {
    return NULL;
  }
  return it->second;
}

bool EmbeddedTestServer::PostTaskToIOThreadAndWait(
    const base::Closure& closure) {
  // Note that PostTaskAndReply below requires
  // base::ThreadTaskRunnerHandle::Get() to return a task runner for posting
  // the reply task. However, in order to make EmbeddedTestServer universally
  // usable, it needs to cope with the situation where it's running on a thread
  // on which a message loop is not (yet) available or as has been destroyed
  // already.
  //
  // To handle this situation, create temporary message loop to support the
  // PostTaskAndReply operation if the current thread as no message loop.
  scoped_ptr<base::MessageLoop> temporary_loop;
  if (!base::MessageLoop::current())
    temporary_loop.reset(new base::MessageLoop());

  base::RunLoop run_loop;
  if (!io_thread_->task_runner()->PostTaskAndReply(FROM_HERE, closure,
                                                   run_loop.QuitClosure())) {
    return false;
  }
  run_loop.Run();

  return true;
}

}  // namespace test_server
}  // namespace net
