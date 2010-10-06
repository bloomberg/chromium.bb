// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_MULTIPLE_ARRAY_INPUT_STREAM_H_
#define REMOTING_BASE_MULTIPLE_ARRAY_INPUT_STREAM_H_

#include <vector>

#include "base/basictypes.h"
#include "google/protobuf/io/zero_copy_stream.h"

namespace remoting {

// A MultipleArrayInputStream provides a ZeroCopyInputStream with multiple
// backing arrays.
class MultipleArrayInputStream :
      public google::protobuf::io::ZeroCopyInputStream {
 public:
  // Construct a MultipleArrayInputStream with |count| backing arrays.
  // TODO(hclam): Consider adding block size to see if it has a performance
  // gain.
  MultipleArrayInputStream();
  virtual ~MultipleArrayInputStream();

  // Add a new buffer to the list.
  void AddBuffer(const char* buffer, int size);

  // google::protobuf::io::ZeroCopyInputStream interface.
  virtual bool Next(const void** data, int* size);
  virtual void BackUp(int count);
  virtual bool Skip(int count);
  virtual int64 ByteCount() const;

 private:
  std::vector<const char*> buffers_;
  std::vector<int> buffer_sizes_;

  size_t current_buffer_;
  int current_buffer_offset_;
  int position_;
  int last_returned_size_;

  DISALLOW_COPY_AND_ASSIGN(MultipleArrayInputStream);
};

}  // namespace remoting

#endif  // REMOTING_BASE_MULTIPLE_ARRAY_INPUT_STREAM_H_
