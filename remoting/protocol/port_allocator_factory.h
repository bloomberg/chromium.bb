// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PORT_ALLOCATOR_FACTORY_H_
#define REMOTING_PROTOCOL_PORT_ALLOCATOR_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace cricket {
class PortAllocator;
}  // namespace cricket

namespace remoting {
namespace protocol {

class TransportContext;

// Factory class used for creating cricket::PortAllocator that is used
// to allocate ICE candidates.
class PortAllocatorFactory {
 public:
  virtual ~PortAllocatorFactory() {}

  virtual scoped_ptr<cricket::PortAllocator> CreatePortAllocator(
      scoped_refptr<TransportContext> transport_context) = 0;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_PORT_ALLOCATOR_FACTORY_H_
