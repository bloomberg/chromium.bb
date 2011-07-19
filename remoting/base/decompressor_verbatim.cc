// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/decompressor_verbatim.h"

#include "base/logging.h"

namespace remoting {

DecompressorVerbatim::DecompressorVerbatim() {
}

DecompressorVerbatim::~DecompressorVerbatim() {
}

void DecompressorVerbatim::Reset() {
}

bool DecompressorVerbatim::Process(const uint8* input_data, int input_size,
                                   uint8* output_data, int output_size,
                                   int* consumed, int* written) {
  DCHECK_GT(output_size, 0);
  int bytes_to_copy = std::min(input_size, output_size);
  memcpy(output_data, input_data, bytes_to_copy);

  // Since we're just a memcpy, consumed and written are the same.
  *consumed = *written = bytes_to_copy;
  return true;
}
}  // namespace remoting
