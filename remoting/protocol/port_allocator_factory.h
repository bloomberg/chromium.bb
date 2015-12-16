// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PORT_ALLOCATOR_FACTORY_H_
#define REMOTING_PROTOCOL_PORT_ALLOCATOR_FACTORY_H_

#include "base/memory/scoped_ptr.h"

namespace cricket {
class HttpPortAllocatorBase;
}  // namespace cricket

namespace remoting {
namespace protocol {

// Factory class used for creating cricket::PortAllocator that is used
// for ICE negotiation.
class PortAllocatorFactory {
 public:
  virtual scoped_ptr<cricket::HttpPortAllocatorBase> CreatePortAllocator() = 0;

  virtual ~PortAllocatorFactory() {}
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_PORT_ALLOCATOR_FACTORY_H_
