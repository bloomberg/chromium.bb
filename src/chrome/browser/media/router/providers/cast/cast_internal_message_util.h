// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_INTERNAL_MESSAGE_UTIL_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_INTERNAL_MESSAGE_UTIL_H_

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/values.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom.h"

namespace cast_channel {
class CastMessage;
}

namespace media_router {

class MediaSinkInternal;

// Represents a message sent or received by the Cast SDK via a
// PresentationConnection.
struct CastInternalMessage {
  // TODO(crbug.com/809249): Add other types of messages.
  enum class Type {
    kClientConnect,   // Initial message sent by SDK client to connect to MRP.
    kAppMessage,      // App messages to pass through between SDK client and the
                      // receiver.
    kReceiverAction,  // Message sent by MRP to inform SDK client of action.
    kNewSession,      // Message sent by MRP to inform SDK client of new
                      // session.
    kOther            // All other types of messages which are not considered
                      // part of communication with Cast SDK.
  };

  // Returns a CastInternalMessage for |message|, or nullptr is |message| is not
  // a valid Cast internal message.
  static std::unique_ptr<CastInternalMessage> From(base::Value message);

  CastInternalMessage(Type type, const std::string& client_id);
  ~CastInternalMessage();

  Type type;
  std::string client_id;
  int sequence_number = -1;

  // The following are set if |type| is |kAppMessage|.
  std::string app_message_namespace;
  std::string app_message_session_id;
  base::Value app_message_body;

  DISALLOW_COPY_AND_ASSIGN(CastInternalMessage);
};

// Represents a Cast session on a Cast device. Cast sessions are derived from
// RECEIVER_STATUS messages sent by Cast devices.
class CastSession {
 public:
  // Returns a CastSession from |receiver_status| message sent by |sink|, or
  // nullptr if |receiver_status| is not a valid RECEIVER_STATUS message.
  // |hash_token| is a per-profile value that is used to hash the sink ID.
  static std::unique_ptr<CastSession> From(const MediaSinkInternal& sink,
                                           const std::string& hash_token,
                                           const base::Value& receiver_status);

  // Returns a string that can be used as the description of the MediaRoute
  // associated with this session.
  static std::string GetRouteDescription(const CastSession& session);

  CastSession();
  ~CastSession();

  // ID of the session.
  std::string session_id;

  // ID of the app in the session.
  std::string app_id;

  // ID used for communicating with the session over the Cast channel.
  std::string transport_id;

  // The set of accepted message namespaces. Must be non-empty, unless the
  // session represents a multizone leader.
  base::flat_set<std::string> message_namespaces;

  // The human-readable name of the Cast application, for example, "YouTube".
  // Mandatory.
  std::string display_name;

  // Descriptive text for the current application content, for example “My
  // Wedding Slideshow”. May be empty.
  std::string status;

  // The dictionary representing this session, derived from |receiver_status|.
  // For convenience, this is used for generating messages sent to the SDK that
  // include the session value.
  base::Value value;
};

// Utility methods for generating messages sent to the SDK.
// |hash_token| is a per-profile value that is used to hash the sink ID.
blink::mojom::PresentationConnectionMessagePtr CreateReceiverActionCastMessage(
    const std::string& client_id,
    const MediaSinkInternal& sink,
    const std::string& hash_token);
blink::mojom::PresentationConnectionMessagePtr CreateReceiverActionStopMessage(
    const std::string& client_id,
    const MediaSinkInternal& sink,
    const std::string& hash_token);
blink::mojom::PresentationConnectionMessagePtr CreateNewSessionMessage(
    const CastSession& session,
    const std::string& client_id);
blink::mojom::PresentationConnectionMessagePtr CreateAppMessageAck(
    const std::string& client_id,
    int sequence_number);
blink::mojom::PresentationConnectionMessagePtr CreateAppMessage(
    const std::string& session_id,
    const std::string& client_id,
    const cast_channel::CastMessage& cast_message);

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_INTERNAL_MESSAGE_UTIL_H_
