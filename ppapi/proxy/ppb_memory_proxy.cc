// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_memory_proxy.h"

#include "ppapi/c/dev/ppb_memory_dev.h"
#include "ppapi/proxy/plugin_var_tracker.h"

namespace ppapi {
namespace proxy {

namespace {

// PPB_Memory_Dev plugin -------------------------------------------------------

void* MemAlloc(uint32_t num_bytes) {
  return malloc(num_bytes);
}

void MemFree(void *ptr) {
  free(ptr);
}

const PPB_Memory_Dev memory_dev_interface = {
  &MemAlloc,
  &MemFree
};

}  // namespace

const PPB_Memory_Dev* GetPPB_Memory_Interface() {
  return &memory_dev_interface;
}

}  // namespace proxy
}  // namespace ppapi
