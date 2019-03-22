// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_internal_message_util.h"

#include "base/base64url.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/sha1.h"
#include "chrome/common/media_router/discovery/media_sink_internal.h"
#include "chrome/common/media_router/providers/cast/cast_media_source.h"
#include "components/cast_channel/cast_socket.h"
#include "components/cast_channel/proto/cast_channel.pb.h"
#include "net/base/escape.h"

namespace media_router {

namespace {

constexpr char kClientConnect[] = "client_connect";
constexpr char kAppMessage[] = "app_message";
constexpr char kReceiverAction[] = "receiver_action";
constexpr char kNewSession[] = "new_session";

bool GetString(const base::Value& value,
               const std::string& key,
               std::string* out) {
  const base::Value* string_value =
      value.FindKeyOfType(key, base::Value::Type::STRING);
  if (!string_value)
    return false;

  *out = string_value->GetString();
  return !out->empty();
}

void CopyValueWithDefault(const base::Value& from,
                          const std::string& key,
                          base::Value default_value,
                          base::Value* to) {
  const base::Value* value = from.FindKey(key);
  to->SetKey(key, value ? value->Clone() : std::move(default_value));
}

void CopyValue(const base::Value& from,
               const std::string& key,
               base::Value* to) {
  const base::Value* value = from.FindKey(key);
  if (value)
    to->SetKey(key, value->Clone());
}

CastInternalMessage::Type CastInternalMessageTypeFromString(
    const std::string& type) {
  if (type == kClientConnect)
    return CastInternalMessage::Type::kClientConnect;
  if (type == kAppMessage)
    return CastInternalMessage::Type::kAppMessage;
  if (type == kReceiverAction)
    return CastInternalMessage::Type::kReceiverAction;
  if (type == kNewSession)
    return CastInternalMessage::Type::kNewSession;

  return CastInternalMessage::Type::kOther;
}

std::string CastInternalMessageTypeToString(CastInternalMessage::Type type) {
  switch (type) {
    case CastInternalMessage::Type::kClientConnect:
      return kClientConnect;
    case CastInternalMessage::Type::kAppMessage:
      return kAppMessage;
    case CastInternalMessage::Type::kReceiverAction:
      return kReceiverAction;
    case CastInternalMessage::Type::kNewSession:
      return kNewSession;
    case CastInternalMessage::Type::kOther:
      NOTREACHED();
      return "";
  }
  NOTREACHED();
  return "";
}

// Possible types in a receiver_action message.
constexpr char kReceiverActionTypeCast[] = "cast";
constexpr char kReceiverActionTypeStop[] = "stop";

base::ListValue CapabilitiesToListValue(uint8_t capabilities) {
  base::ListValue value;
  auto& storage = value.GetList();
  if (capabilities & cast_channel::VIDEO_OUT)
    storage.emplace_back("video_out");
  if (capabilities & cast_channel::VIDEO_IN)
    storage.emplace_back("video_in");
  if (capabilities & cast_channel::AUDIO_OUT)
    storage.emplace_back("audio_out");
  if (capabilities & cast_channel::AUDIO_IN)
    storage.emplace_back("audio_in");
  if (capabilities & cast_channel::MULTIZONE_GROUP)
    storage.emplace_back("multizone_group");
  return value;
}

base::Value CreateReceiver(const MediaSinkInternal& sink,
                           const std::string& hash_token) {
  base::Value receiver(base::Value::Type::DICTIONARY);

  std::string label = base::SHA1HashString(sink.sink().id() + hash_token);
  base::Base64UrlEncode(label, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &label);
  receiver.SetKey("label", base::Value(label));

  receiver.SetKey("friendlyName",
                  base::Value(net::EscapeForHTML(sink.sink().name())));
  receiver.SetKey("capabilities",
                  CapabilitiesToListValue(sink.cast_data().capabilities));
  receiver.SetKey("volume", base::Value());
  receiver.SetKey("isActiveInput", base::Value());
  receiver.SetKey("displayStatus", base::Value());

  receiver.SetKey("receiverType", base::Value("cast"));
  return receiver;
}

base::Value CreateReceiverActionMessage(const std::string& client_id,
                                        const MediaSinkInternal& sink,
                                        const std::string& hash_token,
                                        const char* action_type) {
  base::Value message(base::Value::Type::DICTIONARY);
  message.SetKey("receiver", CreateReceiver(sink, hash_token));
  message.SetKey("action", base::Value(action_type));

  base::Value value(base::Value::Type::DICTIONARY);
  value.SetKey("type", base::Value(CastInternalMessageTypeToString(
                           CastInternalMessage::Type::kReceiverAction)));
  value.SetKey("message", std::move(message));
  value.SetKey("sequenceNumber", base::Value(-1));
  value.SetKey("timeoutMillis", base::Value(0));
  value.SetKey("clientId", base::Value(client_id));
  return value;
}

base::Value CreateAppMessageBody(
    const std::string& session_id,
    const cast_channel::CastMessage& cast_message) {
  // TODO(https://crbug.com/862532): Investigate whether it is possible to move
  // instead of copying the contents of |cast_message|. Right now copying is
  // done because the message is passed as a const ref at the
  // CastSocket::Observer level.
  base::Value message(base::Value::Type::DICTIONARY);
  message.SetKey("sessionId", base::Value(session_id));
  message.SetKey("namespaceName", base::Value(cast_message.namespace_()));
  switch (cast_message.payload_type()) {
    case cast_channel::CastMessage_PayloadType_STRING:
      message.SetKey("message", base::Value(cast_message.payload_utf8()));
      break;
    case cast_channel::CastMessage_PayloadType_BINARY: {
      const auto& payload = cast_message.payload_binary();
      message.SetKey("message",
                     base::Value(base::Value::BlobStorage(
                         payload.front(), payload.front() + payload.size())));
      break;
    }
    default:
      NOTREACHED();
      break;
  }
  return message;
}

}  // namespace

// static
std::unique_ptr<CastInternalMessage> CastInternalMessage::From(
    base::Value message) {
  if (!message.is_dict()) {
    DVLOG(2) << "Failed to read JSON message: " << message;
    return nullptr;
  }

  std::string str_type;
  if (!GetString(message, "type", &str_type)) {
    DVLOG(2) << "Missing type value, message: " << message;
    return nullptr;
  }

  CastInternalMessage::Type message_type =
      CastInternalMessageTypeFromString(str_type);
  if (message_type == CastInternalMessage::Type::kOther) {
    DVLOG(2) << __func__ << ": Unsupported message type: " << str_type
             << ", message: " << message;
    return nullptr;
  }

  std::string client_id;
  if (!GetString(message, "clientId", &client_id)) {
    DVLOG(2) << "Missing clientId, message: " << message;
    return nullptr;
  }

  base::Value* message_body_value = message.FindKey("message");
  if (!message_body_value ||
      (!message_body_value->is_dict() && !message_body_value->is_string())) {
    DVLOG(2) << "Missing message body, message: " << message;
    return nullptr;
  }

  auto internal_message =
      std::make_unique<CastInternalMessage>(message_type, client_id);

  base::Value* sequence_number_value =
      message.FindKeyOfType("sequenceNumber", base::Value::Type::INTEGER);
  if (sequence_number_value)
    internal_message->sequence_number = sequence_number_value->GetInt();

  if (message_type == CastInternalMessage::Type::kAppMessage) {
    if (!message_body_value->is_dict())
      return nullptr;

    if (!GetString(*message_body_value, "namespaceName",
                   &internal_message->app_message_namespace) ||
        !GetString(*message_body_value, "sessionId",
                   &internal_message->app_message_session_id)) {
      DVLOG(2) << "Missing namespace or session ID, message: " << message;
      return nullptr;
    }

    base::Value* app_message_value = message_body_value->FindKey("message");
    if (!app_message_value ||
        (!app_message_value->is_dict() && !app_message_value->is_string())) {
      DVLOG(2) << "Missing app message, message: " << message;
      return nullptr;
    }
    internal_message->app_message_body = std::move(*app_message_value);
  }

  return internal_message;
}

CastInternalMessage::CastInternalMessage(CastInternalMessage::Type type,
                                         const std::string& client_id)
    : type(type), client_id(client_id) {}

CastInternalMessage::~CastInternalMessage() = default;

blink::mojom::PresentationConnectionMessagePtr CreatePresentationMessage(
    const base::Value& message) {
  std::string str;
  CHECK(base::JSONWriter::Write(message, &str));
  return blink::mojom::PresentationConnectionMessage::NewMessage(str);
}

blink::mojom::PresentationConnectionMessagePtr CreateReceiverActionCastMessage(
    const std::string& client_id,
    const MediaSinkInternal& sink,
    const std::string& hash_token) {
  return CreatePresentationMessage(CreateReceiverActionMessage(
      client_id, sink, hash_token, kReceiverActionTypeCast));
}

blink::mojom::PresentationConnectionMessagePtr CreateReceiverActionStopMessage(
    const std::string& client_id,
    const MediaSinkInternal& sink,
    const std::string& hash_token) {
  return CreatePresentationMessage(CreateReceiverActionMessage(
      client_id, sink, hash_token, kReceiverActionTypeStop));
}

// static
std::unique_ptr<CastSession> CastSession::From(
    const MediaSinkInternal& sink,
    const std::string& hash_token,
    const base::Value& receiver_status) {
  // There should be only 1 app on |receiver_status|.
  const base::Value* app_list_value =
      receiver_status.FindKeyOfType("applications", base::Value::Type::LIST);
  if (!app_list_value || app_list_value->GetList().size() != 1) {
    DVLOG(2) << "receiver_status does not contain exactly one app: "
             << receiver_status;
    return nullptr;
  }

  auto session = std::make_unique<CastSession>();

  // Fill in mandatory Session fields.
  const base::Value& app_value = app_list_value->GetList()[0];
  if (!GetString(app_value, "sessionId", &session->session_id) ||
      !GetString(app_value, "appId", &session->app_id) ||
      !GetString(app_value, "transportId", &session->transport_id) ||
      !GetString(app_value, "displayName", &session->display_name)) {
    DVLOG(2) << "app_value missing mandatory fields: " << app_value;
    return nullptr;
  }

  // Optional Session fields.
  GetString(app_value, "statusText", &session->status);

  base::Value receiver_value = CreateReceiver(sink, hash_token);
  CopyValue(receiver_status, "volume", &receiver_value);
  CopyValue(receiver_status, "isActiveInput", &receiver_value);

  // Create |session->value|.
  session->value = base::Value(base::Value::Type::DICTIONARY);
  auto& session_value = session->value;
  session_value.SetKey("sessionId", base::Value(session->session_id));
  session_value.SetKey("appId", base::Value(session->app_id));
  session_value.SetKey("transportId", base::Value(session->transport_id));
  session_value.SetKey("receiver", std::move(receiver_value));

  CopyValueWithDefault(app_value, "displayName", base::Value(""),
                       &session_value);
  CopyValueWithDefault(app_value, "senderApps", base::ListValue(),
                       &session_value);
  CopyValueWithDefault(app_value, "statusText", base::Value(), &session_value);
  CopyValueWithDefault(app_value, "appImages", base::ListValue(),
                       &session_value);
  CopyValueWithDefault(app_value, "namespaces", base::ListValue(),
                       &session_value);

  const base::Value* namespaces_value =
      app_value.FindKeyOfType("namespaces", base::Value::Type::LIST);
  if (!namespaces_value || namespaces_value->GetList().empty()) {
    // A session without namespaces is invalid, except for a multizone leader.
    if (session->app_id != kMultizoneLeaderAppId)
      return nullptr;
  } else {
    for (const auto& namespace_value : namespaces_value->GetList()) {
      std::string message_namespace;
      if (!namespace_value.is_dict() ||
          !GetString(namespace_value, "name", &message_namespace))
        return nullptr;

      session->message_namespaces.insert(std::move(message_namespace));
    }
  }

  session_value.SetKey("namespaces",
                       namespaces_value ? namespaces_value->Clone()
                                        : base::Value(base::Value::Type::LIST));

  return session;
}

CastSession::CastSession() = default;
CastSession::~CastSession() = default;

// static
std::string CastSession::GetRouteDescription(const CastSession& session) {
  return !session.status.empty() ? session.status : session.display_name;
}

blink::mojom::PresentationConnectionMessagePtr CreateNewSessionMessage(
    const CastSession& session,
    const std::string& client_id) {
  base::Value message(base::Value::Type::DICTIONARY);
  message.SetKey("type", base::Value(CastInternalMessageTypeToString(
                             CastInternalMessage::Type::kNewSession)));
  message.SetKey("message", session.value.Clone());
  message.SetKey("sequenceNumber", base::Value(-1));
  message.SetKey("timeoutMillis", base::Value(0));
  message.SetKey("clientId", base::Value(client_id));
  return CreatePresentationMessage(message);
}

blink::mojom::PresentationConnectionMessagePtr CreateAppMessageAck(
    const std::string& client_id,
    int sequence_number) {
  base::Value message(base::Value::Type::DICTIONARY);
  message.SetKey("type", base::Value(CastInternalMessageTypeToString(
                             CastInternalMessage::Type::kAppMessage)));
  message.SetKey("message", base::Value());
  message.SetKey("sequenceNumber", base::Value(sequence_number));
  message.SetKey("timeoutMillis", base::Value(0));
  message.SetKey("clientId", base::Value(client_id));
  return CreatePresentationMessage(message);
}

blink::mojom::PresentationConnectionMessagePtr CreateAppMessage(
    const std::string& session_id,
    const std::string& client_id,
    const cast_channel::CastMessage& cast_message) {
  base::Value message(base::Value::Type::DICTIONARY);
  message.SetKey("type", base::Value(CastInternalMessageTypeToString(
                             CastInternalMessage::Type::kAppMessage)));
  message.SetKey("message", CreateAppMessageBody(session_id, cast_message));
  message.SetKey("sequenceNumber", base::Value(-1));
  message.SetKey("timeoutMillis", base::Value(0));
  message.SetKey("clientId", base::Value(client_id));
  return CreatePresentationMessage(message);
}

}  // namespace media_router
