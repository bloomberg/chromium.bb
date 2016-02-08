// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/gnubby_auth_handler_linux.h"

#include <stdint.h>
#include <unistd.h>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/socket/unix_domain_server_socket_posix.h"
#include "remoting/base/logging.h"
#include "remoting/host/security_key/gnubby_socket.h"
#include "remoting/proto/control.pb.h"
#include "remoting/protocol/client_stub.h"

namespace remoting {

namespace {

const char kConnectionId[] = "connectionId";
const char kControlMessage[] = "control";
const char kControlOption[] = "option";
const char kDataMessage[] = "data";
const char kDataPayload[] = "data";
const char kErrorMessage[] = "error";
const char kGnubbyAuthMessage[] = "gnubby-auth";
const char kGnubbyAuthV1[] = "auth-v1";
const char kMessageType[] = "type";

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

// Creates a string of byte data from a ListValue of numbers. Returns true if
// all of the list elements are numbers.
bool ConvertListValueToString(base::ListValue* bytes, std::string* out) {
  out->clear();

  unsigned int byte_count = bytes->GetSize();
  if (byte_count != 0) {
    out->reserve(byte_count);
    for (unsigned int i = 0; i < byte_count; i++) {
      int value;
      if (!bytes->GetInteger(i, &value))
        return false;
      out->push_back(static_cast<char>(value));
    }
  }
  return true;
}

}  // namespace

GnubbyAuthHandlerLinux::GnubbyAuthHandlerLinux(
    protocol::ClientStub* client_stub)
    : client_stub_(client_stub),
      last_connection_id_(0),
      request_timeout_(
          base::TimeDelta::FromSeconds(kDefaultRequestTimeoutSeconds)) {
  DCHECK(client_stub_);
}

GnubbyAuthHandlerLinux::~GnubbyAuthHandlerLinux() {
  STLDeleteValues(&active_sockets_);
}

// static
scoped_ptr<GnubbyAuthHandler> GnubbyAuthHandler::Create(
    protocol::ClientStub* client_stub) {
  return make_scoped_ptr(new GnubbyAuthHandlerLinux(client_stub));
}

// static
void GnubbyAuthHandler::SetGnubbySocketName(
    const base::FilePath& gnubby_socket_name) {
  g_gnubby_socket_name.Get() = gnubby_socket_name;
}

void GnubbyAuthHandlerLinux::DeliverClientMessage(const std::string& message) {
  DCHECK(CalledOnValidThread());

  scoped_ptr<base::Value> value = base::JSONReader::Read(message);
  base::DictionaryValue* client_message;
  if (value && value->GetAsDictionary(&client_message)) {
    std::string type;
    if (!client_message->GetString(kMessageType, &type)) {
      LOG(ERROR) << "Invalid gnubby-auth message";
      return;
    }

    if (type == kControlMessage) {
      std::string option;
      if (client_message->GetString(kControlOption, &option) &&
          option == kGnubbyAuthV1) {
        CreateAuthorizationSocket();
      } else {
        LOG(ERROR) << "Invalid gnubby-auth control option";
      }
    } else if (type == kDataMessage) {
      ActiveSockets::iterator iter = GetSocketForMessage(client_message);
      if (iter != active_sockets_.end()) {
        base::ListValue* bytes;
        std::string response;
        if (client_message->GetList(kDataPayload, &bytes) &&
            ConvertListValueToString(bytes, &response)) {
          HOST_LOG << "Sending gnubby response: " << GetCommandCode(response);
          iter->second->SendResponse(response);
        } else {
          LOG(ERROR) << "Invalid gnubby data";
          SendErrorAndCloseActiveSocket(iter);
        }
      } else {
        LOG(ERROR) << "Unknown gnubby-auth data connection";
      }
    } else if (type == kErrorMessage) {
      ActiveSockets::iterator iter = GetSocketForMessage(client_message);
      if (iter != active_sockets_.end()) {
        HOST_LOG << "Sending gnubby error";
        SendErrorAndCloseActiveSocket(iter);
      } else {
        LOG(ERROR) << "Unknown gnubby-auth error connection";
      }
    } else {
      LOG(ERROR) << "Unknown gnubby-auth message type: " << type;
    }
  }
}

void GnubbyAuthHandlerLinux::DeliverHostDataMessage(
    int connection_id,
    const std::string& data) const {
  DCHECK(CalledOnValidThread());

  base::DictionaryValue request;
  request.SetString(kMessageType, kDataMessage);
  request.SetInteger(kConnectionId, connection_id);

  base::ListValue* bytes = new base::ListValue();
  for (std::string::const_iterator i = data.begin(); i != data.end(); ++i) {
    bytes->AppendInteger(static_cast<unsigned char>(*i));
  }
  request.Set(kDataPayload, bytes);

  std::string request_json;
  if (!base::JSONWriter::Write(request, &request_json)) {
    LOG(ERROR) << "Failed to create request json";
    return;
  }

  protocol::ExtensionMessage message;
  message.set_type(kGnubbyAuthMessage);
  message.set_data(request_json);

  client_stub_->DeliverHostMessage(message);
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
  DCHECK(CalledOnValidThread());
  DCHECK_NE(net::ERR_IO_PENDING, result);

  if (result < 0) {
    LOG(ERROR) << "Error in accepting a new connection";
    return;
  }

  int connection_id = ++last_connection_id_;
  GnubbySocket* socket =
      new GnubbySocket(std::move(accept_socket_), request_timeout_,
                       base::Bind(&GnubbyAuthHandlerLinux::RequestTimedOut,
                                  base::Unretained(this), connection_id));
  active_sockets_[connection_id] = socket;
  socket->StartReadingRequest(
      base::Bind(&GnubbyAuthHandlerLinux::OnReadComplete,
                 base::Unretained(this), connection_id));

  // Continue accepting new connections.
  DoAccept();
}

void GnubbyAuthHandlerLinux::OnReadComplete(int connection_id) {
  DCHECK(CalledOnValidThread());

  ActiveSockets::iterator iter = active_sockets_.find(connection_id);
  DCHECK(iter != active_sockets_.end());
  std::string request_data;
  if (!iter->second->GetAndClearRequestData(&request_data)) {
    SendErrorAndCloseActiveSocket(iter);
    return;
  }
  ProcessGnubbyRequest(connection_id, request_data);
  iter->second->StartReadingRequest(
      base::Bind(&GnubbyAuthHandlerLinux::OnReadComplete,
                 base::Unretained(this), connection_id));
}

void GnubbyAuthHandlerLinux::CreateAuthorizationSocket() {
  DCHECK(CalledOnValidThread());

  if (!g_gnubby_socket_name.Get().empty()) {
    {
      // DeleteFile() is a blocking operation, but so is creation of the unix
      // socket below. Consider moving this class to a different thread if this
      // causes any problems. See crbug.com/509807 .
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
      LOG(ERROR) << "Failed to open socket for gnubby requests";
      return;
    }
    DoAccept();
  } else {
    HOST_LOG << "No gnubby socket name specified";
  }
}

void GnubbyAuthHandlerLinux::ProcessGnubbyRequest(
    int connection_id,
    const std::string& request_data) {
  HOST_LOG << "Received gnubby request: " << GetCommandCode(request_data);
  DeliverHostDataMessage(connection_id, request_data);
}

GnubbyAuthHandlerLinux::ActiveSockets::iterator
GnubbyAuthHandlerLinux::GetSocketForMessage(base::DictionaryValue* message) {
  int connection_id;
  if (message->GetInteger(kConnectionId, &connection_id)) {
    return active_sockets_.find(connection_id);
  }
  return active_sockets_.end();
}

void GnubbyAuthHandlerLinux::SendErrorAndCloseActiveSocket(
    const ActiveSockets::iterator& iter) {
  iter->second->SendSshError();
  delete iter->second;
  active_sockets_.erase(iter);
}

void GnubbyAuthHandlerLinux::RequestTimedOut(int connection_id) {
  HOST_LOG << "Gnubby request timed out";
  ActiveSockets::iterator iter = active_sockets_.find(connection_id);
  if (iter != active_sockets_.end())
    SendErrorAndCloseActiveSocket(iter);
}

}  // namespace remoting
