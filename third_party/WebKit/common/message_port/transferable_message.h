// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_COMMON_MESSAGE_PORT_TRANSFERABLE_MESSAGE_H_
#define THIRD_PARTY_WEBKIT_COMMON_MESSAGE_PORT_TRANSFERABLE_MESSAGE_H_

#include <vector>

#include "base/containers/span.h"
#include "third_party/WebKit/common/common_export.h"
#include "third_party/WebKit/common/message_port/cloneable_message.h"
#include "third_party/WebKit/common/message_port/message_port_channel.h"

namespace blink {

// This struct represents messages as they are posted over a message port. This
// type can be serialized as a blink::mojom::TransferableMessage struct.
struct BLINK_COMMON_EXPORT TransferableMessage : public CloneableMessage {
  TransferableMessage();
  TransferableMessage(TransferableMessage&&);
  TransferableMessage& operator=(TransferableMessage&&);
  ~TransferableMessage();

  // Any ports being transfered as part of this message.
  std::vector<MessagePortChannel> ports;
};

}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_COMMON_MESSAGE_PORT_TRANSFERABLE_MESSAGE_H_
