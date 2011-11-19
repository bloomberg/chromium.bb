// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_DECOMPRESSOR_ZLIB_H_
#define REMOTING_BASE_DECOMPRESSOR_ZLIB_H_

#include "base/memory/scoped_ptr.h"
#include "remoting/base/decompressor.h"

typedef struct z_stream_s z_stream;

namespace remoting {

// A lossless decompressor using zlib.
class DecompressorZlib : public Decompressor {
 public:
  DecompressorZlib();
  virtual ~DecompressorZlib();

  virtual void Reset() OVERRIDE;

  // Decompressor implementations.
  virtual bool Process(const uint8* input_data, int input_size,
                       uint8* output_data, int output_size,
                       int* consumed, int* written) OVERRIDE;

 private:
  void InitStream();
  scoped_ptr<z_stream> stream_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_DECOMPRESSOR_ZLIB_H_
