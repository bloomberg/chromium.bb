// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_PEPPER_PORT_ALLOCATOR_H_
#define REMOTING_CLIENT_PLUGIN_PEPPER_PORT_ALLOCATOR_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ppapi/cpp/instance_handle.h"
#include "third_party/libjingle/source/talk/p2p/client/httpportallocator.h"

namespace remoting {

// An implementation of cricket::PortAllocator for libjingle that is
// used by the client plugin. There are two differences from
// cricket::HttpPortAllocator:
//   * PepperPortAllocator uses Pepper URLLoader API when creating
//     relay sessions.
//   * PepperPortAllocator resolves STUN DNS names and passes IP
//     addresses to BasicPortAllocator (it uses HostResolverPrivate API
//     for that). This is needed because libjingle's DNS resolution
//     code doesn't work in sandbox.
class PepperPortAllocator : public cricket::HttpPortAllocatorBase {
 public:
  static scoped_ptr<PepperPortAllocator> Create(
      const pp::InstanceHandle& instance);
  virtual ~PepperPortAllocator();

  // cricket::HttpPortAllocatorBase overrides.
  virtual cricket::PortAllocatorSession* CreateSession(
      const std::string& channel_name,
      int component) OVERRIDE;

 private:
  PepperPortAllocator(
      const pp::InstanceHandle& instance,
      scoped_ptr<talk_base::NetworkManager> network_manager,
      scoped_ptr<talk_base::PacketSocketFactory> socket_factory);

  pp::InstanceHandle instance_;
  scoped_ptr<talk_base::NetworkManager> network_manager_;
  scoped_ptr<talk_base::PacketSocketFactory> socket_factory_;

  DISALLOW_COPY_AND_ASSIGN(PepperPortAllocator);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_PEPPER_PORT_ALLOCATOR_H_
