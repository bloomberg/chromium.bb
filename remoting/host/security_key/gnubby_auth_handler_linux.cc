// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/gnubby_auth_handler_linux.h"

#include <stdint.h>
#include <unistd.h>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/socket/unix_domain_server_socket_posix.h"
#include "remoting/base/logging.h"
#include "remoting/host/security_key/gnubby_socket.h"

namespace {

const int64_t kDefaultRequestTimeoutSeconds = 60;

// The name of the socket to listen for gnubby requests on.
base::LazyInstance<base::FilePath>::Leaky g_gnubby_socket_name =
    LAZY_INSTANCE_INITIALIZER;

// Socket authentication function that only allows connections from callers with
// the current uid.
bool MatchUid(const net::UnixDomainServerSocket::Credentials& credentials) {
  bool allowed = credentials.user_id == getuid();
  if (!allowed)
    HOST_LOG << "Refused socket connection from uid " << credentials.user_id;
  return allowed;
}

// Returns the command code (the first byte of the data) if it exists, or -1 if
// the data is empty.
unsigned int GetCommandCode(const std::string& data) {
  return data.empty() ? -1 : static_cast<unsigned int>(data[0]);
}

}  // namespace

namespace remoting {

scoped_ptr<GnubbyAuthHandler> GnubbyAuthHandler::Create(
    const SendMessageCallback& callback) {
  scoped_ptr<GnubbyAuthHandler> auth_handler(new GnubbyAuthHandlerLinux());
  auth_handler->SetSendMessageCallback(callback);
  return auth_handler;
}

void GnubbyAuthHandler::SetGnubbySocketName(
    const base::FilePath& gnubby_socket_name) {
  g_gnubby_socket_name.Get() = gnubby_socket_name;
}

GnubbyAuthHandlerLinux::GnubbyAuthHandlerLinux()
    : last_connection_id_(0),
      request_timeout_(
          base::TimeDelta::FromSeconds(kDefaultRequestTimeoutSeconds)) {}

GnubbyAuthHandlerLinux::~GnubbyAuthHandlerLinux() {
  STLDeleteValues(&active_sockets_);
}

void GnubbyAuthHandlerLinux::CreateGnubbyConnection() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!g_gnubby_socket_name.Get().empty());

  {
    // DeleteFile() is a blocking operation, but so is creation of the unix
    // socket below. Consider moving this class to a different thread if this
    // causes any problems. See crbug.com/509807.
    // TODO(joedow): Since this code now runs as a host extension, we should
    //               perform our IO on a separate thread.
    base::ThreadRestrictions::ScopedAllowIO allow_io;

    // If the file already exists, a socket in use error is returned.
    base::DeleteFile(g_gnubby_socket_name.Get(), false);
  }

  HOST_LOG << "Listening for gnubby requests on "
           << g_gnubby_socket_name.Get().value();

  auth_socket_.reset(
      new net::UnixDomainServerSocket(base::Bind(MatchUid), false));
  int rv = auth_socket_->BindAndListen(g_gnubby_socket_name.Get().value(),
                                       /*backlog=*/1);
  if (rv != net::OK) {
    LOG(ERROR) << "Failed to open socket for gnubby requests: '" << rv << "'";
    return;
  }
  DoAccept();
}

bool GnubbyAuthHandlerLinux::IsValidConnectionId(
    int gnubby_connection_id) const {
  return GetSocketForConnectionId(gnubby_connection_id) !=
         active_sockets_.end();
}

void GnubbyAuthHandlerLinux::SendClientResponse(int gnubby_connection_id,
                                                const std::string& response) {
  ActiveSockets::const_iterator iter =
      GetSocketForConnectionId(gnubby_connection_id);
  if (iter != active_sockets_.end()) {
    iter->second->SendResponse(response);
  } else {
    LOG(WARNING) << "Unknown gnubby-auth data connection: '"
                 << gnubby_connection_id << "'";
  }
}

void GnubbyAuthHandlerLinux::SendErrorAndCloseConnection(
    int gnubby_connection_id) {
  ActiveSockets::const_iterator iter =
      GetSocketForConnectionId(gnubby_connection_id);
  if (iter != active_sockets_.end()) {
    HOST_LOG << "Sending gnubby error";
    SendErrorAndCloseActiveSocket(iter);
  } else {
    LOG(WARNING) << "Unknown gnubby-auth data connection: '"
                 << gnubby_connection_id << "'";
  }
}

void GnubbyAuthHandlerLinux::SetSendMessageCallback(
    const SendMessageCallback& callback) {
  send_message_callback_ = callback;
}

size_t GnubbyAuthHandlerLinux::GetActiveSocketsMapSizeForTest() const {
  return active_sockets_.size();
}

void GnubbyAuthHandlerLinux::SetRequestTimeoutForTest(
    const base::TimeDelta& timeout) {
  request_timeout_ = timeout;
}

void GnubbyAuthHandlerLinux::DoAccept() {
  int result = auth_socket_->Accept(
      &accept_socket_,
      base::Bind(&GnubbyAuthHandlerLinux::OnAccepted, base::Unretained(this)));
  if (result != net::ERR_IO_PENDING)
    OnAccepted(result);
}

void GnubbyAuthHandlerLinux::OnAccepted(int result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(net::ERR_IO_PENDING, result);

  if (result < 0) {
    LOG(ERROR) << "Error in accepting a new connection";
    return;
  }

  int gnubby_connection_id = ++last_connection_id_;
  GnubbySocket* socket = new GnubbySocket(
      std::move(accept_socket_), request_timeout_,
      base::Bind(&GnubbyAuthHandlerLinux::RequestTimedOut,
                 base::Unretained(this), gnubby_connection_id));
  active_sockets_[gnubby_connection_id] = socket;
  socket->StartReadingRequest(
      base::Bind(&GnubbyAuthHandlerLinux::OnReadComplete,
                 base::Unretained(this), gnubby_connection_id));

  // Continue accepting new connections.
  DoAccept();
}

void GnubbyAuthHandlerLinux::OnReadComplete(int gnubby_connection_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  ActiveSockets::const_iterator iter =
      active_sockets_.find(gnubby_connection_id);
  DCHECK(iter != active_sockets_.end());
  std::string request_data;
  if (!iter->second->GetAndClearRequestData(&request_data)) {
    SendErrorAndCloseActiveSocket(iter);
    return;
  }

  HOST_LOG << "Received gnubby request: " << GetCommandCode(request_data);
  send_message_callback_.Run(gnubby_connection_id, request_data);

  iter->second->StartReadingRequest(
      base::Bind(&GnubbyAuthHandlerLinux::OnReadComplete,
                 base::Unretained(this), gnubby_connection_id));
}

GnubbyAuthHandlerLinux::ActiveSockets::const_iterator
GnubbyAuthHandlerLinux::GetSocketForConnectionId(
    int gnubby_connection_id) const {
  return active_sockets_.find(gnubby_connection_id);
}

void GnubbyAuthHandlerLinux::SendErrorAndCloseActiveSocket(
    const ActiveSockets::const_iterator& iter) {
  iter->second->SendSshError();
  delete iter->second;
  active_sockets_.erase(iter);
}

void GnubbyAuthHandlerLinux::RequestTimedOut(int gnubby_connection_id) {
  HOST_LOG << "Gnubby request timed out";
  ActiveSockets::const_iterator iter =
      active_sockets_.find(gnubby_connection_id);
  if (iter != active_sockets_.end())
    SendErrorAndCloseActiveSocket(iter);
}

}  // namespace remoting
