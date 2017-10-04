// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BlinkMessagePortMessageStructTraits_h
#define BlinkMessagePortMessageStructTraits_h

#include "bindings/core/v8/serialization/SerializedScriptValue.h"
#include "core/dom/BlinkMessagePortMessage.h"
#include "third_party/WebKit/common/message_port/message_port.mojom-blink.h"
#include "third_party/WebKit/common/message_port/message_port_channel.h"

namespace mojo {

template <>
struct StructTraits<blink::mojom::blink::MessagePortMessage::DataView,
                    blink::BlinkMessagePortMessage> {
  static base::span<const uint8_t> encoded_message(
      blink::BlinkMessagePortMessage& input) {
    StringView wire_data = input.message->GetWireData();
    return base::make_span(
        reinterpret_cast<const uint8_t*>(wire_data.Characters8()),
        wire_data.length());
  }

  static std::vector<mojo::ScopedMessagePipeHandle> ports(
      blink::BlinkMessagePortMessage& input) {
    std::vector<blink::MessagePortChannel> channels(input.ports.begin(),
                                                    input.ports.end());
    return blink::MessagePortChannel::ReleaseHandles(std::move(channels));
  }

  static bool Read(blink::mojom::blink::MessagePortMessage::DataView,
                   blink::BlinkMessagePortMessage* out);
};

}  // namespace mojo

#endif  // BlinkMessagePortMessageStructTraits_h
