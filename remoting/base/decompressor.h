// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_DECOMPRESSOR_H_
#define REMOTING_BASE_DECOMPRESSOR_H_

#include "base/basictypes.h"

namespace remoting {

// An object to decompress data losslessly. This is used in pair with
// a compressor.
//
// Note that a decompressor can only be used on one stream during its
// lifetime. This object should be destroyed after use.
class Decompressor {
 public:
  virtual ~Decompressor() {}

  // Resets all the internal state so the decompressor behaves as if it was
  // just created.
  virtual void Reset() = 0;

  // Decompress |input_data| with |input_size| bytes.
  //
  // |output_data| is provided by the caller and |output_size| is the
  // size of |output_data|. |output_size| must be greater than 0.
  //
  // Decompressed data is written to |output_data|. |consumed| will
  // contain the number of bytes consumed from the input. |written|
  // contains the number of bytes written to output.
  //
  // Returns true if this method needs to be called again because
  // there is more bytes to be decompressed or more input data is
  // needed.
  virtual bool Process(const uint8* input_data, int input_size,
                       uint8* output_data, int output_size,
                       int* consumed, int* written) = 0;
};

}  // namespace remoting

#endif  // REMOTING_BASE_DECOMPRESSOR_H_
