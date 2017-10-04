// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/WebKit/common/message_port/message_port_message_struct_traits.h"

#include "base/containers/span.h"

namespace mojo {

bool StructTraits<blink::mojom::MessagePortMessage::DataView,
                  blink::MessagePortMessage>::
    Read(blink::mojom::MessagePortMessage::DataView data,
         blink::MessagePortMessage* out) {
  if (!data.ReadEncodedMessage(&out->owned_encoded_message) ||
      !data.ReadPorts(&out->ports))
    return false;

  out->encoded_message = out->owned_encoded_message;
  return true;
}

}  // namespace mojo
