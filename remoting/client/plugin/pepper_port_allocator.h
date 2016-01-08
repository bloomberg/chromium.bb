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
  static scoped_ptr<PepperPortAllocator> Create(
      const pp::InstanceHandle& instance);
  ~PepperPortAllocator() override;

  // PortAllocatorBase overrides.
  cricket::PortAllocatorSession* CreateSessionInternal(
      const std::string& content_name,
      int component,
      const std::string& ice_username_fragment,
      const std::string& ice_password) override;

 private:
  PepperPortAllocator(
      const pp::InstanceHandle& instance,
      scoped_ptr<rtc::NetworkManager> network_manager,
      scoped_ptr<rtc::PacketSocketFactory> socket_factory);

  pp::InstanceHandle instance_;
  scoped_ptr<rtc::NetworkManager> network_manager_;
  scoped_ptr<rtc::PacketSocketFactory> socket_factory_;

  DISALLOW_COPY_AND_ASSIGN(PepperPortAllocator);
};

class PepperPortAllocatorFactory : public protocol::PortAllocatorFactory {
 public:
  PepperPortAllocatorFactory(const pp::InstanceHandle& instance);
  ~PepperPortAllocatorFactory() override;

   // PortAllocatorFactory interface.
  scoped_ptr<protocol::PortAllocatorBase> CreatePortAllocator() override;

 private:
  pp::InstanceHandle instance_;

  DISALLOW_COPY_AND_ASSIGN(PepperPortAllocatorFactory);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_PEPPER_PORT_ALLOCATOR_H_
