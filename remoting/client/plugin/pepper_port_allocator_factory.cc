// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_port_allocator_factory.h"

#include "base/memory/ptr_util.h"
#include "remoting/client/plugin/pepper_network_manager.h"
#include "remoting/client/plugin/pepper_packet_socket_factory.h"
#include "remoting/protocol/port_allocator.h"
#include "remoting/protocol/transport_context.h"

namespace remoting {

PepperPortAllocatorFactory::PepperPortAllocatorFactory(
    pp::InstanceHandle pp_instance)
    : pp_instance_(pp_instance) {}

PepperPortAllocatorFactory::~PepperPortAllocatorFactory() {}

std::unique_ptr<cricket::PortAllocator>
PepperPortAllocatorFactory::CreatePortAllocator(
    scoped_refptr<protocol::TransportContext> transport_context) {
  return base::MakeUnique<protocol::PortAllocator>(
      base::WrapUnique(new PepperNetworkManager(pp_instance_)),
      base::WrapUnique(new PepperPacketSocketFactory(pp_instance_)),
      transport_context);
}

}  // namespace remoting
