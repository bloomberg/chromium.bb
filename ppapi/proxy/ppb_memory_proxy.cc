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

InterfaceProxy* CreateMemoryProxy(Dispatcher* dispatcher,
                                  const void* target_interface) {
  return new PPB_Memory_Proxy(dispatcher, target_interface);
}

}  // namespace

PPB_Memory_Proxy::PPB_Memory_Proxy(Dispatcher* dispatcher,
                                   const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_Memory_Proxy::~PPB_Memory_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_Memory_Proxy::GetInfo() {
  static const Info info = {
    &memory_dev_interface,
    PPB_MEMORY_DEV_INTERFACE,
    INTERFACE_ID_PPB_MEMORY,
    false,
    &CreateMemoryProxy,
  };
  return &info;
}

bool PPB_Memory_Proxy::OnMessageReceived(const IPC::Message& msg) {
  // All PPB_Memory_Dev calls are handled locally; there is no need to send or
  // receive messages here.
  return false;
}

}  // namespace proxy
}  // namespace ppapi
