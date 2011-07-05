// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_memory_impl.h"

#include <stdlib.h>
#include "ppapi/c/dev/ppb_memory_dev.h"

namespace webkit {
namespace ppapi {

namespace {

void* MemAlloc(uint32_t num_bytes) {
  return malloc(num_bytes);
}

void MemFree(void* ptr) {
  free(ptr);
}

const PPB_Memory_Dev ppb_memory = {
  &MemAlloc,
  &MemFree
};

}  // namespace

// static
const struct PPB_Memory_Dev* PPB_Memory_Impl::GetInterface() {
  return &ppb_memory;
}

}  // namespace ppapi
}  // namespace webkit

