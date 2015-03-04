// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_INCOMING_ENDPOINT_H_
#define MOJO_EDK_SYSTEM_INCOMING_ENDPOINT_H_

#include <stddef.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "mojo/edk/system/channel_endpoint_client.h"
#include "mojo/edk/system/message_in_transit_queue.h"
#include "mojo/edk/system/system_impl_export.h"

struct MojoCreateDataPipeOptions;

namespace mojo {
namespace system {

class ChannelEndpoint;
class DataPipe;
class MessagePipe;

// This is a simple |ChannelEndpointClient| that only receives messages. It's
// used for endpoints that are "received" by |Channel|, but not yet turned into
// |MessagePipe|s or |DataPipe|s.
class MOJO_SYSTEM_IMPL_EXPORT IncomingEndpoint : public ChannelEndpointClient {
 public:
  IncomingEndpoint();

  // Must be called before any other method.
  scoped_refptr<ChannelEndpoint> Init();

  scoped_refptr<MessagePipe> ConvertToMessagePipe();
  scoped_refptr<DataPipe> ConvertToDataPipeProducer(
      const MojoCreateDataPipeOptions& validated_options,
      size_t consumer_num_bytes);
  scoped_refptr<DataPipe> ConvertToDataPipeConsumer(
      const MojoCreateDataPipeOptions& validated_options);

  // Must be called before destroying this object if |ConvertToMessagePipe()|
  // wasn't called (but |Init()| was).
  void Close();

  // |ChannelEndpointClient| methods:
  bool OnReadMessage(unsigned port, MessageInTransit* message) override;
  void OnDetachFromChannel(unsigned port) override;

 private:
  ~IncomingEndpoint() override;

  base::Lock lock_;  // Protects the following members.
  scoped_refptr<ChannelEndpoint> endpoint_;
  MessageInTransitQueue message_queue_;

  DISALLOW_COPY_AND_ASSIGN(IncomingEndpoint);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_INCOMING_ENDPOINT_H_
