// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_COMPRESSOR_VERBATIM_H_
#define REMOTING_BASE_COMPRESSOR_VERBATIM_H_

#include "base/compiler_specific.h"
#include "remoting/base/compressor.h"

namespace remoting {

// Compressor for verbatim streams.
class CompressorVerbatim : public Compressor {
 public:
  CompressorVerbatim();
  virtual ~CompressorVerbatim();

  // Compressor implementations.
  virtual bool Process(const uint8* input_data,
                       int input_size,
                       uint8* output_data,
                       int output_size,
                       CompressorFlush flush,
                       int* consumed,
                       int* written) OVERRIDE;

  virtual void Reset() OVERRIDE;
};

}  // namespace remoting

#endif  // REMOTING_BASE_COMPRESSOR_VERBATIM_H_
