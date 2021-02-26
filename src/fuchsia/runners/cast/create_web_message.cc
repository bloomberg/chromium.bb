// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/create_web_message.h"

#include "components/cast/message_port/message_port_fuchsia.h"
#include "fuchsia/base/mem_buffer_util.h"

fuchsia::web::WebMessage CreateWebMessage(
    base::StringPiece message,
    std::unique_ptr<cast_api_bindings::MessagePort> port) {
  fuchsia::web::WebMessage web_message;
  web_message.set_data(cr_fuchsia::MemBufferFromString(message, "msg"));
  if (port) {
    fuchsia::web::OutgoingTransferable outgoing_transferable;
    outgoing_transferable.set_message_port(
        cast_api_bindings::MessagePortFuchsia::FromMessagePort(port.get())
            ->TakeServiceRequest());
    std::vector<fuchsia::web::OutgoingTransferable> outgoing_transferables;
    outgoing_transferables.push_back(std::move(outgoing_transferable));
    web_message.set_outgoing_transfer(std::move(outgoing_transferables));
  }
  return web_message;
}
