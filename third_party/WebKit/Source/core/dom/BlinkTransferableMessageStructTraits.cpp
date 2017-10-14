// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/BlinkTransferableMessageStructTraits.h"

namespace mojo {

bool StructTraits<blink::mojom::blink::TransferableMessage::DataView,
                  blink::BlinkTransferableMessage>::
    Read(blink::mojom::blink::TransferableMessage::DataView data,
         blink::BlinkTransferableMessage* out) {
  std::vector<mojo::ScopedMessagePipeHandle> ports;
  if (!data.ReadMessage(static_cast<blink::BlinkCloneableMessage*>(out)) ||
      !data.ReadPorts(&ports)) {
    return false;
  }

  auto channels =
      blink::MessagePortChannel::CreateFromHandles(std::move(ports));
  out->ports.ReserveInitialCapacity(channels.size());
  out->ports.AppendRange(std::make_move_iterator(channels.begin()),
                         std::make_move_iterator(channels.end()));
  return true;
}

}  // namespace mojo
