// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BlinkCloneableMessageStructTraits_h
#define BlinkCloneableMessageStructTraits_h

#include "bindings/core/v8/serialization/SerializedScriptValue.h"
#include "core/dom/BlinkCloneableMessage.h"
#include "mojo/public/cpp/bindings/array_traits_wtf_vector.h"
#include "mojo/public/cpp/bindings/string_traits_wtf.h"
#include "third_party/WebKit/common/message_port/message_port.mojom-blink.h"

namespace mojo {

template <>
struct StructTraits<blink::mojom::blink::CloneableMessage::DataView,
                    blink::BlinkCloneableMessage> {
  static base::span<const uint8_t> encoded_message(
      blink::BlinkCloneableMessage& input) {
    StringView wire_data = input.message->GetWireData();
    return base::make_span(
        reinterpret_cast<const uint8_t*>(wire_data.Characters8()),
        wire_data.length());
  }

  static Vector<blink::mojom::blink::SerializedBlobPtr> blobs(
      blink::BlinkCloneableMessage& input);

  static bool Read(blink::mojom::blink::CloneableMessage::DataView,
                   blink::BlinkCloneableMessage* out);
};

}  // namespace mojo

#endif  // BlinkCloneableMessageStructTraits_h
