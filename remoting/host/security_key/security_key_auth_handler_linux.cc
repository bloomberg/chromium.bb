// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_auth_handler.h"

#include <stdint.h>
#include <unistd.h>

#include <memory>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"
#include "net/socket/unix_domain_server_socket_posix.h"
#include "remoting/base/logging.h"
#include "remoting/host/security_key/security_key_socket.h"

namespace {

const int64_t kDefaultRequestTimeoutSeconds = 60;

// The name of the socket to listen for security key requests on.
base::LazyInstance<base::FilePath>::Leaky g_security_key_socket_name =
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

class SecurityKeyAuthHandlerLinux : public SecurityKeyAuthHandler {
 public:
  SecurityKeyAuthHandlerLinux();
  ~SecurityKeyAuthHandlerLinux() override;

 private:
  typedef std::map<int, SecurityKeySocket*> ActiveSockets;

  // SecurityKeyAuthHandler interface.
  void CreateSecurityKeyConnection() override;
  bool IsValidConnectionId(int security_key_connection_id) const override;
  void SendClientResponse(int security_key_connection_id,
                          const std::string& response) override;
  void SendErrorAndCloseConnection(int security_key_connection_id) override;
  void SetSendMessageCallback(const SendMessageCallback& callback) override;
  size_t GetActiveConnectionCountForTest() const override;
  void SetRequestTimeoutForTest(base::TimeDelta timeout) override;

  // Starts listening for connection.
  void DoAccept();

  // Called when a connection is accepted.
  void OnAccepted(int result);

  // Called when a SecurityKeySocket has done reading.
  void OnReadComplete(int security_key_connection_id);

  // Gets an active socket iterator for |security_key_connection_id|.
  ActiveSockets::const_iterator GetSocketForConnectionId(
      int security_key_connection_id) const;

  // Send an error and closes an active socket.
  void SendErrorAndCloseActiveSocket(const ActiveSockets::const_iterator& iter);

  // A request timed out.
  void RequestTimedOut(int security_key_connection_id);

  // Ensures SecurityKeyAuthHandlerLinux methods are called on the same thread.
  base::ThreadChecker thread_checker_;

  // Socket used to listen for authorization requests.
  std::unique_ptr<net::UnixDomainServerSocket> auth_socket_;

  // A temporary holder for an accepted connection.
  std::unique_ptr<net::StreamSocket> accept_socket_;

  // Used to pass security key extension messages to the client.
  SendMessageCallback send_message_callback_;

  // The last assigned security key connection id.
  int last_connection_id_;

  // Sockets by connection id used to process gnubbyd requests.
  ActiveSockets active_sockets_;

  // Timeout used for a request.
  base::TimeDelta request_timeout_;

  DISALLOW_COPY_AND_ASSIGN(SecurityKeyAuthHandlerLinux);
};

std::unique_ptr<SecurityKeyAuthHandler> SecurityKeyAuthHandler::Create(
    ClientSessionDetails* client_session_details,
    const SendMessageCallback& send_message_callback) {
  std::unique_ptr<SecurityKeyAuthHandler> auth_handler(
      new SecurityKeyAuthHandlerLinux());
  auth_handler->SetSendMessageCallback(send_message_callback);
  return auth_handler;
}

void SecurityKeyAuthHandler::SetSecurityKeySocketName(
    const base::FilePath& security_key_socket_name) {
  g_security_key_socket_name.Get() = security_key_socket_name;
}

SecurityKeyAuthHandlerLinux::SecurityKeyAuthHandlerLinux()
    : last_connection_id_(0),
      request_timeout_(
          base::TimeDelta::FromSeconds(kDefaultRequestTimeoutSeconds)) {}

SecurityKeyAuthHandlerLinux::~SecurityKeyAuthHandlerLinux() {
  STLDeleteValues(&active_sockets_);
}

void SecurityKeyAuthHandlerLinux::CreateSecurityKeyConnection() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!g_security_key_socket_name.Get().empty());

  {
    // DeleteFile() is a blocking operation, but so is creation of the unix
    // socket below. Consider moving this class to a different thread if this
    // causes any problems. See crbug.com/509807.
    // TODO(joedow): Since this code now runs as a host extension, we should
    //               perform our IO on a separate thread: crbug.com/591739
    base::ThreadRestrictions::ScopedAllowIO allow_io;

    // If the file already exists, a socket in use error is returned.
    base::DeleteFile(g_security_key_socket_name.Get(), false);
  }

  HOST_LOG << "Listening for security key requests on "
           << g_security_key_socket_name.Get().value();

  auth_socket_.reset(
      new net::UnixDomainServerSocket(base::Bind(MatchUid), false));
  int rv = auth_socket_->BindAndListen(g_security_key_socket_name.Get().value(),
                                       /*backlog=*/1);
  if (rv != net::OK) {
    LOG(ERROR) << "Failed to open socket for auth requests: '" << rv << "'";
    return;
  }
  DoAccept();
}

