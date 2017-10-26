// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory.h>
#include <stddef.h>
#include <stdint.h>

#include "third_party/minizip/src/ioapi.h"
#include "third_party/minizip/src/ioapi_mem.h"
#include "third_party/minizip/src/unzip.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  zlib_filefunc_def filefunc32 = {0};
  ourmemory_t unzmem = {0};
  unzFile handle;
  unzmem.size = size;
  unzmem.base = (char*)data;
  unzmem.grow = 0;

  fill_memory_filefunc(&filefunc32, &unzmem);
  handle = unzOpen2(nullptr, &filefunc32);
  unzClose(handle);

  return 0;
}
