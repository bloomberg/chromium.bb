// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_STREAM_MESSAGE_PIPE_ADAPTER_H_
#define REMOTING_PROTOCOL_STREAM_MESSAGE_PIPE_ADAPTER_H_

#include "base/callback.h"
#include "remoting/protocol/message_channel_factory.h"
#include "remoting/protocol/message_pipe.h"
#include "remoting/protocol/message_reader.h"

namespace remoting {
class BufferedSocketWriter;

namespace protocol {

class P2PStreamSocket;
class StreamChannelFactory;

// MessagePipe implementation that sends and receives messages over a
// P2PStreamSocket.
class StreamMessagePipeAdapter : public MessagePipe {
 public:
  typedef base::Callback<void(int)> ErrorCallback;

  StreamMessagePipeAdapter(std::unique_ptr<P2PStreamSocket> socket,
                           const ErrorCallback& error_callback);
  ~StreamMessagePipeAdapter() override;

  // MessagePipe interface.
  void Start(EventHandler* event_handler) override;
  void Send(google::protobuf::MessageLite* message,
            base::OnceClosure done) override;

 private:
  void CloseOnError(int error);

  EventHandler* event_handler_ = nullptr;

  std::unique_ptr<P2PStreamSocket> socket_;
  ErrorCallback error_callback_;

  std::unique_ptr<MessageReader> reader_;
  std::unique_ptr<BufferedSocketWriter> writer_;

  DISALLOW_COPY_AND_ASSIGN(StreamMessagePipeAdapter);
};

class StreamMessageChannelFactoryAdapter : public MessageChannelFactory {
 public:
  typedef base::Callback<void(int)> ErrorCallback;

  StreamMessageChannelFactoryAdapter(
      StreamChannelFactory* stream_channel_factory,
      const ErrorCallback& error_callback);
  ~StreamMessageChannelFactoryAdapter() override;

  // MessageChannelFactory interface.
  void CreateChannel(const std::string& name,
                     const ChannelCreatedCallback& callback) override;
  void CancelChannelCreation(const std::string& name) override;

 private:
  void OnChannelCreated(const ChannelCreatedCallback& callback,
                        std::unique_ptr<P2PStreamSocket> socket);

  StreamChannelFactory* stream_channel_factory_;
  ErrorCallback error_callback_;

  DISALLOW_COPY_AND_ASSIGN(StreamMessageChannelFactoryAdapter);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_STREAM_MESSAGE_PIPE_ADAPTER_H_
