// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_MESSAGES_DECODER_H_
#define REMOTING_PROTOCOL_MESSAGES_DECODER_H_

#include <deque>
#include <list>

#include "base/ref_counted.h"
#include "google/protobuf/message_lite.h"
#include "remoting/proto/internal.pb.h"

namespace net {
class IOBuffer;
}

namespace remoting {

typedef std::list<ChromotingHostMessage*> HostMessageList;
typedef std::list<ChromotingClientMessage*> ClientMessageList;

// A protocol decoder is used to decode data transmitted in the chromoting
// network.
// TODO(hclam): Defines the interface and implement methods.
class MessagesDecoder {
 public:
  MessagesDecoder();
  virtual ~MessagesDecoder();

  // Parse data received from network into ClientMessages. Output is written
  // to |messages|.
  virtual void ParseClientMessages(scoped_refptr<net::IOBuffer> data,
                                   int data_size,
                                   ClientMessageList* messages);

  // Parse data received from network into HostMessages. Output is
  // written to |messages|.
  virtual void ParseHostMessages(scoped_refptr<net::IOBuffer> data,
                                 int data_size,
                                 HostMessageList* messages);

 private:
  // DataChunk stores reference to a net::IOBuffer and size of the data
  // stored in that buffer.
  struct DataChunk {
    DataChunk(net::IOBuffer* data, size_t data_size);
    ~DataChunk();

    scoped_refptr<net::IOBuffer> data;
    size_t data_size;
  };

  // TODO(sergeyu): It might be more efficient to memcopy data to one big buffer
  // instead of storing chunks in dqueue.
  typedef std::deque<DataChunk> DataList;

  // A private method used to parse data received from network into protocol
  // buffers.
  template <typename T>
  void ParseMessages(scoped_refptr<net::IOBuffer> data,
                     int data_size,
                     std::list<T*>* messages);

  // Parse one message from |data_list_|. Return true if sucessful.
  template <typename T>
  bool ParseOneMessage(T** messages);

  // A utility method to read payload size of the protocol buffer from the
  // data list. Return false if we don't have enough data.
  bool GetPayloadSize(int* size);

  DataList data_list_;
  size_t last_read_position_;

  // Count the number of bytes in |data_list_| not read.
  size_t available_bytes_;

  // Stores the size of the next payload if known.
  size_t next_payload_;

  // True if the size of the next payload is known. After one payload is read,
  // this is reset to false.
  bool next_payload_known_;
};

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_MESSAGES_DECODER_H_
