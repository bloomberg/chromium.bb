// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_auth_handler.h"

#include <unistd.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
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
  explicit SecurityKeyAuthHandlerLinux(
      scoped_refptr<base::SingleThreadTaskRunner> file_task_runner);
  ~SecurityKeyAuthHandlerLinux() override;

 private:
  typedef std::map<int, std::unique_ptr<SecurityKeySocket>> ActiveSockets;

  // SecurityKeyAuthHandler interface.
  void CreateSecurityKeyConnection() override;
  bool IsValidConnectionId(int security_key_connection_id) const override;
  void SendClientResponse(int security_key_connection_id,
                          const std::string& response) override;
  void SendErrorAndCloseConnection(int security_key_connection_id) override;
  void SetSendMessageCallback(const SendMessageCallback& callback) override;
  size_t GetActiveConnectionCountForTest() const override;
  void SetRequestTimeoutForTest(base::TimeDelta timeout) override;

  // Sets up the socket used for accepting new connections.
  void CreateSocket();

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
  int last_connection_id_ = 0;

  // Sockets by connection id used to process gnubbyd requests.
  ActiveSockets active_sockets_;

  // Used to perform blocking File IO.
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;

  // Timeout used for a request.
  base::TimeDelta request_timeout_;

  base::WeakPtrFactory<SecurityKeyAuthHandlerLinux> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SecurityKeyAuthHandlerLinux);
};

std::unique_ptr<SecurityKeyAuthHandler> SecurityKeyAuthHandler::Create(
    ClientSessionDetails* client_session_details,
    const SendMessageCallback& send_message_callback,
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner) {
  std::unique_ptr<SecurityKeyAuthHandler> auth_handler(
      new SecurityKeyAuthHandlerLinux(file_task_runner));
  auth_handler->SetSendMessageCallback(send_message_callback);
  return auth_handler;
}

void SecurityKeyAuthHandler::SetSecurityKeySocketName(
    const base::FilePath& security_key_socket_name) {
  g_security_key_socket_name.Get() = security_key_socket_name;
}

SecurityKeyAuthHandlerLinux::SecurityKeyAuthHandlerLinux(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner)
    : file_task_runner_(file_task_runner),
      request_timeout_(
          base::TimeDelta::FromSeconds(kDefaultRequestTimeoutSeconds)),
      weak_factory_(this) {}

SecurityKeyAuthHandlerLinux::~SecurityKeyAuthHandlerLinux() {}

void SecurityKeyAuthHandlerLinux::CreateSecurityKeyConnection() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!g_security_key_socket_name.Get().empty());

  // We need to run the DeleteFile method on |file_task_runner_| as it is a
  // blocking function call which cannot be run on the main thread.  Once
  // that task has completed, the main thread will be called back and we will
  // resume setting up our security key auth socket there.
  file_task_runner_->PostTaskAndReply(
      FROM_HERE, base::Bind(base::IgnoreResult(&base::DeleteFile),
                            g_security_key_socket_name.Get(), false),
      base::Bind(&SecurityKeyAuthHandlerLinux::CreateSocket,
                 weak_factory_.GetWeakPtr()));
}

void SecurityKeyAuthHandlerLinux::CreateSocket() {
  DCHECK(thread_checker_.CalledOnValidThread());
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
  DCHECK(thread_checker_.CalledOnValidThread());
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
  active_sockets_[security_key_connection_id] = base::WrapUnique(socket);
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
