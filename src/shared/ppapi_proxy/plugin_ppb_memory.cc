// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_memory.h"

#include <cstdlib>

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/dev/ppb_memory_dev.h"

namespace ppapi_proxy {

namespace {

void* MemAlloc(uint32_t num_bytes) {
  DebugPrintf("PPB_Memory_Dev::MemAlloc(%"NACL_PRIu32")\n", num_bytes);
  return std::malloc(num_bytes);
}

void MemFree(void* ptr) {
  DebugPrintf("PPB_Memory_Dev::MemFree(%p)\n", ptr);
  std::free(ptr);
}

}  // namespace

const PPB_Memory_Dev* PluginMemory::GetInterface() {
  static const PPB_Memory_Dev memory_interface = {
    MemAlloc,
    MemFree
  };
  return &memory_interface;
}

}  // namespace ppapi_proxy
