// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_COMMON_MESSAGE_PORT_TRANSFERABLE_MESSAGE_STRUCT_TRAITS_H_
#define THIRD_PARTY_WEBKIT_COMMON_MESSAGE_PORT_TRANSFERABLE_MESSAGE_STRUCT_TRAITS_H_

#include "third_party/WebKit/common/message_port/cloneable_message_struct_traits.h"
#include "third_party/WebKit/common/message_port/message_port.mojom.h"
#include "third_party/WebKit/common/message_port/transferable_message.h"

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::TransferableMessage::DataView,
                 blink::TransferableMessage> {
  static blink::CloneableMessage& message(blink::TransferableMessage& input) {
    return input;
  }

  static std::vector<mojo::ScopedMessagePipeHandle> ports(
      blink::TransferableMessage& input) {
    return blink::MessagePortChannel::ReleaseHandles(input.ports);
  }

  static bool Read(blink::mojom::TransferableMessage::DataView data,
                   blink::TransferableMessage* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_WEBKIT_COMMON_MESSAGE_PORT_TRANSFERABLE_MESSAGE_STRUCT_TRAITS_H_
