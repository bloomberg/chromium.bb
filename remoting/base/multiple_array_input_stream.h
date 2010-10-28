// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MultipleArrayInputStream implements ZeroCopyInputStream to be used by
// protobuf to decode bytes into a protocol buffer message.
//
// This input stream is made of multiple IOBuffers received from the network.
// This object retains the IOBuffers added to it.
//
// Internally, we wrap each added IOBuffer in a DrainableIOBuffer. This allows
// us to track how much data has been consumed from each IOBuffer.

#ifndef REMOTING_BASE_MULTIPLE_ARRAY_INPUT_STREAM_H_
#define REMOTING_BASE_MULTIPLE_ARRAY_INPUT_STREAM_H_

#include <vector>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "google/protobuf/io/zero_copy_stream.h"

namespace net {
class DrainableIOBuffer;
class IOBuffer;
}  // namespace net

namespace remoting {

class MultipleArrayInputStream :
      public google::protobuf::io::ZeroCopyInputStream {
 public:
  MultipleArrayInputStream();
  virtual ~MultipleArrayInputStream();

  // Add a buffer to the list. |buffer| is retained by this object.
  void AddBuffer(net::IOBuffer* buffer, int size);

  // google::protobuf::io::ZeroCopyInputStream interface.
  virtual bool Next(const void** data, int* size);
  virtual void BackUp(int count);
  virtual bool Skip(int count);
  virtual int64 ByteCount() const;

 private:
  std::vector<scoped_refptr<net::DrainableIOBuffer> > buffers_;

  size_t current_buffer_;
  int position_;
  int last_returned_size_;

  DISALLOW_COPY_AND_ASSIGN(MultipleArrayInputStream);
};

}  // namespace remoting

#endif  // REMOTING_BASE_MULTIPLE_ARRAY_INPUT_STREAM_H_
