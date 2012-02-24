// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "remoting/base/compressor_verbatim.h"

namespace remoting {

CompressorVerbatim::CompressorVerbatim() {
}

CompressorVerbatim::~CompressorVerbatim() {
}

void CompressorVerbatim::Reset() {
}

bool CompressorVerbatim::Process(const uint8* input_data, int input_size,
                                 uint8* output_data, int output_size,
                                 CompressorFlush flush, int* consumed,
                                 int* written) {
  DCHECK_GT(output_size, 0);
  int bytes_to_copy = std::min(input_size, output_size);
  memcpy(output_data, input_data, bytes_to_copy);

  // Since we're just a memcpy, consumed and written are the same.
  *consumed = *written = bytes_to_copy;
  return (flush != CompressorFinish) || (output_size < bytes_to_copy);
}

}  // namespace remoting
