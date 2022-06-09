// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TODO(crbug.com/1207718): Delete this file.

#include "components/cast_streaming/browser/cast_message_port_impl.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "components/cast_streaming/browser/message_serialization.h"
#include "third_party/openscreen/src/platform/base/error.h"

namespace cast_streaming {

namespace {

const char kKeyMediaSessionId[] = "mediaSessionId";
const char kKeyPlaybackRate[] = "playbackRate";
const char kKeyPlayerState[] = "playerState";
const char kKeyCurrentTime[] = "currentTime";
const char kKeySupportedMediaCommands[] = "supportedMediaCommands";
const char kKeyDisableStreamGrouping[] = "disableStreamGrouping";
const char kKeyMedia[] = "media";
const char kKeyContentId[] = "contentId";
const char kKeyStreamType[] = "streamType";
const char kKeyContentType[] = "contentType";
const char kValuePlaying[] = "PLAYING";
const char kValueLive[] = "LIVE";
const char kValueVideoWebm[] = "video/webm";

base::Value GetMediaCurrentStatusValue() {
  base::Value media(base::Value::Type::DICTIONARY);
  media.SetKey(kKeyContentId, base::Value(""));
  media.SetKey(kKeyStreamType, base::Value(kValueLive));
  media.SetKey(kKeyContentType, base::Value(kValueVideoWebm));

  base::Value media_current_status(base::Value::Type::DICTIONARY);
  media_current_status.SetKey(kKeyMediaSessionId, base::Value(0));
  media_current_status.SetKey(kKeyPlaybackRate, base::Value(1.0));
  media_current_status.SetKey(kKeyPlayerState, base::Value(kValuePlaying));
  media_current_status.SetKey(kKeyCurrentTime, base::Value(0));
  media_current_status.SetKey(kKeySupportedMediaCommands, base::Value(0));
  media_current_status.SetKey(kKeyDisableStreamGrouping, base::Value(true));
  media_current_status.SetKey(kKeyMedia, std::move(media));

  return media_current_status;
}

}  // namespace

CastMessagePortImpl::CastMessagePortImpl(
    std::unique_ptr<cast_api_bindings::MessagePort> message_port,
    base::OnceClosure on_close)
    : message_port_(std::move(message_port)), on_close_(std::move(on_close)) {
  DVLOG(1) << __func__;
  message_port_->SetReceiver(this);

  // Initialize the connection with the Cast Streaming Sender.
  PostMessage(kValueSystemSenderId, kSystemNamespace, kInitialConnectMessage);
}

CastMessagePortImpl::~CastMessagePortImpl() = default;

void CastMessagePortImpl::MaybeClose() {
  if (message_port_) {
    message_port_.reset();
  }
  if (client_) {
    client_->OnError(
        openscreen::Error(openscreen::Error::Code::kCastV2CastSocketError));
  }
  if (on_close_) {
    // |this| might be deleted as part of |on_close_| being run. Do not add any
    // code after running the closure.
    std::move(on_close_).Run();
  }
}

void CastMessagePortImpl::SetClient(
    openscreen::cast::MessagePort::Client* client,
    std::string client_sender_id) {
  DVLOG(2) << __func__;
  DCHECK_NE(!client_, !client);
  client_ = client;
  if (!client_)
    MaybeClose();
}

void CastMessagePortImpl::ResetClient() {
  client_ = nullptr;
  MaybeClose();
}

void CastMessagePortImpl::SendInjectResponse(const std::string& sender_id,
                                             const std::string& message) {
  absl::optional<base::Value> value = base::JSONReader::Read(message);
  if (!value) {
    LOG(ERROR) << "Malformed message from sender " << sender_id
               << ": not a json payload:" << message;
    return;
  }

  if (!value->is_dict()) {
    LOG(ERROR) << "Malformed message from sender " << sender_id
               << ": non-dictionary json payload: " << message;
    return;
  }

  const std::string* type = value->FindStringKey(kKeyType);
  if (!type) {
    LOG(ERROR) << "Malformed message from sender " << sender_id
               << ": no message type: " << message;
    return;
  }
  if (*type != kValueWrapped) {
    LOG(ERROR) << "Malformed message from sender " << sender_id
               << ": unknown message type: " << *type;
    return;
  }

  absl::optional<int> request_id = value->FindIntKey(kKeyRequestId);
  if (!request_id) {
    LOG(ERROR) << "Malformed message from sender " << sender_id
               << ": no request id: " << message;
    return;
  }

  // Build the response message.
  base::Value response_value(base::Value::Type::DICTIONARY);
  response_value.SetKey(kKeyType, base::Value(kValueError));
  response_value.SetKey(kKeyRequestId, base::Value(request_id.value()));
  response_value.SetKey(kKeyData, base::Value(kValueInjectNotSupportedError));
  response_value.SetKey(kKeyCode, base::Value(kValueWrappedError));

  std::string json_message;
  CHECK(base::JSONWriter::Write(response_value, &json_message));
  PostMessage(sender_id, kInjectNamespace, json_message);
}

void CastMessagePortImpl::HandleMediaMessage(const std::string& sender_id,
                                             const std::string& message) {
  absl::optional<base::Value> value = base::JSONReader::Read(message);
  if (!value) {
    LOG(ERROR) << "Malformed message from sender " << sender_id
               << ": not a json payload: " << message;
    return;
  }

  if (!value->is_dict()) {
    LOG(ERROR) << "Malformed message from sender " << sender_id
               << ": non-dictionary json payload: " << message;
    return;
  }

  const std::string* type = value->FindStringKey(kKeyType);
  if (!type) {
    LOG(ERROR) << "Malformed message from sender " << sender_id
               << ": no message type: " << message;
    return;
  }

  if (*type == kValueMediaPlay || *type == kValueMediaPause) {
    // Not supported. Just ignore.
    return;
  }

  if (*type != kValueMediaGetStatus) {
    LOG(ERROR) << "Malformed message from sender " << sender_id
               << ": unknown message type: " << *type;
    return;
  }

  absl::optional<int> request_id = value->FindIntKey(kKeyRequestId);
  if (!request_id.has_value()) {
    LOG(ERROR) << "Malformed message from sender " << sender_id
               << ": no request id: " << message;
    return;
  }

  base::Value message_status_list(base::Value::Type::LIST);
  message_status_list.Append(GetMediaCurrentStatusValue());

  base::Value response_value(base::Value::Type::DICTIONARY);
  response_value.SetKey(kKeyRequestId, base::Value(request_id.value()));
  response_value.SetKey(kKeyType, base::Value(kValueMediaStatus));
  response_value.SetKey(kKeyStatus, std::move(message_status_list));

  std::string json_message;
  CHECK(base::JSONWriter::Write(response_value, &json_message));
  PostMessage(sender_id, kMediaNamespace, json_message);
}

void CastMessagePortImpl::PostMessage(const std::string& sender_id,
                                      const std::string& message_namespace,
                                      const std::string& message) {
  DVLOG(3) << __func__;
  if (!message_port_)
    return;

  DVLOG(3) << "Received Open Screen message. SenderId: " << sender_id
           << ". Namespace: " << message_namespace << ". Message: " << message;
  message_port_->PostMessage(
      SerializeCastMessage(sender_id, message_namespace, message));
}

bool CastMessagePortImpl::OnMessage(
    base::StringPiece message,
    std::vector<std::unique_ptr<cast_api_bindings::MessagePort>> ports) {
  DVLOG(3) << __func__;

  // If |client_| was cleared, |message_port_| should have been reset.
  DCHECK(client_);

  if (!ports.empty()) {
    // We should never receive any ports for Cast Streaming.
    LOG(ERROR) << "Received ports on Cast Streaming MessagePort.";
    return false;
  }

  std::string sender_id;
  std::string message_namespace;
  std::string str_message;
  if (!DeserializeCastMessage(message, &sender_id, &message_namespace,
                              &str_message)) {
    LOG(ERROR) << "Received bad message.";
    client_->OnError(
        openscreen::Error(openscreen::Error::Code::kCastV2InvalidMessage));
    return true;
  }
  DVLOG(3) << "Received Cast message. SenderId: " << sender_id
           << ". Namespace: " << message_namespace
           << ". Message: " << str_message;

  // TODO(b/156118960): Have Open Screen handle message namespaces.
  if (message_namespace == kMirroringNamespace ||
      message_namespace == kRemotingNamespace) {
    client_->OnMessage(sender_id, message_namespace, str_message);
  } else if (message_namespace == kInjectNamespace) {
    SendInjectResponse(sender_id, str_message);
  } else if (message_namespace == kMediaNamespace) {
    HandleMediaMessage(sender_id, str_message);
  } else if (message_namespace != kSystemNamespace) {
    // System messages are ignored, log messages from unknown namespaces.
    DVLOG(2) << "Unknown message from " << sender_id
             << ", namespace=" << message_namespace
             << ", message=" << str_message;
  }

  return true;
}

void CastMessagePortImpl::OnPipeError() {
  DVLOG(3) << __func__;
  MaybeClose();
}

}  // namespace cast_streaming
