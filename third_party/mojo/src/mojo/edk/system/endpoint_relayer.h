// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_ENDPOINT_RELAYER_H_
#define MOJO_EDK_SYSTEM_ENDPOINT_RELAYER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "mojo/edk/system/channel_endpoint_client.h"
#include "mojo/edk/system/system_impl_export.h"

namespace mojo {
namespace system {

class ChannelEndpoint;

// This is a simple |ChannelEndpointClient| that just relays messages between
// two |ChannelEndpoint|s (without the overhead of |MessagePipe|).
class MOJO_SYSTEM_IMPL_EXPORT EndpointRelayer : public ChannelEndpointClient {
 public:
  EndpointRelayer();

  // Gets the other port number (i.e., 0 -> 1, 1 -> 0).
  static unsigned GetPeerPort(unsigned port);

  // Initialize this object. This must be called before any other method.
  void Init(ChannelEndpoint* endpoint0, ChannelEndpoint* endpoint1);

  // |ChannelEndpointClient| methods:
  bool OnReadMessage(unsigned port, MessageInTransit* message) override;
  void OnDetachFromChannel(unsigned port) override;

 private:
  ~EndpointRelayer() override;

  // TODO(vtl): We could probably get away without the lock if we had a
  // thread-safe |scoped_refptr|.
  base::Lock lock_;  // Protects the following members.
  scoped_refptr<ChannelEndpoint> endpoints_[2];

  DISALLOW_COPY_AND_ASSIGN(EndpointRelayer);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_ENDPOINT_RELAYER_H_
