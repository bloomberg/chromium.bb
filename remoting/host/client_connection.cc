// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/client_connection.h"

#include "google/protobuf/message.h"
#include "media/base/data_buffer.h"
#include "remoting/base/protocol_decoder.h"
#include "remoting/base/protocol_util.h"

using media::DataBuffer;

namespace remoting {

// Determine how many update streams we should count to find the size of
// average update stream.
static const size_t kAverageUpdateStream = 10;

ClientConnection::ClientConnection(MessageLoop* message_loop,
                                   ProtocolDecoder* decoder,
                                   EventHandler* handler)
    : loop_(message_loop),
      decoder_(decoder),
      size_in_queue_(0),
      update_stream_size_(0),
      handler_(handler) {
  DCHECK(loop_);
  DCHECK(decoder_.get());
  DCHECK(handler_);
}

ClientConnection::~ClientConnection() {
  // TODO(hclam): When we shut down the viewer we may have to close the
  // jingle channel.
}

// static
scoped_refptr<media::DataBuffer>
    ClientConnection::CreateWireFormatDataBuffer(
        const HostMessage* msg) {
  // TODO(hclam): Instead of serializing |msg| create an DataBuffer
  // object that wraps around it.
  scoped_ptr<const HostMessage> message_deleter(msg);
  return SerializeAndFrameMessage(*msg);
}

void ClientConnection::SendInitClientMessage(int width, int height) {
  DCHECK_EQ(loop_, MessageLoop::current());
  DCHECK(!update_stream_size_);

  // If we are disconnected then return.
  if (!channel_)
    return;

  HostMessage msg;
  msg.mutable_init_client()->set_width(width);
  msg.mutable_init_client()->set_height(height);
  DCHECK(msg.IsInitialized());
  channel_->Write(SerializeAndFrameMessage(msg));
}

void ClientConnection::SendBeginUpdateStreamMessage() {
  DCHECK_EQ(loop_, MessageLoop::current());

  // If we are disconnected then return.
  if (!channel_)
    return;

  HostMessage msg;
  msg.mutable_begin_update_stream();
  DCHECK(msg.IsInitialized());

  scoped_refptr<DataBuffer> data = SerializeAndFrameMessage(msg);
  DCHECK(!update_stream_size_);
  update_stream_size_ += data->GetDataSize();
  channel_->Write(data);
}

void ClientConnection::SendUpdateStreamPacketMessage(
    scoped_refptr<DataBuffer> data) {
  DCHECK_EQ(loop_, MessageLoop::current());

  // If we are disconnected then return.
  if (!channel_)
    return;

  update_stream_size_ += data->GetDataSize();
  channel_->Write(data);
}

void ClientConnection::SendEndUpdateStreamMessage() {
  DCHECK_EQ(loop_, MessageLoop::current());

  // If we are disconnected then return.
  if (!channel_)
    return;

  HostMessage msg;
  msg.mutable_end_update_stream();
  DCHECK(msg.IsInitialized());

  scoped_refptr<DataBuffer> data = SerializeAndFrameMessage(msg);
  update_stream_size_ += data->GetDataSize();
  channel_->Write(data);

  // Here's some logic to help finding the average update stream size.
  size_in_queue_ += update_stream_size_;
  size_queue_.push_back(update_stream_size_);
  if (size_queue_.size() > kAverageUpdateStream) {
    size_in_queue_ -= size_queue_.front();
    size_queue_.pop_front();
    DCHECK_GE(size_in_queue_, 0);
  }
  update_stream_size_ = 0;
}

int ClientConnection::GetPendingUpdateStreamMessages() {
  DCHECK_EQ(loop_, MessageLoop::current());

  if (!size_queue_.size())
    return 0;
  int average_size = size_in_queue_ / size_queue_.size();
  if (!average_size)
    return 0;
  return channel_->write_buffer_size() / average_size;
}

void ClientConnection::Disconnect() {
  DCHECK_EQ(loop_, MessageLoop::current());

  // If there is a channel then close it and release the reference.
  if (channel_) {
    channel_->Close();
    channel_ = NULL;
  }
}

void ClientConnection::OnStateChange(JingleChannel* channel,
                                     JingleChannel::State state) {
  DCHECK(channel);
  loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &ClientConnection::StateChangeTask, state));
}

void ClientConnection::OnPacketReceived(JingleChannel* channel,
                                        scoped_refptr<DataBuffer> data) {
  DCHECK_EQ(channel_.get(), channel);
  loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &ClientConnection::PacketReceivedTask, data));
}

void ClientConnection::StateChangeTask(JingleChannel::State state) {
  DCHECK_EQ(loop_, MessageLoop::current());

  DCHECK(handler_);
  switch(state) {
    case JingleChannel::CONNECTING:
      break;
    // Don't care about this message.
    case JingleChannel::OPEN:
      handler_->OnConnectionOpened(this);
      break;
    case JingleChannel::CLOSED:
      handler_->OnConnectionClosed(this);
      break;
    case JingleChannel::FAILED:
      handler_->OnConnectionFailed(this);
      break;
    default:
      // We shouldn't receive other states.
      NOTREACHED();
  }
}

void ClientConnection::PacketReceivedTask(scoped_refptr<DataBuffer> data) {
  DCHECK_EQ(loop_, MessageLoop::current());

  // Use the decoder to parse incoming data.
  DCHECK(decoder_.get());
  ClientMessageList list;
  decoder_->ParseClientMessages(data, &list);

  // Then submit the messages to the handler.
  DCHECK(handler_);
  handler_->HandleMessages(this, &list);
}

}  // namespace remoting
