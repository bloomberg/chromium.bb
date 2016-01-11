// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_PEPPER_PORT_ALLOCATOR_H_
#define REMOTING_CLIENT_PLUGIN_PEPPER_PORT_ALLOCATOR_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "ppapi/cpp/instance_handle.h"
#include "remoting/protocol/port_allocator_base.h"
#include "remoting/protocol/port_allocator_factory.h"

namespace remoting {

// An implementation of cricket::PortAllocator for libjingle that is used by the
// client plugin. It uses Pepper URLLoader API when creating relay sessions.
class PepperPortAllocator : public protocol::PortAllocatorBase {
 public:
  PepperPortAllocator(
      scoped_refptr<protocol::TransportContext> transport_context,
      pp::InstanceHandle pp_instance);
  ~PepperPortAllocator() override;

  pp::InstanceHandle pp_instance() { return pp_instance_; }

  // PortAllocatorBase overrides.
  cricket::PortAllocatorSession* CreateSessionInternal(
      const std::string& content_name,
      int component,
      const std::string& ice_username_fragment,
      const std::string& ice_password) override;

 private:
  pp::InstanceHandle pp_instance_;
  scoped_ptr<rtc::NetworkManager> network_manager_;
  scoped_ptr<rtc::PacketSocketFactory> socket_factory_;

  DISALLOW_COPY_AND_ASSIGN(PepperPortAllocator);
};

class PepperPortAllocatorFactory : public protocol::PortAllocatorFactory {
 public:
  PepperPortAllocatorFactory(pp::InstanceHandle pp_instance);
  ~PepperPortAllocatorFactory() override;

   // PortAllocatorFactory interface.
  scoped_ptr<cricket::PortAllocator> CreatePortAllocator(
      scoped_refptr<protocol::TransportContext> transport_context) override;

 private:
  pp::InstanceHandle pp_instance_;

  DISALLOW_COPY_AND_ASSIGN(PepperPortAllocatorFactory);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_PEPPER_PORT_ALLOCATOR_H_
