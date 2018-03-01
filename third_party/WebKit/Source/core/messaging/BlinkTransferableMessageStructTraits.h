// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BlinkTransferableMessageStructTraits_h
#define BlinkTransferableMessageStructTraits_h

#include "bindings/core/v8/serialization/SerializedScriptValue.h"
#include "core/CoreExport.h"
#include "core/messaging/BlinkCloneableMessageStructTraits.h"
#include "core/messaging/BlinkTransferableMessage.h"
#include "mojo/public/cpp/bindings/array_traits_wtf_vector.h"
#include "skia/public/interfaces/bitmap_skbitmap_struct_traits.h"
#include "third_party/WebKit/public/common/message_port/message_port_channel.h"
#include "third_party/WebKit/public/mojom/message_port/message_port.mojom-blink.h"

namespace mojo {

template <>
struct CORE_EXPORT
    StructTraits<blink::mojom::blink::TransferableMessage::DataView,
                 blink::BlinkTransferableMessage> {
  static blink::BlinkCloneableMessage& message(
      blink::BlinkTransferableMessage& input) {
    return input;
  }

  static Vector<mojo::ScopedMessagePipeHandle> ports(
      blink::BlinkTransferableMessage& input) {
    Vector<mojo::ScopedMessagePipeHandle> result;
    result.ReserveInitialCapacity(input.ports.size());
    for (const auto& port : input.ports)
      result.push_back(port.ReleaseHandle());
    return result;
  }

  static const blink::SerializedScriptValue::ArrayBufferContentsArray&
  array_buffer_contents_array(const blink::BlinkCloneableMessage& input) {
    return input.message->GetArrayBufferContentsArray();
  }

  static Vector<SkBitmap> image_bitmap_contents_array(
      const blink::BlinkCloneableMessage& input);

  static bool has_user_gesture(const blink::BlinkTransferableMessage& input) {
    return input.has_user_gesture;
  }

  static bool Read(blink::mojom::blink::TransferableMessage::DataView,
                   blink::BlinkTransferableMessage* out);
};

template <>
class CORE_EXPORT
    StructTraits<blink::mojom::blink::SerializedArrayBufferContents::DataView,
                 WTF::ArrayBufferContents> {
 public:
  static base::span<uint8_t> contents(
      const WTF::ArrayBufferContents& array_buffer_contents) {
    uint8_t* allocation_start =
        static_cast<uint8_t*>(array_buffer_contents.Data());
    return base::make_span(allocation_start,
                           array_buffer_contents.DataLength());
  }
  static bool Read(blink::mojom::blink::SerializedArrayBufferContents::DataView,
                   WTF::ArrayBufferContents* out);
};

}  // namespace mojo

#endif  // BlinkTransferableMessageStructTraits_h
