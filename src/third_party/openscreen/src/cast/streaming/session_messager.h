// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_SESSION_MESSAGER_H_
#define CAST_STREAMING_SESSION_MESSAGER_H_

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "cast/common/public/message_port.h"
#include "json/value.h"

namespace openscreen {
namespace cast {

// A message port interface designed specifically for use by the Receiver
// and Sender session classes.
class SessionMessager : public MessagePort::Client {
 public:
  struct Message {
    // |sender_id| is always the other side of the message port connection:
    // the source if an incoming message, or the destination if an outgoing
    // message. The sender ID of this side of the message port is passed in
    // through the SessionMessager constructor.
    std::string sender_id = {};

    // The namespace of the message. Currently only kCastWebrtcNamespace
    // is supported--when new namespaces are added this class will have to be
    // updated.
    std::string message_namespace = {};

    // The sequence number of the message. This is important currently for
    // ensuring we reply to the proper request message, such as for OFFER/ANSWER
    // exchanges.
    int sequence_number = 0;

    // The body of the message, as a JSON object.
    Json::Value body;
  };

  using MessageCallback = std::function<void(Message)>;
  using ErrorCallback = std::function<void(Error)>;

  SessionMessager(MessagePort* message_port,
                  std::string source_id,
                  ErrorCallback cb);
  ~SessionMessager();

  // Set a message callback, such as OnOffer or OnAnswer.
  void SetHandler(std::string message_type, MessageCallback cb);

  // Send a JSON message.
  Error SendMessage(Message message);

  // MessagePort::Client overrides
  void OnMessage(const std::string& source_id,
                 const std::string& message_namespace,
                 const std::string& message) override;
  void OnError(Error error) override;

 private:
  // Since the number of message callbacks is expected to be low,
  // we use a vector of key, value pairs instead of a map.
  std::vector<std::pair<std::string, MessageCallback>> callbacks_;

  MessagePort* const message_port_;
  ErrorCallback error_callback_;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STREAMING_SESSION_MESSAGER_H_
