// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_MESSAGES_DECODER_H_
#define REMOTING_PROTOCOL_MESSAGES_DECODER_H_

#include <deque>
#include <list>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "net/base/io_buffer.h"
#include "remoting/base/compound_buffer.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace remoting {

class ChromotingClientMessage;
class ChromotingHostMessage;
class ClientControlMessage;
class ClientEventMessage;
class HostControlMessage;
class HostEventMessage;

// MessageDecoder uses CompoundBuffer to decode bytes into protocol
// buffer messages. This can be used to decode bytes received from the
// network.
//
// It provides ParseMessages() which accepts an IOBuffer. If enough bytes
// are collected to produce protocol buffer messages then the bytes will be
// consumed and the generated protocol buffer messages are added to the output
// list.
//
// It retains ownership of IOBuffer given to this object and keeps it alive
// until it is full consumed.
class MessageDecoder {
 public:
  MessageDecoder();
  virtual ~MessageDecoder();

  // Parses the bytes in |data| into a protobuf of type MessageType.  The bytes
  // in |data| are conceptually a stream of bytes to be parsed into a series of
  // protobufs. Each parsed protouf is appended into the |messages|. All calls
  // to ParseMessages should use same MessageType for any single instace of
  // MessageDecoder.

  // This function retains |data| until all its bytes are consumed.
  // Ownership of the produced protobufs is passed to the caller via the
  // |messages| list.
  template <class MessageType>
  void ParseMessages(scoped_refptr<net::IOBuffer> data,
                     int data_size, std::list<MessageType*>* messages) {
    AddBuffer(data, data_size);

    // Then try to parse the next message until we can't parse anymore.
    MessageType* message;
    while (ParseOneMessage<MessageType>(&message)) {
      messages->push_back(message);
    }
  }

  private:
  // Parse one message from |buffer_list_|. Return true if sucessful.
  template <class MessageType>
  bool ParseOneMessage(MessageType** message) {
    CompoundBuffer buffer;
    if (!GetNextMessageData(&buffer))
      return false;

    CompoundBufferInputStream stream(&buffer);
    *message = new MessageType();
    bool ret = (*message)->ParseFromZeroCopyStream(&stream);
    if (!ret)
      delete *message;
    return ret;
  }

  void AddBuffer(scoped_refptr<net::IOBuffer> data, int data_size);

  bool GetNextMessageData(CompoundBuffer* message_buffer);

  // Retrieves the read payload size of the current protocol buffer via |size|.
  // Returns false and leaves |size| unmodified, if we do not have enough data
  // to retrieve the current size.
  bool GetPayloadSize(int* size);

  CompoundBuffer buffer_;

  // |next_payload_| stores the size of the next payload if known.
  // |next_payload_known_| is true if the size of the next payload is known.
  // After one payload is read this is reset to false.
  int next_payload_;
  bool next_payload_known_;
};

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_MESSAGES_DECODER_H_
