// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_status_service.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringize_macros.h"
#include "base/values.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_util.h"
#include "remoting/host/websocket_connection.h"
#include "remoting/host/websocket_listener.h"

namespace remoting {

namespace {

// HostStatusService uses the first port available in the following range.
const int kPortRangeMin = 12810;
const int kPortRangeMax = 12820;

const char kChromeExtensionUrlSchemePrefix[] = "chrome-extension://";

#if defined(NDEBUG)
const char* const kAllowedWebApplicationIds[] = {
  "gbchcmhmhahfdphkhkmpfmihenigjmpp",  // Chrome Remote Desktop
  "kgngmbheleoaphbjbaiobfdepmghbfah",  // Pre-release Chrome Remote Desktop
  "odkaodonbgfohohmklejpjiejmcipmib",  // Dogfood Chrome Remote Desktop
  "ojoimpklfciegopdfgeenehpalipignm",  // Chromoting canary
};
#endif

// All messages we expect should be smaller than 64k.
const uint64 kMaximumIncomingMessageSize = 65536;

}  // namespace

class HostStatusService::Connection : public WebSocketConnection::Delegate {
 public:
  // |service| owns Connection connection objects and must outlive them.
  Connection(HostStatusService* service,
             scoped_ptr<WebSocketConnection> websocket);
  virtual ~Connection();

  // WebSocketConnection::Delegate interface.
  virtual void OnWebSocketMessage(const std::string& message) OVERRIDE;
  virtual void OnWebSocketClosed() OVERRIDE;

 private:
  // Sends message with the specified |method| and |data|.
  void SendMessage(const std::string& method,
                   scoped_ptr<base::DictionaryValue> data);

   // Closes the connection and destroys this object.
  void Close();

  HostStatusService* service_;
  scoped_ptr<WebSocketConnection> websocket_;

  DISALLOW_COPY_AND_ASSIGN(Connection);
};

HostStatusService::Connection::Connection(
    HostStatusService* service,
    scoped_ptr<WebSocketConnection> websocket)
    : service_(service),
      websocket_(websocket.Pass()) {
  websocket_->Accept(this);
  websocket_->set_maximum_message_size(kMaximumIncomingMessageSize);
}

HostStatusService::Connection::~Connection() {
}

void HostStatusService::Connection::OnWebSocketMessage(
    const std::string& message) {
  scoped_ptr<base::Value> json(
      base::JSONReader::Read(message, base::JSON_ALLOW_TRAILING_COMMAS));

  // Verify that we've received a valid JSON dictionary and extract |method| and
  // |data| fields from it.
  base::DictionaryValue* message_dict = NULL;
  std::string method;
  base::DictionaryValue* data = NULL;
  if (!json.get() ||
      !json->GetAsDictionary(&message_dict) ||
      !message_dict->GetString("method", &method) ||
      !message_dict->GetDictionary("data", &data)) {
    LOG(ERROR) << "Received invalid message: " << message;
    Close();
    return;
  }

  if (method == "getHostStatus") {
    SendMessage("hostStatus", service_->GetStatusMessage());
  } else {
    LOG(ERROR) << "Received message with unknown method: " << message;
    scoped_ptr<base::DictionaryValue> response(new base::DictionaryValue());
    response->SetString("method", method);
    SendMessage("unsupportedMethod", response.Pass());
    return;
  }
}

void HostStatusService::Connection::OnWebSocketClosed() {
  Close();
}

void HostStatusService::Connection::SendMessage(
    const std::string& method,
    scoped_ptr<base::DictionaryValue> data) {
  scoped_ptr<base::DictionaryValue> message(new base::DictionaryValue());
  message->SetString("method", method);
  message->Set("data", data.release());

  std::string message_json;
  base::JSONWriter::Write(message.get(), &message_json);
  websocket_->SendText(message_json);
}

void HostStatusService::Connection::Close() {
  websocket_.reset();
  service_->OnConnectionClosed(this);
}

HostStatusService::HostStatusService()
    : started_(false) {
  // TODO(sergeyu): Do we need to listen on IPv6 port too?
  char ip[] = {127, 0, 0, 1};
  net::IPAddressNumber localhost(ip, ip + sizeof(ip));
  for (int port = kPortRangeMin; port < kPortRangeMax; ++port) {
    net::IPEndPoint endpoint(localhost, port);
    // base::Unretained is safe because we own |websocket_listener_|.
    if (websocket_listener_.Listen(
            endpoint,
            base::Bind(&HostStatusService::OnNewConnection,
                       base::Unretained(this)))) {
      service_host_name_ = "localhost:" + base::UintToString(port);
      LOG(INFO) << "Listening for WebSocket connections on localhost:" << port;
      break;
    }
  }
}

HostStatusService::~HostStatusService() {
  STLDeleteElements(&connections_);
}

void HostStatusService::SetHostIsUp(const std::string& host_id) {
  started_ = true;
  host_id_ = host_id;
}
void HostStatusService::SetHostIsDown() {
  started_ = false;
  host_id_.clear();
}

// static
bool HostStatusService::IsAllowedOrigin(const std::string& origin) {
#ifndef NDEBUG
  // Allow all chrome extensions in Debug builds.
  return StartsWithASCII(origin, kChromeExtensionUrlSchemePrefix, false);
#else
  // For Release builds allow only specific set of clients.
  //
  // TODO(sergeyu): Allow whitelisting of origins different from the ones in
  // kAllowedWebApplicationIds (e.g. specify them in the host config).
  std::string prefix(kChromeExtensionUrlSchemePrefix);
  for (size_t i = 0; i < arraysize(kAllowedWebApplicationIds); ++i) {
    if (origin == prefix + kAllowedWebApplicationIds[i]) {
      return true;
    }
  }
  return false;
#endif
}

void HostStatusService::OnNewConnection(
    scoped_ptr<WebSocketConnection> connection) {
  if (connection->request_host() != service_host_name_) {
    LOG(ERROR) << "Received connection for invalid host: "
               << connection->request_host()
               << ". Expected " << service_host_name_;
    connection->Reject();
    return;
  }

  if (connection->request_path() != "/remoting_host_status") {
    LOG(ERROR) << "Received connection for unknown path: "
               << connection->request_path();
    connection->Reject();
    return;
  }

  if (!IsAllowedOrigin(connection->origin())) {
    LOG(ERROR) << "Rejecting connection from unknown origin: "
               << connection->origin();
    connection->Reject();
    return;
  }

  // Accept connection.
  connections_.insert(new Connection(this, connection.Pass()));
}

void HostStatusService::OnConnectionClosed(Connection* connection) {
  connections_.erase(connection);
  delete connection;
}

scoped_ptr<base::DictionaryValue> HostStatusService::GetStatusMessage() {
  scoped_ptr<base::DictionaryValue> result(new base::DictionaryValue());
  result->SetString("state", started_ ? "STARTED" : "STOPPED");
  result->SetString("version", STRINGIZE(VERSION));
  result->SetString("hostId", host_id_);
  return result.Pass();
}

}  // namespace remoting
