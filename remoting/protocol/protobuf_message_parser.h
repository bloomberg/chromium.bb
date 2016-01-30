// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PROTOBUF_MESSAGE_PARSER_H_
#define REMOTING_PROTOCOL_PROTOBUF_MESSAGE_PARSER_H_

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/protocol/message_reader.h"

namespace remoting {
namespace protocol {

// Version of MessageReader for protocol buffer messages, that parses
// each incoming message.
template <class T>
class ProtobufMessageParser {
 public:
  // The callback that is called when a new message is received. |done_task|
  // must be called by the callback when it's done processing the |message|.
  typedef typename base::Callback<void(scoped_ptr<T> message)>
      MessageReceivedCallback;

  // |message_reader| must outlive ProtobufMessageParser.
  ProtobufMessageParser(const MessageReceivedCallback& callback,
                        MessageReader* message_reader)
      : message_reader_(message_reader),
        message_received_callback_(callback) {
    message_reader->SetMessageReceivedCallback(base::Bind(
        &ProtobufMessageParser<T>::OnNewData, base::Unretained(this)));
  }
  ~ProtobufMessageParser() {
    message_reader_->SetMessageReceivedCallback(
        MessageReader::MessageReceivedCallback());
  }

 private:
  void OnNewData(scoped_ptr<CompoundBuffer> buffer) {
    scoped_ptr<T> message(new T());
    CompoundBufferInputStream stream(buffer.get());
    bool ret = message->ParseFromZeroCopyStream(&stream);
    if (!ret) {
      LOG(WARNING) << "Received message that is not a valid protocol buffer.";
    } else {
      DCHECK_EQ(stream.position(), buffer->total_bytes());
      message_received_callback_.Run(std::move(message));
    }
  }

  MessageReader* message_reader_;
  MessageReceivedCallback message_received_callback_;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_PROTOBUF_MESSAGE_PARSER_H_
