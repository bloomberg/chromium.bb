// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_RPC_BROKER_H_
#define CAST_STREAMING_RPC_BROKER_H_

#include <vector>

#include "cast/streaming/remoting.pb.h"
#include "util/flat_map.h"

namespace openscreen {
namespace cast {

// Processes incoming and outgoing RPC messages and links them to desired
// components on both end points. For outgoing messages, the messager
// must send an RPC message with associated handle value. On the messagee side,
// the message is sent to a pre-registered component using that handle.
// Before RPC communication starts, both sides need to negotiate the handle
// value in the existing RPC communication channel using the special handles
// |kAcquire*Handle|.
//
// NOTE: RpcBroker doesn't actually send RPC messages to the remote. The session
// messager needs to set SendMessageCallback, and call ProcessMessageFromRemote
// as appropriate. The RpcBroker then distributes each RPC message to the
// subscribed component.
class RpcBroker {
 public:
  using Handle = int;
  using ReceiveMessageCallback = std::function<void(const RpcMessage&)>;
  using SendMessageCallback = std::function<void(std::vector<uint8_t>)>;

  explicit RpcBroker(SendMessageCallback send_message_cb);
  RpcBroker(const RpcBroker&) = delete;
  RpcBroker(RpcBroker&&) noexcept;
  RpcBroker& operator=(const RpcBroker&) = delete;
  RpcBroker& operator=(RpcBroker&&);
  ~RpcBroker();

  // Get unique handle value for RPC message handles.
  Handle GetUniqueHandle();

  // Register a component to receive messages via the given
  // ReceiveMessageCallback. |handle| is a unique handle value provided by a
  // prior call to GetUniqueHandle() and is used to reference the component in
  // the RPC messages. The receiver can then use it to direct an RPC message
  // back to a specific component.
  void RegisterMessageReceiverCallback(Handle handle,
                                       ReceiveMessageCallback callback);

  // Allows components to unregister in order to stop receiving message.
  void UnregisterMessageReceiverCallback(Handle handle);

  // Distributes an incoming RPC message to the registered (if any) component.
  void ProcessMessageFromRemote(const RpcMessage& message);

  // Executes the |send_message_cb_| using |message|.
  void SendMessageToRemote(const RpcMessage& message);

  // Checks if the handle is registered for receiving messages. Test-only.
  bool IsRegisteredForTesting(Handle handle);

  // Predefined invalid handle value for RPC message.
  static constexpr Handle kInvalidHandle = -1;

  // Predefined handle values for RPC messages related to initialization (before
  // the receiver handle(s) are known).
  static constexpr Handle kAcquireRendererHandle = 0;
  static constexpr Handle kAcquireDemuxerHandle = 1;

  // The first handle to return from GetUniqueHandle().
  static constexpr Handle kFirstHandle = 100;

 private:
  // Next unique handle to return from GetUniqueHandle().
  Handle next_handle_;

  // Maps of handle values to associated MessageReceivers.
  FlatMap<Handle, ReceiveMessageCallback> receive_callbacks_;

  // Callback that is ran to send a serialized message.
  SendMessageCallback send_message_cb_;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STREAMING_RPC_BROKER_H_
