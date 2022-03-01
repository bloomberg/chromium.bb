// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_extension_session.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "remoting/base/logging.h"
#include "remoting/host/client_session_details.h"
#include "remoting/host/security_key/security_key_auth_handler.h"
#include "remoting/proto/control.pb.h"
#include "remoting/protocol/client_stub.h"

namespace {

// Used as the type attribute of all Security Key protocol::ExtensionMessages.
const char kExtensionMessageType[] = "gnubby-auth";

// SecurityKey extension message data members.
const char kConnectionId[] = "connectionId";
const char kControlMessage[] = "control";
const char kControlOption[] = "option";
const char kDataMessage[] = "data";
const char kDataPayload[] = "data";
const char kErrorMessage[] = "error";
const char kSecurityKeyAuthV1[] = "auth-v1";
const char kMessageType[] = "type";

// Returns the command code (the first byte of the data) if it exists, or -1 if
// the data is empty.
unsigned int GetCommandCode(const std::string& data) {
  return data.empty() ? -1 : static_cast<unsigned int>(data[0]);
}

// Creates a string of byte data from a ListValue of numbers. Returns true if
// all of the list elements are numbers.
bool ConvertListToString(const base::Value& bytes, std::string* out) {
  out->clear();

  unsigned int byte_count = bytes.GetList().size();
  if (byte_count != 0) {
    out->reserve(byte_count);
    for (unsigned int i = 0; i < byte_count; i++) {
      auto value = bytes.GetList()[i].GetIfInt();
      if (!value.has_value()) {
        return false;
      }
      out->push_back(static_cast<char>(value.value()));
    }
  }
  return true;
}

}  // namespace

namespace remoting {

SecurityKeyExtensionSession::SecurityKeyExtensionSession(
    ClientSessionDetails* client_session_details,
    protocol::ClientStub* client_stub,
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner)
    : client_stub_(client_stub) {
  DCHECK(client_stub_);

  security_key_auth_handler_ = remoting::SecurityKeyAuthHandler::Create(
      client_session_details,
      base::BindRepeating(&SecurityKeyExtensionSession::SendMessageToClient,
                          base::Unretained(this)),
      file_task_runner);
}

SecurityKeyExtensionSession::~SecurityKeyExtensionSession() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

// Returns true if the |message| is a Security Key ExtensionMessage.
// This is done so the host does not pass |message| to other HostExtensions.
// TODO(joedow): Use |client_session_details| to disconnect the session if we
//               receive an invalid extension message.
bool SecurityKeyExtensionSession::OnExtensionMessage(
    ClientSessionDetails* client_session_details,
    protocol::ClientStub* client_stub,
    const protocol::ExtensionMessage& message) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (message.type() != kExtensionMessageType) {
    return false;
  }

  std::unique_ptr<base::Value> value =
      base::JSONReader::ReadDeprecated(message.data());
  if (!value || !value->is_dict()) {
    LOG(WARNING) << "Failed to retrieve data from gnubby-auth message.";
    return true;
  }

  std::string* maybe_type = value->FindStringKey(kMessageType);
  if (!maybe_type) {
    LOG(WARNING) << "Invalid gnubby-auth message format.";
    return true;
  }
  std::string type = *maybe_type;

  base::Value::DictStorage client_message = std::move(*value).TakeDict();
  if (type == kControlMessage) {
    ProcessControlMessage(client_message);
  } else if (type == kDataMessage) {
    ProcessDataMessage(client_message);
  } else if (type == kErrorMessage) {
    ProcessErrorMessage(client_message);
  } else {
    VLOG(2) << "Unknown gnubby-auth message type: " << type;
  }

  return true;
}

void SecurityKeyExtensionSession::ProcessControlMessage(
    const base::Value::DictStorage& message_data) const {
  auto option_iter = message_data.find(kControlOption);
  if (option_iter == message_data.end() || !option_iter->second.is_string()) {
    LOG(WARNING) << "Could not extract control option from message.";
    return;
  }
  auto option = option_iter->second.GetString();

  if (option == kSecurityKeyAuthV1) {
    security_key_auth_handler_->CreateSecurityKeyConnection();
  } else {
    VLOG(2) << "Invalid gnubby-auth control option: " << option;
  }
}

void SecurityKeyExtensionSession::ProcessDataMessage(
    const base::Value::DictStorage& message_data) const {
  auto connection_id_iter = message_data.find(kConnectionId);
  if (connection_id_iter == message_data.end() ||
      !connection_id_iter->second.is_int()) {
    LOG(WARNING) << "Could not extract connection id from message.";
    return;
  }
  auto connection_id = connection_id_iter->second.GetInt();

  if (!security_key_auth_handler_->IsValidConnectionId(connection_id)) {
    LOG(WARNING) << "Unknown gnubby-auth data connection: '" << connection_id
                 << "'";
    return;
  }

  std::string response;
  auto bytes_iter = message_data.find(kDataPayload);
  if (bytes_iter != message_data.end() &&
      ConvertListToString(bytes_iter->second, &response)) {
    HOST_LOG << "Sending security key response: " << GetCommandCode(response);
    security_key_auth_handler_->SendClientResponse(connection_id, response);
  } else {
    LOG(WARNING) << "Could not extract response data from message.";
    security_key_auth_handler_->SendErrorAndCloseConnection(connection_id);
    return;
  }
}

void SecurityKeyExtensionSession::ProcessErrorMessage(
    const base::Value::DictStorage& message_data) const {
  auto connection_id_iter = message_data.find(kConnectionId);
  if (connection_id_iter == message_data.end() ||
      !connection_id_iter->second.is_int()) {
    LOG(WARNING) << "Could not extract connection id from message.";
    return;
  }
  auto connection_id = connection_id_iter->second.GetInt();

  if (security_key_auth_handler_->IsValidConnectionId(connection_id)) {
    HOST_LOG << "Sending security key error";
    security_key_auth_handler_->SendErrorAndCloseConnection(connection_id);
  } else {
    LOG(WARNING) << "Unknown gnubby-auth connection id: " << connection_id;
  }
}

void SecurityKeyExtensionSession::SendMessageToClient(
    int connection_id,
    const std::string& data) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(client_stub_);

  base::DictionaryValue request;
  request.SetString(kMessageType, kDataMessage);
  request.SetInteger(kConnectionId, connection_id);

  base::ListValue bytes;
  for (auto& byte : data) {
    bytes.Append(static_cast<unsigned char>(byte));
  }
  request.SetKey(kDataPayload, std::move(bytes));

  std::string request_json;
  CHECK(base::JSONWriter::Write(request, &request_json));

  protocol::ExtensionMessage message;
  message.set_type(kExtensionMessageType);
  message.set_data(request_json);

  client_stub_->DeliverHostMessage(message);
}

void SecurityKeyExtensionSession::SetSecurityKeyAuthHandlerForTesting(
    std::unique_ptr<SecurityKeyAuthHandler> security_key_auth_handler) {
  DCHECK(security_key_auth_handler);

  security_key_auth_handler_ = std::move(security_key_auth_handler);
  security_key_auth_handler_->SetSendMessageCallback(
      base::BindRepeating(&SecurityKeyExtensionSession::SendMessageToClient,
                          base::Unretained(this)));
}

}  // namespace remoting
