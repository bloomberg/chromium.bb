// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_COMPRESSOR_H_
#define REMOTING_BASE_COMPRESSOR_H_

#include "base/basictypes.h"

namespace remoting {

// An object to compress data losslessly. Compressed data can be fully
// recovered by a Decompressor.
//
// Note that a Compressor can only be used on one stream during its
// lifetime. This object should be destroyed after use.
class Compressor {
 public:

  // Defines the flush modes for a compressor.
  enum CompressorFlush {
    CompressorNoFlush,
    CompressorSyncFlush,
    CompressorFinish,
  };
  virtual ~Compressor() {}

  // Resets all the internal state so the compressor behaves as if it
  // was just created.
  virtual void Reset() = 0;

  // Compress |input_data| with |input_size| bytes.
  //
  // |output_data| is provided by the caller and |output_size| is the
  // size of |output_data|. |output_size| must be greater than 0.
  //
  // |flush| is set to one of the three value:
  // - CompressorNoFlush
  //   No flushing is requested
  // - CompressorSyncFlush
  //   Write all pending output and write a synchronization point in the
  //   output data stream.
  // - CompressorFinish
  //   Mark the end of stream.
  //
  // Compressed data is written to |output_data|. |consumed| will
  // contain the number of bytes consumed from the input. |written|
  // contains the number of bytes written to output.
  //
  // Returns true if this method needs to be called again because
  // there is more data to be written out. This is particularly
  // useful for end of the compression stream.
  virtual bool Process(const uint8* input_data, int input_size,
                       uint8* output_data, int output_size,
                       CompressorFlush flush, int* consumed, int* written) = 0;
};

}  // namespace remoting

#endif  // REMOTING_BASE_COMPRESSOR_H_
