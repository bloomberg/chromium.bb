// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/gnubby_auth_handler_posix.h"

#include <unistd.h>
#include <utility>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "net/socket/unix_domain_listen_socket_posix.h"
#include "remoting/base/logging.h"
#include "remoting/host/gnubby_socket.h"
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

// The name of the socket to listen for gnubby requests on.
base::LazyInstance<base::FilePath>::Leaky g_gnubby_socket_name =
    LAZY_INSTANCE_INITIALIZER;

// STL predicate to match by a StreamListenSocket pointer.
class CompareSocket {
 public:
  explicit CompareSocket(net::StreamListenSocket* socket) : socket_(socket) {}

  bool operator()(const std::pair<int, GnubbySocket*> element) const {
    return element.second->IsSocket(socket_);
  }

 private:
  net::StreamListenSocket* socket_;
};

// Socket authentication function that only allows connections from callers with
// the current uid.
bool MatchUid(uid_t user_id, gid_t) {
  bool allowed = user_id == getuid();
  if (!allowed)
    HOST_LOG << "Refused socket connection from uid " << user_id;
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

GnubbyAuthHandlerPosix::GnubbyAuthHandlerPosix(
    protocol::ClientStub* client_stub)
    : client_stub_(client_stub), last_connection_id_(0) {
  DCHECK(client_stub_);
}

GnubbyAuthHandlerPosix::~GnubbyAuthHandlerPosix() {
  STLDeleteValues(&active_sockets_);
}

// static
scoped_ptr<GnubbyAuthHandler> GnubbyAuthHandler::Create(
    protocol::ClientStub* client_stub) {
  return scoped_ptr<GnubbyAuthHandler>(new GnubbyAuthHandlerPosix(client_stub));
}

// static
void GnubbyAuthHandler::SetGnubbySocketName(
    const base::FilePath& gnubby_socket_name) {
  g_gnubby_socket_name.Get() = gnubby_socket_name;
}

void GnubbyAuthHandlerPosix::DeliverClientMessage(const std::string& message) {
  DCHECK(CalledOnValidThread());

  scoped_ptr<base::Value> value(base::JSONReader::Read(message));
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

void GnubbyAuthHandlerPosix::DeliverHostDataMessage(
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
  if (!base::JSONWriter::Write(&request, &request_json)) {
    LOG(ERROR) << "Failed to create request json";
    return;
  }

  protocol::ExtensionMessage message;
  message.set_type(kGnubbyAuthMessage);
  message.set_data(request_json);

  client_stub_->DeliverHostMessage(message);
}

bool GnubbyAuthHandlerPosix::HasActiveSocketForTesting(
    net::StreamListenSocket* socket) const {
  return std::find_if(active_sockets_.begin(),
                      active_sockets_.end(),
                      CompareSocket(socket)) != active_sockets_.end();
}

int GnubbyAuthHandlerPosix::GetConnectionIdForTesting(
    net::StreamListenSocket* socket) const {
  ActiveSockets::const_iterator iter = std::find_if(
      active_sockets_.begin(), active_sockets_.end(), CompareSocket(socket));
  return iter->first;
}

GnubbySocket* GnubbyAuthHandlerPosix::GetGnubbySocketForTesting(
    net::StreamListenSocket* socket) const {
  ActiveSockets::const_iterator iter = std::find_if(
      active_sockets_.begin(), active_sockets_.end(), CompareSocket(socket));
  return iter->second;
}

void GnubbyAuthHandlerPosix::DidAccept(
    net::StreamListenSocket* server,
    scoped_ptr<net::StreamListenSocket> socket) {
  DCHECK(CalledOnValidThread());

  int connection_id = ++last_connection_id_;
  active_sockets_[connection_id] =
      new GnubbySocket(socket.Pass(),
                       base::Bind(&GnubbyAuthHandlerPosix::RequestTimedOut,
                                  base::Unretained(this),
                                  connection_id));
}

void GnubbyAuthHandlerPosix::DidRead(net::StreamListenSocket* socket,
                                     const char* data,
                                     int len) {
  DCHECK(CalledOnValidThread());

  ActiveSockets::iterator iter = std::find_if(
      active_sockets_.begin(), active_sockets_.end(), CompareSocket(socket));
  if (iter != active_sockets_.end()) {
    GnubbySocket* gnubby_socket = iter->second;
    gnubby_socket->AddRequestData(data, len);
    if (gnubby_socket->IsRequestTooLarge()) {
      SendErrorAndCloseActiveSocket(iter);
    } else if (gnubby_socket->IsRequestComplete()) {
      std::string request_data;
      gnubby_socket->GetAndClearRequestData(&request_data);
      ProcessGnubbyRequest(iter->first, request_data);
    }
  } else {
    LOG(ERROR) << "Received data for unknown connection";
  }
}

void GnubbyAuthHandlerPosix::DidClose(net::StreamListenSocket* socket) {
  DCHECK(CalledOnValidThread());

  ActiveSockets::iterator iter = std::find_if(
      active_sockets_.begin(), active_sockets_.end(), CompareSocket(socket));
  if (iter != active_sockets_.end()) {
    delete iter->second;
    active_sockets_.erase(iter);
  }
}

void GnubbyAuthHandlerPosix::CreateAuthorizationSocket() {
  DCHECK(CalledOnValidThread());

  if (!g_gnubby_socket_name.Get().empty()) {
    // If the file already exists, a socket in use error is returned.
    base::DeleteFile(g_gnubby_socket_name.Get(), false);

    HOST_LOG << "Listening for gnubby requests on "
             << g_gnubby_socket_name.Get().value();

    auth_socket_ = net::deprecated::UnixDomainListenSocket::CreateAndListen(
        g_gnubby_socket_name.Get().value(), this, base::Bind(MatchUid));
    if (!auth_socket_.get()) {
      LOG(ERROR) << "Failed to open socket for gnubby requests";
    }
  } else {
    HOST_LOG << "No gnubby socket name specified";
  }
}

void GnubbyAuthHandlerPosix::ProcessGnubbyRequest(
    int connection_id,
    const std::string& request_data) {
  HOST_LOG << "Received gnubby request: " << GetCommandCode(request_data);
  DeliverHostDataMessage(connection_id, request_data);
}

GnubbyAuthHandlerPosix::ActiveSockets::iterator
GnubbyAuthHandlerPosix::GetSocketForMessage(base::DictionaryValue* message) {
  int connection_id;
  if (message->GetInteger(kConnectionId, &connection_id)) {
    return active_sockets_.find(connection_id);
  }
  return active_sockets_.end();
}

void GnubbyAuthHandlerPosix::SendErrorAndCloseActiveSocket(
    const ActiveSockets::iterator& iter) {
  iter->second->SendSshError();

  delete iter->second;
  active_sockets_.erase(iter);
}

void GnubbyAuthHandlerPosix::RequestTimedOut(int connection_id) {
  HOST_LOG << "Gnubby request timed out";
  ActiveSockets::iterator iter = active_sockets_.find(connection_id);
  if (iter != active_sockets_.end())
    SendErrorAndCloseActiveSocket(iter);
}

}  // namespace remoting