bool SecurityKeyAuthHandlerLinux::IsValidConnectionId(
    int security_key_connection_id) const {
  return GetSocketForConnectionId(security_key_connection_id) !=
         active_sockets_.end();
}

void SecurityKeyAuthHandlerLinux::SendClientResponse(
    int security_key_connection_id,
    const std::string& response) {
  ActiveSockets::const_iterator iter =
      GetSocketForConnectionId(security_key_connection_id);
  if (iter != active_sockets_.end()) {
    iter->second->SendResponse(response);
  } else {
    LOG(WARNING) << "Unknown gnubby-auth data connection: '"
                 << security_key_connection_id << "'";
  }
}

void SecurityKeyAuthHandlerLinux::SendErrorAndCloseConnection(
    int security_key_connection_id) {
  ActiveSockets::const_iterator iter =
      GetSocketForConnectionId(security_key_connection_id);
  if (iter != active_sockets_.end()) {
    HOST_LOG << "Sending security key error";
    SendErrorAndCloseActiveSocket(iter);
  } else {
    LOG(WARNING) << "Unknown gnubby-auth data connection: '"
                 << security_key_connection_id << "'";
  }
}

void SecurityKeyAuthHandlerLinux::SetSendMessageCallback(
    const SendMessageCallback& callback) {
  send_message_callback_ = callback;
}

size_t SecurityKeyAuthHandlerLinux::GetActiveConnectionCountForTest() const {
  return active_sockets_.size();
}

void SecurityKeyAuthHandlerLinux::SetRequestTimeoutForTest(
    base::TimeDelta timeout) {
  request_timeout_ = timeout;
}

void SecurityKeyAuthHandlerLinux::DoAccept() {
  int result = auth_socket_->Accept(
      &accept_socket_, base::Bind(&SecurityKeyAuthHandlerLinux::OnAccepted,
                                  base::Unretained(this)));
  if (result != net::ERR_IO_PENDING)
    OnAccepted(result);
}

void SecurityKeyAuthHandlerLinux::OnAccepted(int result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(net::ERR_IO_PENDING, result);

  if (result < 0) {
    LOG(ERROR) << "Error in accepting a new connection";
    return;
  }

  int security_key_connection_id = ++last_connection_id_;
  SecurityKeySocket* socket = new SecurityKeySocket(
      std::move(accept_socket_), request_timeout_,
      base::Bind(&SecurityKeyAuthHandlerLinux::RequestTimedOut,
                 base::Unretained(this), security_key_connection_id));
  active_sockets_[security_key_connection_id] = socket;
  socket->StartReadingRequest(
      base::Bind(&SecurityKeyAuthHandlerLinux::OnReadComplete,
                 base::Unretained(this), security_key_connection_id));

  // Continue accepting new connections.
  DoAccept();
}

void SecurityKeyAuthHandlerLinux::OnReadComplete(
    int security_key_connection_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  ActiveSockets::const_iterator iter =
      active_sockets_.find(security_key_connection_id);
  DCHECK(iter != active_sockets_.end());
  std::string request_data;
  if (!iter->second->GetAndClearRequestData(&request_data)) {
    SendErrorAndCloseActiveSocket(iter);
    return;
  }

  HOST_LOG << "Received security key request: " << GetCommandCode(request_data);
  send_message_callback_.Run(security_key_connection_id, request_data);

  iter->second->StartReadingRequest(
      base::Bind(&SecurityKeyAuthHandlerLinux::OnReadComplete,
                 base::Unretained(this), security_key_connection_id));
}

SecurityKeyAuthHandlerLinux::ActiveSockets::const_iterator
SecurityKeyAuthHandlerLinux::GetSocketForConnectionId(
    int security_key_connection_id) const {
  return active_sockets_.find(security_key_connection_id);
}

void SecurityKeyAuthHandlerLinux::SendErrorAndCloseActiveSocket(
    const ActiveSockets::const_iterator& iter) {
  iter->second->SendSshError();
  delete iter->second;
  active_sockets_.erase(iter);
}

void SecurityKeyAuthHandlerLinux::RequestTimedOut(
    int security_key_connection_id) {
  HOST_LOG << "SecurityKey request timed out";
  ActiveSockets::const_iterator iter =
      active_sockets_.find(security_key_connection_id);
  if (iter != active_sockets_.end())
    SendErrorAndCloseActiveSocket(iter);
}

}  // namespace remoting
