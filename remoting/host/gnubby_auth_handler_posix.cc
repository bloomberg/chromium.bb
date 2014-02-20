// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/gnubby_auth_handler_posix.h"

#include <unistd.h>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "net/socket/unix_domain_socket_posix.h"
#include "remoting/base/logging.h"
#include "remoting/host/gnubby_util.h"
#include "remoting/proto/control.pb.h"
#include "remoting/protocol/client_stub.h"

namespace remoting {

namespace {

const int kMaxRequestLength = 4096;

const char kConnectionId[] = "connectionId";
const char kControlMessage[] = "control";
const char kControlOption[] = "option";
const char kDataMessage[] = "data";
const char kGnubbyAuthMessage[] = "gnubby-auth";
const char kGnubbyAuthV1[] = "auth-v1";
const char kJSONMessage[] = "jsonMessage";
const char kMessageType[] = "type";

// The name of the socket to listen for gnubby requests on.
base::LazyInstance<base::FilePath>::Leaky g_gnubby_socket_name =
    LAZY_INSTANCE_INITIALIZER;

// STL predicate to match by a StreamListenSocket pointer.
class CompareSocket {
 public:
  explicit CompareSocket(net::StreamListenSocket* socket) : socket_(socket) {}

  bool operator()(const std::pair<int, net::StreamListenSocket*> element)
      const {
    return socket_ == element.second;
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

// Returns the request data length from the first four data bytes.
int GetRequestLength(const char* data) {
  return ((data[0] & 255) << 24) + ((data[1] & 255) << 16) +
         ((data[2] & 255) << 8) + (data[3] & 255) + 4;
}

// Returns true if the request data is complete (has at least as many bytes as
// indicated by the size in the first four bytes plus four for the first bytes).
bool IsRequestComplete(const char* data, int data_len) {
  if (data_len < 4)
    return false;
  return GetRequestLength(data) <= data_len;
}

// Returns true if the request data size is bigger than the threshold.
bool IsRequestTooLarge(const char* data, int data_len, int max_len) {
  if (data_len < 4)
    return false;
  return GetRequestLength(data) > max_len;
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
      int connection_id;
      std::string json_message;
      if (client_message->GetInteger(kConnectionId, &connection_id) &&
          client_message->GetString(kJSONMessage, &json_message)) {
        ActiveSockets::iterator iter = active_sockets_.find(connection_id);
        if (iter != active_sockets_.end()) {
          HOST_LOG << "Sending gnubby response";

          std::string response;
          GetGnubbyResponseFromJson(json_message, &response);
          iter->second->Send(response);
        } else {
          LOG(ERROR) << "Received gnubby-auth data for unknown connection";
        }
      } else {
        LOG(ERROR) << "Invalid gnubby-auth data message";
      }
    } else {
      LOG(ERROR) << "Unknown gnubby-auth message type: " << type;
    }
  }
}

void GnubbyAuthHandlerPosix::DeliverHostDataMessage(int connection_id,
                                                    const std::string& data)
    const {
  DCHECK(CalledOnValidThread());

  base::DictionaryValue request;
  request.SetString(kMessageType, kDataMessage);
  request.SetInteger(kConnectionId, connection_id);
  request.SetString(kJSONMessage, data);

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

void GnubbyAuthHandlerPosix::DidAccept(
    net::StreamListenSocket* server,
    scoped_ptr<net::StreamListenSocket> socket) {
  DCHECK(CalledOnValidThread());

  active_sockets_[++last_connection_id_] = socket.release();
}

void GnubbyAuthHandlerPosix::DidRead(net::StreamListenSocket* socket,
                                     const char* data,
                                     int len) {
  DCHECK(CalledOnValidThread());

  ActiveSockets::iterator socket_iter = std::find_if(
      active_sockets_.begin(), active_sockets_.end(), CompareSocket(socket));
  if (socket_iter != active_sockets_.end()) {
    int connection_id = socket_iter->first;

    ActiveRequests::iterator request_iter =
        active_requests_.find(connection_id);
    if (request_iter != active_requests_.end()) {
      std::vector<char>& saved_vector = request_iter->second;
      if (IsRequestTooLarge(
              saved_vector.data(), saved_vector.size(), kMaxRequestLength)) {
        // We can't close a StreamListenSocket; throw away everything but the
        // size bytes.
        saved_vector.resize(4);
        return;
      }
      saved_vector.insert(saved_vector.end(), data, data + len);

      if (IsRequestComplete(saved_vector.data(), saved_vector.size())) {
        ProcessGnubbyRequest(
            connection_id, saved_vector.data(), saved_vector.size());
        active_requests_.erase(request_iter);
      }
    } else if (IsRequestComplete(data, len)) {
      ProcessGnubbyRequest(connection_id, data, len);
    } else {
      if (IsRequestTooLarge(data, len, kMaxRequestLength)) {
        // Only save the size bytes.
        active_requests_[connection_id] = std::vector<char>(data, data + 4);
      } else {
        active_requests_[connection_id] = std::vector<char>(data, data + len);
      }
    }
  }
}

void GnubbyAuthHandlerPosix::DidClose(net::StreamListenSocket* socket) {
  DCHECK(CalledOnValidThread());

  ActiveSockets::iterator iter = std::find_if(
      active_sockets_.begin(), active_sockets_.end(), CompareSocket(socket));
  if (iter != active_sockets_.end()) {
    active_requests_.erase(iter->first);

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

    auth_socket_ = net::UnixDomainSocket::CreateAndListen(
        g_gnubby_socket_name.Get().value(), this, base::Bind(MatchUid));
    if (!auth_socket_.get()) {
      LOG(ERROR) << "Failed to open socket for gnubby requests";
    }
  } else {
    HOST_LOG << "No gnubby socket name specified";
  }
}

void GnubbyAuthHandlerPosix::ProcessGnubbyRequest(int connection_id,
                                                  const char* data,
                                                  int data_len) {
  std::string json;
  if (GetJsonFromGnubbyRequest(data, data_len, &json)) {
    HOST_LOG << "Received gnubby request";
    DeliverHostDataMessage(connection_id, json);
  } else {
    LOG(ERROR) << "Could not decode gnubby request";
  }
}

}  // namespace remoting
