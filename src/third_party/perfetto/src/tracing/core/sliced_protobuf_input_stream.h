/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_TRACING_CORE_SLICED_PROTOBUF_INPUT_STREAM_H_
#define SRC_TRACING_CORE_SLICED_PROTOBUF_INPUT_STREAM_H_

#include "perfetto/ext/tracing/core/slice.h"

#include <stdint.h>

#include <google/protobuf/io/zero_copy_stream.h>

namespace perfetto {

// Wraps a sequence of Slice(s) in a protobuf ZeroCopyInputStream that can be
// passed to protobuf::Message::ParseFromZeroCopyStream().
class SlicedProtobufInputStream
    : public google::protobuf::io::ZeroCopyInputStream {
 public:
  explicit SlicedProtobufInputStream(const Slices*);
  ~SlicedProtobufInputStream() override;

  // ZeroCopyInputStream implementation. See zero_copy_stream.h for the API
  // contract of the methods below.
  bool Next(const void** data, int* size) override;
  void BackUp(int count) override;
  bool Skip(int count) override;
  google::protobuf::int64 ByteCount() const override;

 private:
  bool Validate() const;

  const Slices* const slices_;
  Slices::const_iterator cur_slice_;
  size_t pos_in_cur_slice_ = 0;
};

}  // namespace perfetto

#endif  // SRC_TRACING_CORE_SLICED_PROTOBUF_INPUT_STREAM_H_
