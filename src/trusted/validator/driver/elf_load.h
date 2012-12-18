/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_DRIVER_ELF_LOAD_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_DRIVER_ELF_LOAD_H_

#include <vector>

#include "native_client/src/include/portability.h"


typedef std::vector<uint8_t> Image;

// Hypothetically reading whole ELF file to memory can cause problems with huge
// amounts of debug info, but unless it actually happens this approach is used
// for simplicity.
void ReadImage(const char *filename, Image *image);


struct Segment {
  const uint8_t *data;
  uint32_t size;
  uint32_t vaddr;
  int bitness;  // 32 or 64
};


Segment GetElfTextSegment(const std::vector<uint8_t> &image);

#endif
