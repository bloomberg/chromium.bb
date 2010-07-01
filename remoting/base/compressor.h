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
  virtual ~Compressor() {}

  // Compress |input_data| with |input_size| bytes.
  //
  // |output_data| is provided by the caller and |output_size| is the
  // size of |output_data|.
  //
  // Compressed data is written to |output_data|. |consumed| will
  // contain the number of bytes consumed from the input. |written|
  // contains the number of bytes written to output.
  virtual void Write(const uint8* input_data, int input_size,
                     uint8* output_data, int output_size,
                     int* consumed, int* written) = 0;

  // Flush all the remaining data in the compressor.
  //
  // |output_data| is provided by the caller and |output_size| is called
  // with the size of |output_data|.
  //
  // Output data is written to |output_data| and |written| contains the
  // number of bytes written.
  //
  // Returns true if |output_data| is not large enough to carry all
  // compressed data and caller needs to call Flush() again.
  virtual bool Flush(uint8* output_data, int output_size,
                     int* written) = 0;
};

}  // namespace remoting

#endif  // REMOTING_BASE_COMPRESSOR_H_
