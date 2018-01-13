// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_COMMON_MESSAGE_PORT_CLONEABLE_MESSAGE_STRUCT_TRAITS_H_
#define THIRD_PARTY_WEBKIT_COMMON_MESSAGE_PORT_CLONEABLE_MESSAGE_STRUCT_TRAITS_H_

#include "third_party/WebKit/common/message_port/cloneable_message.h"
#include "third_party/WebKit/common/message_port/message_port.mojom.h"

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::CloneableMessage::DataView,
                 blink::CloneableMessage> {
  static base::span<const uint8_t> encoded_message(
      blink::CloneableMessage& input);

  static std::vector<blink::mojom::SerializedBlobPtr>& blobs(
      blink::CloneableMessage& input) {
    return input.blobs;
  }

  static uint64_t stack_trace_id(blink::CloneableMessage& input) {
    return input.stack_trace_id;
  }

  static int64_t stack_trace_debugger_id_first(blink::CloneableMessage& input) {
    return input.stack_trace_debugger_id_first;
  }

  static int64_t stack_trace_debugger_id_second(
      blink::CloneableMessage& input) {
    return input.stack_trace_debugger_id_second;
  }

  static bool Read(blink::mojom::CloneableMessage::DataView data,
                   blink::CloneableMessage* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_WEBKIT_COMMON_MESSAGE_PORT_CLONEABLE_MESSAGE_STRUCT_TRAITS_H_
