// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/chromium_port_allocator_factory.h"

#include "remoting/protocol/chromium_socket_factory.h"
#include "remoting/protocol/port_allocator.h"
#include "remoting/protocol/transport_context.h"

namespace remoting {
namespace protocol {

ChromiumPortAllocatorFactory::ChromiumPortAllocatorFactory() {}
ChromiumPortAllocatorFactory::~ChromiumPortAllocatorFactory() {}

scoped_ptr<cricket::PortAllocator>
ChromiumPortAllocatorFactory::CreatePortAllocator(
    scoped_refptr<TransportContext> transport_context) {
  return make_scoped_ptr(new PortAllocator(
      make_scoped_ptr(new rtc::BasicNetworkManager()),
      make_scoped_ptr(new ChromiumPacketSocketFactory()), transport_context));
}

}  // namespace protocol
}  // namespace remoting
