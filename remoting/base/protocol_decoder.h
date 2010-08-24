// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_PROTOCOL_DECODER_H_
#define REMOTING_BASE_PROTOCOL_DECODER_H_

#include <deque>
#include <vector>

#include "base/ref_counted.h"
#include "google/protobuf/message_lite.h"
#include "media/base/data_buffer.h"
#include "remoting/base/protocol/chromotocol.pb.h"

namespace remoting {

typedef std::vector<ChromotingHostMessage*> HostMessageList;
typedef std::vector<ChromotingClientMessage*> ClientMessageList;

// A protocol decoder is used to decode data transmitted in the chromoting
// network.
// TODO(hclam): Defines the interface and implement methods.
class ProtocolDecoder {
 public:
  ProtocolDecoder();

  virtual ~ProtocolDecoder() {}

  // Parse data received from network into ClientMessages. Ownership of |data|
  // is passed to this object and output is written to |messages|.
  virtual void ParseClientMessages(scoped_refptr<media::DataBuffer> data,
                                   ClientMessageList* messages);

  // Parse data received from network into HostMessages. Ownership of |data|
  // is passed to this object and output is written to |messages|.
  virtual void ParseHostMessages(scoped_refptr<media::DataBuffer> data,
                                 HostMessageList* messages);

 private:
  // A private method used to parse data received from network into protocol
  // buffers.
  template <typename T>
  void ParseMessages(scoped_refptr<media::DataBuffer> data,
                     std::vector<T*>* messages);

  // Parse one message from |data_list_|. Return true if sucessful.
  template <typename T>
  bool ParseOneMessage(T** messages);

  // A utility method to read payload size of the protocol buffer from the
  // data list. Return false if we don't have enough data.
  bool GetPayloadSize(int* size);

  typedef std::deque<scoped_refptr<media::DataBuffer> > DataList;
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

#endif  // REMOTING_BASE_PROTOCOL_DECODER_H_
