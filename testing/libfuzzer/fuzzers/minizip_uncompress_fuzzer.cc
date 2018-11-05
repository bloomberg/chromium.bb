// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory.h>
#include <stddef.h>
#include <stdint.h>

#include "third_party/minizip/src/mz.h"
#include "third_party/minizip/src/mz_strm_mem.h"
#include "third_party/minizip/src/mz_zip.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  void* stream = mz_stream_mem_create(nullptr);
  mz_stream_mem_set_buffer(
      stream, reinterpret_cast<void*>(const_cast<uint8_t*>(data)), size);

  void* zip_file = mz_zip_create(nullptr);
  int result = mz_zip_open(zip_file, stream, MZ_OPEN_MODE_READ);
  if (result == MZ_OK) {
    result = mz_zip_goto_first_entry(zip_file);
    while (result == MZ_OK) {
      result = mz_zip_entry_read_open(zip_file, 0, nullptr);
      if (result != MZ_OK) {
        break;
      }

      mz_zip_file* file_info = nullptr;
      result = mz_zip_entry_get_info(zip_file, &file_info);
      if (result != MZ_OK) {
        break;
      }

      result = mz_zip_entry_close(zip_file);
      if (result != MZ_OK) {
        break;
      }

      result = mz_zip_goto_next_entry(zip_file);
    }
    mz_zip_close(zip_file);
  }

  mz_zip_delete(&zip_file);
  mz_stream_mem_delete(&stream);

  return 0;
}
