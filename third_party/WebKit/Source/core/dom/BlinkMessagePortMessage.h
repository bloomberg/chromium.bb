// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BlinkMessagePortMessage_h
#define BlinkMessagePortMessage_h

#include "bindings/core/v8/serialization/SerializedScriptValue.h"
#include "third_party/WebKit/common/message_port/message_port_channel.h"

namespace blink {

// This struct represents messages as they are posted over a message port. This
// type can be serialized as a blink::mojom::MessagePortMessage struct.
// This is the renderer-side equivalent of blink::MessagePortMessage, where this
// struct uses blink types, while the other struct uses std:: types.
struct BlinkMessagePortMessage {
  BlinkMessagePortMessage();
  ~BlinkMessagePortMessage();

  RefPtr<blink::SerializedScriptValue> message;
  Vector<MessagePortChannel> ports;
};

}  // namespace blink

#endif  // BlinkMessagePortMessage_h
