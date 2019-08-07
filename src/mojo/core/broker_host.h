// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_BROKER_HOST_H_
#define MOJO_CORE_BROKER_HOST_H_

#include <stdint.h>
#include <vector>

#include "base/macros.h"
#include "base/message_loop/message_loop_current.h"
#include "base/process/process_handle.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "mojo/core/channel.h"
#include "mojo/core/connection_params.h"
#include "mojo/core/embedder/process_error_callback.h"
#include "mojo/core/platform_handle_in_transit.h"
#include "mojo/core/scoped_process_handle.h"
#include "mojo/public/cpp/platform/platform_handle.h"

namespace mojo {
namespace core {

// The BrokerHost is a channel to a broker client process, servicing synchronous
// IPCs issued by the client.
class BrokerHost : public Channel::Delegate,
                   public base::MessageLoopCurrent::DestructionObserver {
 public:
  BrokerHost(base::ProcessHandle client_process,
             ConnectionParams connection_params,
             const ProcessErrorCallback& process_error_callback);

  // Send |handle| to the client, to be used to establish a NodeChannel to us.
  bool SendChannel(PlatformHandle handle);

#if defined(OS_WIN)
  // Sends a named channel to the client. Like above, but for named pipes.
  void SendNamedChannel(const base::StringPiece16& pipe_name);
#endif

 private:
  ~BrokerHost() override;

  bool PrepareHandlesForClient(std::vector<PlatformHandleInTransit>* handles);

  // Channel::Delegate:
  void OnChannelMessage(const void* payload,
                        size_t payload_size,
                        std::vector<PlatformHandle> handles) override;
  void OnChannelError(Channel::Error error) override;

  // base::MessageLoopCurrent::DestructionObserver:
  void WillDestroyCurrentMessageLoop() override;

  void OnBufferRequest(uint32_t num_bytes);

  const ProcessErrorCallback process_error_callback_;

#if defined(OS_WIN)
  ScopedProcessHandle client_process_;
#endif

  scoped_refptr<Channel> channel_;

  DISALLOW_COPY_AND_ASSIGN(BrokerHost);
};

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_BROKER_HOST_H_
