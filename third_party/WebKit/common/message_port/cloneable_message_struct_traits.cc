// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/WebKit/common/message_port/cloneable_message_struct_traits.h"

#include "base/containers/span.h"

namespace mojo {

base::span<const uint8_t> StructTraits<
    blink::mojom::CloneableMessage::DataView,
    blink::CloneableMessage>::encoded_message(blink::CloneableMessage& input) {
  return input.encoded_message;
}

bool StructTraits<blink::mojom::CloneableMessage::DataView,
                  blink::CloneableMessage>::
    Read(blink::mojom::CloneableMessage::DataView data,
         blink::CloneableMessage* out) {
  if (!data.ReadEncodedMessage(&out->owned_encoded_message))
    return false;

  out->encoded_message = out->owned_encoded_message;
  return true;
}

}  // namespace mojo
