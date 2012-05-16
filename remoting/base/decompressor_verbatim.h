// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_DECOMPRESSOR_VERBATIM_H_
#define REMOTING_BASE_DECOMPRESSOR_VERBATIM_H_

#include "base/compiler_specific.h"
#include "remoting/base/decompressor.h"

namespace remoting {

// A lossless decompressor using zlib.
class DecompressorVerbatim : public Decompressor {
 public:
  DecompressorVerbatim();
  virtual ~DecompressorVerbatim();

  virtual void Reset() OVERRIDE;

  // Decompressor implementations.
  virtual bool Process(const uint8* input_data, int input_size,
                       uint8* output_data, int output_size,
                       int* consumed, int* written) OVERRIDE;
};

}  // namespace remoting

#endif  // REMOTING_BASE_DECOMPRESSOR_VERBATIM_H_
